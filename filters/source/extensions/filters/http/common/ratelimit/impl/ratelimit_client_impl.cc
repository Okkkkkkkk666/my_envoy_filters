#include "ratelimit_client_impl.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RatelimitClient {
namespace Impl {

const std::string GrpcClientImpl::serivce_name_{
    "envoy.extensions.filters.http.common.ratelimit.v3.RateLimitService"};
const std::string GrpcClientImpl::limit_method_{"ShouldRateLimit"};
const std::string GrpcClientImpl::clean_method_{"CleanQuota"};
GrpcClientImpl::GrpcClientImpl(const Grpc::RawAsyncClientSharedPtr& async_client,
                               const absl::optional<std::chrono::milliseconds>& timeout)
    : client_(async_client), timeout_(timeout) {}

GrpcClientImpl::~GrpcClientImpl() { ASSERT(!limit_callbacks_); }

void GrpcClientImpl::cancel() {
  if (limit_callbacks_ != nullptr) {
    request_->cancel();
    limit_callbacks_ = nullptr;
  }
  if (clean_callbacks_ != nullptr) {
    request_->cancel();
    clean_callbacks_ = nullptr;
  }
}

void GrpcClientImpl::createRequest(
    v3::RateLimitRequest& request,
    const std::vector<std::shared_ptr<RateLimitPolicy>>& ratelimitpolicy) const {
  for (auto policy : ratelimitpolicy) {
    v3::RateLimitPolicy* new_pollicy = request.add_ratelimit_policys();
    new_pollicy->set_key(policy->key());
    new_pollicy->set_add_headers(policy->addHeaders());
    new_pollicy->set_block_time(policy->blockTime());
    new_pollicy->set_dryrun(policy->dryrun());
    new_pollicy->set_rule_name(policy->ruleName());
    const Impl::Quotas quotas = policy->quotas();
    for (int i = 0; i < 2; i++) {
      if (quotas[i].duration_ == 0 && quotas[i].max_count_ == 0) {
        break;
      }
      v3::Quota* new_quota = new_pollicy->add_quota();
      new_quota->set_duration(quotas[i].duration_);
      new_quota->set_max_count(quotas[i].max_count_);
    }
  }
}

void GrpcClientImpl::limit(
    Filters::Common::RatelimitClient::LimitRequestCallbacks& callbacks,
    const std::vector<std::shared_ptr<RateLimitPolicy>>& ratelimitpolicy,
    Tracing::Span& parent_span, const StreamInfo::StreamInfo& stream_info) {
  ASSERT(limit_callbacks_ == nullptr);
  limit_callbacks_ = &callbacks;
  v3::RateLimitRequest request;
  createRequest(request, ratelimitpolicy);
  request_ = client_->sendRaw(
      serivce_name_, limit_method_, Grpc::Common::serializeMessage(request), *this, parent_span,
      Http::AsyncClient::RequestOptions().setTimeout(timeout_).setParentContext(
          Http::AsyncClient::ParentContext{&stream_info}));
}

void GrpcClientImpl::clean(Filters::Common::RatelimitClient::CleanRequestCallbacks& callbacks,
                           const std::vector<std::string>& keys, Tracing::Span& parent_span,
                           const StreamInfo::StreamInfo& stream_info) {
  clean_callbacks_ = &callbacks;
  v3::CleanQuotaRequest request;
  for (const std::string& key : keys) {
    request.add_key_lists(key);
  }
  request_ = client_->sendRaw(
      serivce_name_, clean_method_, Grpc::Common::serializeMessage(request), *this, parent_span,
      Http::AsyncClient::RequestOptions().setTimeout(timeout_).setParentContext(
          Http::AsyncClient::ParentContext{&stream_info}));
}

void GrpcClientImpl::onSuccessRaw(Buffer::InstancePtr&& response, Tracing::Span&) {
  ENVOY_LOG(trace, "ratelimit service request success");
  request_ = nullptr;
  if (limit_callbacks_) {
    Buffer::ZeroCopyInputStreamImpl stream(std::move(response));
    LimitGrpcResponsePtr message = std::make_unique<v3::RateLimitResponse>();
    message->ParseFromZeroCopyStream(&stream);
    Filters::Common::RatelimitClient::LimitStatus status =
        Filters::Common::RatelimitClient::LimitStatus::OK;
    if (message->status() == v3::RateLimitResponse::OVER_LIMIT) {
      status = Filters::Common::RatelimitClient::LimitStatus::OverLimit;
    }
    limit_callbacks_->complete(status, std::move(message));
    limit_callbacks_ = nullptr;
  } else if (clean_callbacks_) {
    clean_callbacks_->complete(Filters::Common::RatelimitClient::CleanStatus::SUCCESS);
    clean_callbacks_ = nullptr;
  }
}

void GrpcClientImpl::onFailure(Grpc::Status::GrpcStatus status, const std::string& msg,
                               Tracing::Span&) {
  ENVOY_LOG_TO_LOGGER(Logger::Registry::getLog(Logger::Id::filter), debug,
                      "rate limit fail, status={} msg={}", status, msg);
  if (limit_callbacks_) {
    limit_callbacks_->complete(Filters::Common::RatelimitClient::LimitStatus::Error, nullptr);
    limit_callbacks_ = nullptr;
  } else if (clean_callbacks_) {
    clean_callbacks_->complete(Filters::Common::RatelimitClient::CleanStatus::FAILURE);
    clean_callbacks_ = nullptr;
  }
}

Filters::Common::RatelimitClient::ClientPtr
rateLimitClient(Server::Configuration::FactoryContext& context,
                const envoy::config::core::v3::GrpcService& grpc_service,
                const std::chrono::milliseconds timeout) {

  return std::make_unique<GrpcClientImpl>(
      context.clusterManager().grpcAsyncClientManager().getOrCreateRawAsyncClient(
          grpc_service, context.scope(), true, Grpc::CacheOption::CacheWhenRuntimeEnabled),
      timeout);
}

} // namespace Impl
} // namespace RatelimitClient
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
