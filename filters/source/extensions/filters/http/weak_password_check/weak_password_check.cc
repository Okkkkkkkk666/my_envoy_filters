#include "weak_password_check.h"
#include "filters/source/extensions/filters/http/common/utility/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {


using namespace Envoy::Extensions::Filters::Common::Utility;
const std::string Filter::filter_name_(FILTER_NAME);

FilterGlobalConfig::FilterGlobalConfig(const v3::WeakPasswordCheckGlobal& proto_config,
                                       Server::Configuration::FactoryContext&) {
  if (proto_config.auto_mode()) {
      config_ = std::make_shared<Impl::AutoConfig>(proto_config);
  } else {
      config_ = std::make_shared<Impl::AdvancedConfig>(proto_config);
  }
}

int Filter::threadIndex() {
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

bool Filter::weakPasswordProcess(Impl::AutoConfigPtr config) {
  for (auto& it : user_info_) {
    bool is_weak = false;
    bool is_plain = false;
    bool maybe_base64 = false;
    bool decode_base64_success = false;
    enum Impl::TrigRuleType trig_type = Impl::TrigNone;

    if (Impl::WpdUtility::isBase64(it.second)) {
      maybe_base64 = true;
      std::string decoded_password;
      if (Impl::WpdUtility::tryDecodeBase64(it.second, decoded_password)) {
        decode_base64_success = true;
        it.second = decoded_password;
      }
    }

    if (Impl::WpdUtility::isPasswordWeak(it.first, it.second)) {
      is_weak = true;
      trig_type = Impl::TrigInternal;
      is_plain = true;
    } else {
      enum Impl::PossibleEncryptType type = Impl::WpdUtility::getPossibleEncryptType(it.second);
      if (type == Impl::POSSIBLE_ENCRYPT_TYPE_UNKNOWN) {
        is_plain = true;
        std::string plain_password;
        if (config->queryPlainPassword(Impl::WpdUtility::MD5(it.second), Impl::POSSIBLE_ENCRYPT_TYPE_MD5_SHA256_SM3, plain_password, trig_type)) {
          is_weak = true;
        }
      } else {
        std::string lower_password(it.second);
        std::transform(lower_password.begin(), lower_password.end(), lower_password.begin(),
                       tolower);
        ENVOY_LOG(debug, "lower: {}", lower_password);
        // 密码与用户名或者用户名倒序的加密密文相同
        if (Impl::WpdUtility::isNameReverse(it.first, lower_password)) {
          is_weak = true;
          trig_type = Impl::TrigInternal;
        } else {
          if (it.second.size() != Impl::AutoConfig::Md5Length) {
            ENVOY_LOG(debug, "user: {}, strong password: {}", it.first, it.second);
          } else {
            std::string plain_password;
            if (config->queryPlainPassword(lower_password, type, plain_password, trig_type)) {
              is_weak = true;
            }
          }
        }
      }
    }

    // base64解码失败，非弱密码，同时又是base64格式（长度为4的倍数，仅由base64的64个基本字符组成）时
    // 认为密码为加密后使用base64编码的密码，此时应为非明文密码
    if (is_weak == false && maybe_base64 == true && decode_base64_success == false) {
      is_plain = false;
    }

    addLog(is_weak, is_plain, it.first, it.second, trig_type);
    return true;
  }
  return false;
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  // 判断是否存在网关级配置
  if (filter_hcm_config_ == nullptr) {
    deal_flag_ = false;
    return Http::FilterHeadersStatus::Continue;
  }

  content_type_ = Impl::getContentType(headers.getContentTypeValue());
  is_compressed_ = Impl::isContentCompressed(headers);

  // 获取path
  path_ = std::string(headers.getPathValue());
  const std::string_view path_view(path_);

  // 过滤.css .png等
  if (Impl::WpdUtility::isIgnoreResourcesRequest(path_view)) {
    deal_flag_ = false;
    return Http::FilterHeadersStatus::Continue;
  }

  Impl::AutoConfigPtr config = filter_hcm_config_->config();
  if (!config) {
    ENVOY_LOG(error, "no match rule");
    deal_flag_ = false;
    return Http::FilterHeadersStatus::Continue;
  }
  if (config->extractUserInfo(getUpstreamName(), path_view, Impl::CONTENT_TYPE_FROM_URLENCODED,
                              threadIndex(), user_info_)) {
    ENVOY_LOG(debug, "extract success: {}", path_view);
  }

  //TODO user password 在 cookie中
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  if (!deal_flag_ || user_info_.size()) {
    return Http::FilterDataStatus::Continue;
  }

  if (content_type_ == Impl::ContentType::CONTENT_TYPE_UNKNOWN) {
    deal_flag_ = false;
    return Http::FilterDataStatus::Continue;
  }
  if (is_compressed_) {
    deal_flag_ = false;
    return Http::FilterDataStatus::Continue;
  }
  Impl::AutoConfigPtr config = filter_hcm_config_->config();
  if (!config) {
    ENVOY_LOG(error, "no match rule");
    deal_flag_ = false;
    return Http::FilterDataStatus::Continue;
  }

  PROCESS_DECODER_BUFFER_LIMIT(is_decode_buffer_full_);

  const std::string& request_body = decoder_callbacks_->decodingBuffer()
                                        ? decoder_callbacks_->decodingBuffer()->toString()
                                        : data.toString();
  if (config->extractUserInfo(getUpstreamName(),
                              std::string_view(request_body.data(), request_body.size()),
                              content_type_, threadIndex(), user_info_)) {
    ENVOY_LOG(debug, "extract success: {}", request_body);
  }
  if (user_info_.empty()) {
    deal_flag_ = false;
  }

  return Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool) {
  if (!deal_flag_) {
    return Http::FilterHeadersStatus::Continue;
  }

  Impl::AutoConfigPtr config = filter_hcm_config_->config();
  if (!config) {
    ENVOY_LOG(error, "no match rule");
    deal_flag_ = false;
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
    absl::string_view set_cookie = result[i]->value().getStringView();
    if (config->isLoginSuccess(std::string_view(set_cookie.data(), set_cookie.size()), Impl::TokenRegexType::TOKEN_TYPE_COOKIE, threadIndex())) {
      ENVOY_LOG(debug, "set_cookie: {}", result[i]->value().getStringView());
      deal_flag_ = false;
      weakPasswordProcess(config);
      break;
    }
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool end_stream) {
  if (!deal_flag_ || is_compressed_ || (token_type_ == Impl::TokenRegexType::TOKEN_TYPE_UNKNOWN)) {
    return Http::FilterDataStatus::Continue;
  }
  Impl::AutoConfigPtr config = filter_hcm_config_->config();
  if (!config) {
    ENVOY_LOG(error, "no match rule");
    deal_flag_ = false;
    return Http::FilterDataStatus::Continue;
  }

  PROCESS_ENCODER_BUFFER_LIMIT(is_encode_buffer_full_);

  const std::string& response_body = encoder_callbacks_->encodingBuffer()
                                         ? encoder_callbacks_->encodingBuffer()->toString()
                                         : data.toString();
  if (config->isLoginSuccess(std::string_view(response_body.data(), response_body.size()),
                             token_type_, threadIndex())) {
    ENVOY_LOG(debug, "login: {}", response_body);
    deal_flag_ = false;
    weakPasswordProcess(config);
  }
  return Http::FilterDataStatus::Continue;
}

void Filter::addLog(bool is_weak, bool is_plain, const std::string& user_name,
                    const std::string& password, enum Impl::TrigRuleType type) {
  const auto& res_info = log_.add_result_info();
  res_info->set_is_weak_password(is_weak);
  res_info->set_is_plain_text(is_plain);
  res_info->set_user_name(user_name);
  std::string de_sensitive_pass(password);
  res_info->set_password(Impl::WpdUtility::deSensitive(de_sensitive_pass));
  res_info->set_trig_type(static_cast<v3::TrigRuleType>(type));
  res_info->set_path(path_);

  ENVOY_LOG(debug, "user_name: {}, password: {}, deSensitive: {}, is_weak: {}, is_plain: {}, trig_type: {}, path: {}", user_name, password, de_sensitive_pass, is_weak, is_plain, type, path_);
}

void Filter::onStreamComplete() {
  if (log_.result_info_size()) {
    log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true, true));
  }
}

inline const std::string& Filter::getUpstreamName() const {
  static const std::string empty;
  Upstream::ClusterInfoConstSharedPtr cluster =
      decoder_callbacks_->streamInfo().upstreamClusterInfo().has_value()
          ? decoder_callbacks_->streamInfo().upstreamClusterInfo().value()
          : nullptr;

  return cluster ? cluster->name() : empty;
}

inline const Network::Address::InstanceConstSharedPtr Filter::getDownstreamAddress() const {
  return decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
}

void Filter::onDestroy() {
}

} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy