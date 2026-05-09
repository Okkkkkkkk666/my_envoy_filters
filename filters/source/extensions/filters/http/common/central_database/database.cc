#include "database.h"

#include "source/common/grpc/common.h"
#include "envoy/tracing/http_tracer.h"
#include "source/common/tracing/null_span_impl.h"
#include "source/common/buffer/zero_copy_input_stream_impl.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace CentralDatabase {
const std::string Database::serivce_name_{
    "envoy.extensions.filters.http.common.central_database.v3.CentralDatabaseService"};
const std::string Database::insert_method_{"Insert"};
const std::string Database::del_method_{"Delete"};
const std::string Database::get_method_{"Get"};
const std::string Database::update_method_{"Update"};
const std::string Database::clean_method_{"Clean"};

Database::Database(Server::Configuration::FactoryContext& context,
                   const envoy::config::core::v3::GrpcService& grpc_service,
                   const std::chrono::milliseconds& timeout, const std::string& name_space)
    : namespace_(name_space), timeout_(timeout) {
  client_ = context.clusterManager().grpcAsyncClientManager().getOrCreateRawAsyncClient(
      grpc_service, context.scope(), true, Grpc::CacheOption::CacheWhenRuntimeEnabled);
}

void Database::insert(const std::string& /*key*/, const void* /*data*/, size_t /*size*/,
                      uint32_t /*ttl*/, bool /*force*/) {
  throw "not implemented!";
}

void Database::del(const std::string& /*key*/) { throw "not implemented!"; }

void Database::update(const std::string& /*key*/, const void* /*data*/, size_t /*size*/,
                      uint32_t /*ttl*/) {
  throw "not implemented!";
}

void Database::insertAsync(const std::string& key, InsertCallback cb, const void* data, size_t size,
                           uint32_t ttl, bool force) {
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

void Database::delAsync(const std::string& key, DelCallback cb) {
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

void Database::getAsync(const std::string& key, GetCallback cb, const void* data, size_t size,
                        uint32_t ttl) {
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

void Database::updateAsync(const std::string& key, UpdateCallback cb, const void* data, size_t size,
                           uint32_t ttl) {
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

void Database::cleanAsync(CleanCallback cb) {
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

void Database::cancel() {
  if (request_) {
    request_->cancel();
    request_ = nullptr;
  }

  state_ = State::Idle;
}

void Database::onSuccessRaw(Buffer::InstancePtr&& response, Tracing::Span& /*span*/) {
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

void Database::onFailure(Grpc::Status::GrpcStatus status, const std::string& message,
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
} // namespace CentralDatabase
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
