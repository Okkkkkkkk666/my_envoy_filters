#include "user_identify.h"
#include "source/common/http/path_utility.h"
#include "source/common/srhino_plugin_framework/v1_0_x/context_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {

using namespace Envoy::Extensions::Filters::Common::Utility;
const std::string UserIdentifyFilter::filter_name_(FILTER_NAME);

UserIdentifyFilterGlobalConfig::UserIdentifyFilterGlobalConfig(
    const v3::UserIdentifyGlobal& proto_config, Server::Configuration::FactoryContext&,
    std::string url_list, std::string user_name_list, std::string token_list,
    std::string guest_list)
    : data_interactive_(std::make_shared<Impl::DataInteractive>()) {

  if (proto_config.has_auto_mode()) {
    // init AutoIdentify
    identify_ = std::make_shared<Impl::Auto>(user_name_list, url_list, token_list, guest_list);
  } else {
    // init customize
    if (proto_config.has_customize()) {
      identify_ =
          std::make_shared<Impl::Customize>(proto_config.customize(), url_list, token_list, guest_list);
    }
  }
}

int UserIdentifyFilter::threadIndex() {
  if (thread_index_ != -1)
    return thread_index_;

  // worker_10
  std::string thread_name;
  if (encoder_callbacks_) {
    thread_name = encoder_callbacks_->dispatcher().name();
  } else if (decoder_callbacks_) {
    thread_name = decoder_callbacks_->dispatcher().name();
  } else {
    return -1;
  }

  auto pos = thread_name.find_first_of('_');
  if (pos == std::string::npos) {
    ENVOY_LOG(error, "Parse thread index failed: {}", thread_name);
    return -1;
  } else {
    thread_index_ = std::atol(thread_name.data() + pos + 1);
  }
  // ENVOY_LOG(trace, "thread index: {}", thread_index_);
  return thread_index_;
}

Http::FilterHeadersStatus UserIdentifyFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  // 判断是否存在网关级配置
  if (filter_hcm_config_ == nullptr || !deal_flag_) {
    return Http::FilterHeadersStatus::Continue;
  }

  IdentifyBasePtr identify = filter_hcm_config_->identify();
  if (!identify) {
    return Http::FilterHeadersStatus::Continue;
  }
  upstream_ = getUpstreamName();
  if (!ctx_) {
    ctx_ = std::make_shared<Impl::IdentifyCtx>();
    identify->initCtx(upstream_, threadIndex(), ctx_);
  }

  content_type_ = Impl::getContentType(headers.getContentTypeValue());
  if (content_type_ == Impl::CONTENT_TYPE_JSON) {
    token_type_ = Impl::TokenRegexType::TOKEN_TYPE_JSON;
  }
  is_compressed_ = Impl::isContentCompressed(headers);
  ENVOY_LOG(debug, "content_type:{}", content_type_);
  ENVOY_LOG(debug, "upstream_:{}", upstream_);

  absl::string_view request_path(headers.getPathValue());
  absl::string_view just_path(Http::PathUtil::removeQueryAndFragment(request_path));
  ENVOY_LOG(debug, "request_path:{}", request_path);
  ENVOY_LOG(debug, "just_path:{}", just_path);
  is_login_ = identify->isLogin(std::string_view(just_path.data(), just_path.size()), ctx_);
  if (is_login_) {
    // 处理登陆请求
    if (identify->getUserNameFromUrl(std::string_view(request_path.data(), request_path.size()),
                                     ctx_, user_name_)) {
      if (user_name_.size() > USER_NAME_MAX_LEN) {
        ENVOY_LOG(error, "user_name length: {} excced: {}", user_name_.size(), USER_NAME_MAX_LEN);
        deal_flag_ = false;
      }
    }
    return Http::FilterHeadersStatus::Continue;
  } else {
    // 处理访问请求
    // 从cookie中提取token
    Http::HeaderMap::GetResult result = headers.get(Http::Headers::get().Cookie);
    for (size_t i = 0; i < result.size(); ++i) {
      ENVOY_LOG(debug, "cookie: {}", result[i]->value().getStringView());
      absl::string_view cookie = result[i]->value().getStringView();
      if (identify->getTokenFromCookie(std::string_view(cookie.data(), cookie.size()), ctx_,
                                       is_login_ ? Impl::HttpType::HTTP_TYPE_LOGIN
                                                 : Impl::HttpType::HTTP_TYPE_GUEST,
                                       token_)) {
        ENVOY_LOG(debug, "get token success: {}", token_);
        token_key_init();
        deal_flag_ = false; // 处理完成

        bool need_sync = false;
        bool find = filter_hcm_config_->getDataInterative()->searchAndUpdateLocalCache(
            token_key_, GuestName, user_name_, kv_db_, need_sync, [&]() {
              decoder_callbacks_->continueDecoding(); // 恢复filter
              if (!user_name_.empty()) {
                ENVOY_LOG(debug, "grpc get user_name success: {}", user_name_);
              } else {
                ENVOY_LOG(warn, "grpc get user_name failed, token: {}", token_key_);
                // token找不到用户名，就当成访客处理
                user_name_ = GuestName;
              }
              storeLog();
              storeUserInfoToFilter(user_name_);
            });
        if (find) {
          storeLog();
          storeUserInfoToFilter(user_name_);
        } else {
          if (need_sync) {
            ENVOY_LOG(trace, "waiting for grpc");
            return Http::FilterHeadersStatus::StopAllIterationAndWatermark;
          } else {
            // 出错 kv_db_可能为nullptr
          }
        }

        return Http::FilterHeadersStatus::Continue;
      }
    }
  }
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus UserIdentifyFilter::decodeData(Buffer::Instance& data, bool end_stream) {
  if (!deal_flag_ || is_compressed_) {
    return Http::FilterDataStatus::Continue;
  }

  if (is_login_) {
    if (!user_name_.empty()) {
      // 已经获取到user_name
      return Http::FilterDataStatus::Continue;
    }
    if (content_type_ == Impl::ContentType::CONTENT_TYPE_UNKNOWN) {
      return Http::FilterDataStatus::Continue;
    }
  } else {
    if (!token_.empty()) {
      // 已经获取到token，不可能有这种情况
      ENVOY_LOG(error, "deal token:{} in {}", token_, __FUNCTION__);
      return Http::FilterDataStatus::Continue;
    }
    if (token_type_ == Impl::TokenRegexType::TOKEN_TYPE_UNKNOWN) {
      return Http::FilterDataStatus::Continue;
    }
  }
  IdentifyBasePtr identify = filter_hcm_config_->identify();
  if (!identify) {
    return Http::FilterDataStatus::Continue;
  }

  PROCESS_DECODER_BUFFER_LIMIT(is_decode_buffer_full_);

  const std::string& request_body = decoder_callbacks_->decodingBuffer()
                                        ? decoder_callbacks_->decodingBuffer()->toString()
                                        : data.toString();
  ENVOY_LOG(debug, "request body: {}", request_body);

  if (is_login_) {
    // 此时尝试从请求体中获取user_name
    if (identify->getUserNameFromBody(request_body, ctx_, content_type_, user_name_)) {
      if (user_name_.size() > USER_NAME_MAX_LEN) {
        ENVOY_LOG(error, "user_name length: {} excced: {}", user_name_.size(), USER_NAME_MAX_LEN);
        deal_flag_ = false;
      }
    }
  } else {
    // 此时尝试从请求体中获取token
    if (identify->getTokenFromBody(request_body, ctx_,
                                   is_login_ ? Impl::HttpType::HTTP_TYPE_LOGIN
                                             : Impl::HttpType::HTTP_TYPE_GUEST,
                                   token_type_, token_)) {
      ENVOY_LOG(debug, "get token success: {}", token_);
      token_key_init();
      deal_flag_ = false; // 处理完成

      bool need_sync = false;
      bool find = filter_hcm_config_->getDataInterative()->searchAndUpdateLocalCache(
          token_key_, GuestName, user_name_, kv_db_, need_sync, [&]() {
            decoder_callbacks_->continueDecoding(); // 恢复filter
            if (!user_name_.empty()) {
              ENVOY_LOG(debug, "grpc get user_name success: {}", user_name_);
            } else {
              ENVOY_LOG(warn, "grpc get user_name failed, token: {}", token_key_);
              // token找不到用户名，就当成访客处理
              user_name_ = GuestName;
            }
            storeLog();
            storeUserInfoToFilter(user_name_);
          });
      if (find) {
        storeLog();
        storeUserInfoToFilter(user_name_);
      } else {
        if (need_sync) {
          ENVOY_LOG(trace, "waiting for grpc");
          return Http::FilterDataStatus::StopIterationAndWatermark;
        } else {
          // 出错 kv_db_可能为nullptr
        }
      }

      return Http::FilterDataStatus::Continue;
    } else {
      // 如果从请求体中也找不到token，则放弃继续处理
      deal_flag_ = false; // 处理完成
    }
  }
  return Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus UserIdentifyFilter::encodeHeaders(Http::ResponseHeaderMap& headers,
                                                            bool) {
  if (!deal_flag_) {
    return Http::FilterHeadersStatus::Continue;
  }
  if (is_login_) {
    if (user_name_.empty()) { // 响应包都已经收到了，还没有找到用户名，则不继续处理
      deal_flag_ = false;
      return Http::FilterHeadersStatus::Continue;
    }
    if (!token_.empty()) { // 不可能出现这种情况
      deal_flag_ = false;
      ENVOY_LOG(error, "deal token:{} in {}", token_, __FUNCTION__);
      return Http::FilterHeadersStatus::Continue;
    }
  } else {
    // http响应不处理非登陆请求
    deal_flag_ = false;
    return Http::FilterHeadersStatus::Continue;
  }

  IdentifyBasePtr identify = filter_hcm_config_->identify();
  if (!identify) {
    return Http::FilterHeadersStatus::Continue;
  }

  content_type_ = Impl::getContentType(headers.getContentTypeValue());
  if (content_type_ == Impl::CONTENT_TYPE_JSON) {
    token_type_ = Impl::TokenRegexType::TOKEN_TYPE_JSON;
  }
  is_compressed_ = Impl::isContentCompressed(headers);
  // 尝试从header中获取token
  Http::HeaderMap::GetResult result = headers.get(Http::Headers::get().SetCookie);
  for (size_t i = 0; i < result.size(); ++i) {
    ENVOY_LOG(debug, "set_cookie: {}", result[i]->value().getStringView());
    absl::string_view set_cookie = result[i]->value().getStringView();
    if (identify->getTokenFromCookie(std::string_view(set_cookie.data(), set_cookie.size()), ctx_,
                                     is_login_ ? Impl::HttpType::HTTP_TYPE_LOGIN
                                               : Impl::HttpType::HTTP_TYPE_GUEST,
                                     token_)) {
      ENVOY_LOG(debug, "get token success: {}", token_);
      token_key_init();
      deal_flag_ = false;
      storeLog();
      storeUserInfoToFilter(user_name_);

      // 插入新的username和token到中央缓存和本地缓存
      bool need_sync = false;
      filter_hcm_config_->getDataInterative()->updateCache(
          token_key_, user_name_, kv_db_, need_sync,
          [&]() { encoder_callbacks_->continueEncoding(); });

      return need_sync ? Http::FilterHeadersStatus::StopAllIterationAndWatermark
                       : Http::FilterHeadersStatus::Continue;
    }
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus UserIdentifyFilter::encodeData(Buffer::Instance& data, bool end_stream) {
  if (!deal_flag_ || is_compressed_) {
    return Http::FilterDataStatus::Continue;
  }
  if (is_login_) {
    if (user_name_.empty()) { // 响应包都已经收到了，还没有找到用户名，则不继续处理
      deal_flag_ = false;
      return Http::FilterDataStatus::Continue;
    }
    if (!token_.empty()) { // 不可能出现这种情况
      deal_flag_ = false;
      ENVOY_LOG(error, "deal token:{} in {}", token_, __FUNCTION__);
      return Http::FilterDataStatus::Continue;
    }
    if (token_type_ == Impl::TokenRegexType::TOKEN_TYPE_UNKNOWN) {
      return Http::FilterDataStatus::Continue;
    }
  } else {
    // http响应不处理非登陆请求
    deal_flag_ = false;
    return Http::FilterDataStatus::Continue;
  }
  IdentifyBasePtr identify = filter_hcm_config_->identify();
  if (!identify) {
    return Http::FilterDataStatus::Continue;
  }

  PROCESS_ENCODER_BUFFER_LIMIT(is_encode_buffer_full_);

  const std::string& response_body = encoder_callbacks_->encodingBuffer()
                                         ? encoder_callbacks_->encodingBuffer()->toString()
                                         : data.toString();
  ENVOY_LOG(debug, "response body: {}", response_body);

  // 从响应消息体中获取token
  if (identify->getTokenFromBody(
          response_body, ctx_, is_login_ ? Impl::HttpType::HTTP_TYPE_LOGIN : Impl::HttpType::HTTP_TYPE_GUEST,
          token_type_, token_)) {
    ENVOY_LOG(debug, "get token success: {}", token_);
    token_key_init();
    deal_flag_ = false;
    storeLog();
    storeUserInfoToFilter(user_name_);

    // 插入新的username和token到中央缓存和本地缓存
    bool need_sync = false;
    filter_hcm_config_->getDataInterative()->updateCache(
        token_key_, user_name_, kv_db_, need_sync,
        [&]() { encoder_callbacks_->continueEncoding(); });

    return need_sync ? Http::FilterDataStatus::StopIterationAndWatermark
                     : Http::FilterDataStatus::Continue;
  } else {
    // 如果从响应体中也找不到token，则放弃继续处理
    deal_flag_ = false; // 处理完成
  }

  return Http::FilterDataStatus::Continue;
}

void UserIdentifyFilter::onDestroy() {
  if (kv_db_) {
    kv_db_->cancel();
  }
}

void UserIdentifyFilter::onStreamComplete() {
  if (!token_.empty() && !user_name_.empty()) {
    log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true));
  }
}

inline const Network::Address::InstanceConstSharedPtr
UserIdentifyFilter::getDownstreamAddress() const {
  return decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
}

inline const std::string& UserIdentifyFilter::getUpstreamName() const {
  static const std::string empty;
  Upstream::ClusterInfoConstSharedPtr cluster =
      decoder_callbacks_->streamInfo().upstreamClusterInfo().has_value()
          ? decoder_callbacks_->streamInfo().upstreamClusterInfo().value()
          : nullptr;
  return cluster ? cluster->name() : empty;
}

inline void UserIdentifyFilter::token_key_init() {
  if (!token_.empty()) {
    token_key_ = upstream_;
    token_key_.append(TokenKeySep);
    token_key_.append(token_);
  }
}

void UserIdentifyFilter::storeLog() {
  // 日志脱敏
  std::string show_token(token_);
  if (show_token.size() > 6) {
    show_token.replace(show_token.begin() + 3, show_token.end() - 3, 6, '*');
  }
  ENVOY_LOG(debug, "show_token:{}", show_token);

  auto result_info = log_.add_result_info();
  result_info->set_src_ip(getDownstreamAddress()->ip()->addressAsString());
  result_info->set_user(user_name_);
  result_info->set_token(show_token);
}

std::unordered_map<std::string, std::string>& UserIdentifyFilter::sharedData() {
  static const absl::string_view data_name("shared_data");

  Envoy::Http::StreamFilterCallbacks* stream_callbacks_(
      decoder_callbacks_ != nullptr
          ? static_cast<Envoy::Http::StreamFilterCallbacks*>(decoder_callbacks_)
          : static_cast<Envoy::Http::StreamFilterCallbacks*>(encoder_callbacks_));

  auto& filter_state = stream_callbacks_->streamInfo().filterState();
  if (!filter_state->hasDataWithName(data_name)) {
    filter_state->setData(data_name, std::make_shared<Envoy::StreamInfo::UserData>(),
                          Envoy::StreamInfo::FilterState::StateType::Mutable);
  }

  return filter_state->getDataMutable<Envoy::StreamInfo::UserData>(data_name);
}

// 将用户信息存入本地连接信息
void UserIdentifyFilter::storeUserInfoToFilter(const std::string& user_name) {
  auto &shared_data = sharedData();
  shared_data[SharedDataUserName] = user_name;
}

bool UserIdentifyFilter::getUserInfo(std::string& user_name) {
  auto &shared_data = sharedData();
  if (!shared_data.count(SharedDataUserName)) {
    ENVOY_LOG(trace, "not found");
    return false;
  }
  user_name = shared_data.at(SharedDataUserName);
  return true;
}

} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy