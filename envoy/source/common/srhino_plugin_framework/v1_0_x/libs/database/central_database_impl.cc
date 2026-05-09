#include <format>
#include "source/common/tracing/null_span_impl.h"
#include "source/common/grpc/common.h"
#include "source/common/buffer/zero_copy_input_stream_impl.h"
#include "envoy/upstream/cluster_manager.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/database/central_database_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/context_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Database {
using namespace Envoy;
const std::string CentralDatabaseImpl::serivce_name_{
    "envoy.extensions.filters.http.common.central_database.v3.CentralDatabaseService"};
const std::string CentralDatabaseImpl::insert_method_{"Insert"};
const std::string CentralDatabaseImpl::del_method_{"Delete"};
const std::string CentralDatabaseImpl::get_method_{"Get"};
const std::string CentralDatabaseImpl::update_method_{"Update"};
const std::string CentralDatabaseImpl::clean_method_{"Clean"};
const std::string CentralDatabaseImpl::cluster_name_{"cluster.central-database"};

CentralDatabaseImpl::CentralDatabaseImpl(
    Envoy::Server::Configuration::FactoryContext& factory_context,
    const std::chrono::milliseconds& timeout, const std::string& name_space)
    : namespace_(name_space), timeout_(timeout) {
  envoy::config::core::v3::GrpcService envoy_grpc;
  google::protobuf::Duration duration;
  duration.set_nanos(1000);
  *(envoy_grpc.mutable_timeout()) = duration;
  envoy_grpc.mutable_envoy_grpc()->set_cluster_name(cluster_name_);
  client_ = factory_context.clusterManager().grpcAsyncClientManager().getOrCreateRawAsyncClient(
      envoy_grpc, factory_context.scope(), true, Envoy::Grpc::CacheOption::CacheWhenRuntimeEnabled);
}

void CentralDatabaseImpl::insert(const std::string& /*key*/, const void* /*data*/, size_t /*size*/,
                                 uint32_t /*ttl*/, bool /*force*/) {
  throw "not implemented!";
}

void CentralDatabaseImpl::del(const std::string& /*key*/) { throw "not implemented!"; }

void CentralDatabaseImpl::update(const std::string& /*key*/, const void* /*data*/, size_t /*size*/,
                                 uint32_t /*ttl*/) {
  throw "not implemented!";
}

void CentralDatabaseImpl::insertAsync(const std::string& key, InsertCallback cb, const void* data,
                                      size_t size, uint32_t ttl, bool force) {
  ASSERT(state_ == State::Idle);
  if (state_ != State::Idle) {
    return;
  }

  insert_cb_ = cb;
  state_ = State::CallingInsert;

  insert_req_msg_.Clear();
  insert_req_msg_.set_namespace_(namespace_);
  insert_req_msg_.set_key(key);
  insert_req_msg_.set_value(data, size);
  insert_req_msg_.set_ttl(ttl);
  insert_req_msg_.set_force(force);

  request_ = client_->sendRaw(
      serivce_name_, insert_method_, Grpc::Common::serializeMessage(insert_req_msg_), *this,
      Tracing::NullSpan::instance(), Http::AsyncClient::RequestOptions().setTimeout(timeout_));
}

void CentralDatabaseImpl::delAsync(const std::string& key, DelCallback cb) {
  ASSERT(state_ == State::Idle);
  if (state_ != State::Idle) {
    return;
  }

  delete_cb_ = cb;
  state_ = State::CallingDelete;

  del_req_msg_.Clear();
  del_req_msg_.set_namespace_(namespace_);
  del_req_msg_.set_key(key);

  request_ = client_->sendRaw(
      serivce_name_, del_method_, Grpc::Common::serializeMessage(del_req_msg_), *this,
      Tracing::NullSpan::instance(), Http::AsyncClient::RequestOptions().setTimeout(timeout_));
}

void CentralDatabaseImpl::getAsync(const std::string& key, GetCallback cb, const void* data,
                                   size_t size, uint32_t ttl) {
  ASSERT(state_ == State::Idle);
  if (state_ != State::Idle) {
    return;
  }

  get_cb_ = cb;
  state_ = State::CallingGet;

  get_req_msg_.Clear();
  get_req_msg_.set_namespace_(namespace_);
  get_req_msg_.set_key(key);
  get_req_msg_.set_value(data, size);
  get_req_msg_.set_ttl(ttl);

  request_ = client_->sendRaw(
      serivce_name_, get_method_, Grpc::Common::serializeMessage(get_req_msg_), *this,
      Tracing::NullSpan::instance(), Http::AsyncClient::RequestOptions().setTimeout(timeout_));
}

void CentralDatabaseImpl::updateAsync(const std::string& key, UpdateCallback cb, const void* data,
                                      size_t size, uint32_t ttl) {
  ASSERT(state_ == State::Idle);
  if (state_ != State::Idle) {
    return;
  }

  update_cb_ = cb;
  state_ = State::CallingUpdate;

  update_req_msg_.Clear();
  update_req_msg_.set_namespace_(namespace_);
  update_req_msg_.set_key(key);
  update_req_msg_.set_value(data, size);
  update_req_msg_.set_ttl(ttl);

  request_ = client_->sendRaw(
      serivce_name_, update_method_, Grpc::Common::serializeMessage(update_req_msg_), *this,
      Tracing::NullSpan::instance(), Http::AsyncClient::RequestOptions().setTimeout(timeout_));
}

void CentralDatabaseImpl::cleanAsync(CleanCallback cb) {
  ASSERT(state_ == State::Idle);
  if (state_ != State::Idle) {
    return;
  }

  clean_cb_ = cb;
  state_ = State::CallingClean;

  clean_req_msg_.Clear();
  clean_req_msg_.set_namespace_(namespace_);

  request_ = client_->sendRaw(
      serivce_name_, clean_method_, Grpc::Common::serializeMessage(clean_req_msg_), *this,
      Tracing::NullSpan::instance(), Http::AsyncClient::RequestOptions().setTimeout(timeout_));
}

void CentralDatabaseImpl::cancel() {
  if (request_) {
    request_->cancel();
    request_ = nullptr;
  }

  state_ = State::Idle;
}

void CentralDatabaseImpl::onSuccessRaw(Buffer::InstancePtr&& response, Tracing::Span& /*span*/) {
  ENVOY_LOG(trace, "central database request success");
  request_ = nullptr;
  Buffer::ZeroCopyInputStreamImpl stream(std::move(response));
  switch (state_) {
  case State::CallingInsert:
    if (insert_cb_) {
      dbv3::InsertResponse message;
      message.ParseFromZeroCopyStream(&stream);
      insert_cb_(message.result() > 0, insert_req_msg_.key(), insert_req_msg_.value().c_str(),
                 insert_req_msg_.value().size());
    }
    break;
  case State::CallingDelete:
    if (delete_cb_) {
      dbv3::DeleteResponse message;
      message.ParseFromZeroCopyStream(&stream);
      delete_cb_(message.result() > 0, del_req_msg_.key());
    }
    break;
  case State::CallingGet:
    if (get_cb_) {
      dbv3::GetResponse message;
      message.ParseFromZeroCopyStream(&stream);

      // 插入成功
      if (message.result() == 2) {
        get_cb_(message.result(), get_req_msg_.key(), get_req_msg_.value().c_str(),
                get_req_msg_.value().size());
      } else {
        get_cb_(message.result(), get_req_msg_.key(), message.value().c_str(),
                message.value().size());
      }
    }
    break;
  case State::CallingUpdate:
    if (update_cb_) {
      dbv3::UpdateResponse message;
      message.ParseFromZeroCopyStream(&stream);
      update_cb_(message.result() > 0, update_req_msg_.key(), update_req_msg_.value().c_str(),
                 update_req_msg_.value().size());
    }
    break;
  case State::CallingClean:
    if (clean_cb_) {
      dbv3::CleanResponse message;
      message.ParseFromZeroCopyStream(&stream);
      clean_cb_(message.result() > 0);
    }
    break;
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }

  state_ = State::Idle;
}

void CentralDatabaseImpl::onFailure(Grpc::Status::GrpcStatus status, const std::string& message,
                                    Tracing::Span& /*span*/) {
  ENVOY_LOG(trace, "central database request failure:({}){}", status, message);
  request_ = nullptr;
  switch (state_) {
  case State::CallingInsert:
    if (insert_cb_) {
      insert_cb_(false, insert_req_msg_.key(), insert_req_msg_.value().c_str(),
                 insert_req_msg_.value().size());
    }
    break;
  case State::CallingDelete:
    if (delete_cb_) {
      delete_cb_(false, del_req_msg_.key());
    }
    break;
  case State::CallingGet:
    if (get_cb_) {
      get_cb_(0, get_req_msg_.key(), nullptr, 0);
    }
    break;
  case State::CallingUpdate:
    if (update_cb_) {
      update_cb_(false, update_req_msg_.key(), update_req_msg_.value().c_str(),
                 update_req_msg_.value().size());
    }
    break;
  case State::CallingClean:
    if (clean_cb_) {
      clean_cb_(false);
    }
    break;
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }

  state_ = State::Idle;
}

} // namespace Database
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework