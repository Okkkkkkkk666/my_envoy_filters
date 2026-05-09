#include "anti_cc.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AntiCC {

const Http::LowerCaseString AntiCCFilter::header_ratelimit_duration_("X-RateLimit-Duration");
const Http::LowerCaseString AntiCCFilter::header_ratelimit_remaining_("X-RateLimit-Remaining");
const Http::LowerCaseString AntiCCFilter::header_ratelimit_drop_("X-RateLimit-Limit");
const Http::LowerCaseString AntiCCFilter::header_verification_result_("Stone-Rhino-Code-Verify");
const std::string AntiCCFilter::header_path_get_html_("/api/v1/codeVerify/getPage");
const std::string AntiCCFilter::header_path_get_code_("/api/v1/codeVerify/getCode");
const std::string AntiCCFilter::header_path_code_verify_("/api/v1/codeVerify/checkCode");
const std::string AntiCCFilter::header_path_add_blacklist_(
    "/srhino/v1/antiddos/ddos-envoy/groups/anti-cc/ttl-rules-append");
const std::string AntiCCFilter::filter_name_(FILTER_NAME);
const std::string AntiCCFilter::src_ip_str_("src_ip");
const std::string AntiCCFilter::path_str_("path");
const std::string AntiCCFilter::ip_str_("ip");
const std::string AntiCCFilter::ttl_str_("ttl");
const std::string AntiCCFilter::type_str_("type");
const std::string AntiCCFilter::source_blacklist_str_("sb");
const std::string AntiCCFilter::src_and_dest_blacklist_str_("s_db");
const char* AntiCCFilter::data_str_("data");
const char* AntiCCFilter::value_str_("value");
const char* AntiCCFilter::mode_str_("mode");
const char* AntiCCFilter::result_str_("result");

AntiCCFilterGlobalConfig::AntiCCFilterGlobalConfig(
    const v3::AntiCCGlobal& proto_config, Server::Configuration::ServerFactoryContext& context)
    : cluster_manager_(context.clusterManager()),
      verification_server_name_(proto_config.verification_server().cluster_name()),
      polycube_server_name_(proto_config.polycube_server().cluster_name()),
      http_request_timeout_(
          PROTOBUF_GET_MS_OR_DEFAULT(proto_config.verification_server(), timeout, 1000)),
      mode_(proto_config.mode()), auto_level_(proto_config.auto_level()),
      action_(proto_config.mode() == v3::AUTO
                  ? std::make_shared<Impl::Action>(proto_config.action(), proto_config.auto_level())
                  : std::make_shared<Impl::Action>(proto_config.action())),
      ip_whitelist_([&proto_config]() {
        std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr> ip_whitelists;
        for (const auto& ip_whitelist : proto_config.ip_whitelist()) {
          ip_whitelists.push_back(
              std::make_unique<Filters::Common::IpWhitelist::IpWhitelist>(ip_whitelist));
        }
        return ip_whitelists;
      }()) {}

AntiCCFilter::AntiCCFilter(Server::Configuration::FactoryContext& context,
                           FilterGlobalConfigSharedPtr config,
                           Filters::Common::CentralDatabase::DatabasePtr&& blacklist_kv_db,
                           Filters::Common::CentralDatabase::DatabasePtr&& verify_status_kv_db,
                           Filters::Common::RatelimitClient::ClientPtr&& ratelimit_client)
    : Http::PassThroughFilterEx(context.getServerFactoryContext()), filter_hcm_config_(config),
      ratelimit_client_(std::move(ratelimit_client)), blacklist_kv_db_(std::move(blacklist_kv_db)),
      verify_status_kv_db_(std::move(verify_status_kv_db)) {}

Http::FilterHeadersStatus AntiCCFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  request_headers_ = &headers;

  // 逐条匹配ip白名单
  Network::Address::InstanceConstSharedPtr downstream_address = getDownstreamAddress();
  src_ip_and_host_ = downstream_address->ip()->addressAsString() + '_' + getLocalAddress();
  for (auto& ip_whitelist : filter_hcm_config_->ipWhitelist()) {
    if (ip_whitelist->enable() && ip_whitelist->match(downstream_address)) {
      ENVOY_LOG(trace, "This IP has hit the whitelist, ip={}",
                downstream_address->ip()->addressAsString());
      return Http::FilterHeadersStatus::Continue;
    }
  }

  // 判断是否为“刷新验证码”的URL
  if (!(request_headers_->get(Http::LowerCaseString(header_verification_result_)).empty())) {
    // 请求验证服务器,请求验证结果
    auto value = request_headers_->getByKey(header_verification_result_);
    if (value.has_value()) {
      ENVOY_LOG(trace, "Request verify server, type={}", value.value());
      if (value.value() == CODE_GET_CODE) {
        process_verify_quota_ = true;
        sendAsyncGrpcRateLimitRequest(process_verify_quota_);
      } else if (value.value() == CODE_VERIFY_CODE) {
        has_verification_result_header_ = true;
      }
      // 防止恶意构造携带Stone-Rhino-Code-Verify（!= 2或3）的请求
      else {
        state_ = State::Responded;
        decoder_callbacks_->sendLocalReply(Http::Code::Forbidden, "", nullptr, absl::nullopt, "");
      }
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  // 查找远程黑名单缓存
  getFromRemoteCache(src_ip_and_host_);

  return (state_ == State::NotStarted || state_ == State::Complete)
             ? Http::FilterHeadersStatus::Continue
             : Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus AntiCCFilter::decodeData(Buffer::Instance& data, bool end_stream) {
  ASSERT(state_ != State::Responded);
  // 判断是否为“校验验证码”的URL
  if (has_verification_result_header_) {
    if (end_stream) {
      decoder_callbacks_->addDecodedData(data, false);
      const std::string& request_body = decoder_callbacks_->decodingBuffer()->toString();
      std::string body;
      if (requestBodyAddSrcIp(request_body, body)) {
        request_ = sendAsyncHttpRequest(RequestMode::Verify, body);
        if (request_ == nullptr) {
          return Http::FilterDataStatus::Continue;
        }
      } else {
        // 人机验证结果响应体json解析失败
        decoder_callbacks_->sendLocalReply(Http::Code::Forbidden, "", nullptr, absl::nullopt,
                                           "json_parse_error");
      }
    }
  } else if (state_ == State::NotStarted || state_ == State::Complete) {
    return Http::FilterDataStatus::Continue;
  }
  return Http::FilterDataStatus::StopIterationAndWatermark;
}

void AntiCCFilter::getFromRemoteCache(const std::string& key) {
  if (blacklist_kv_db_) {
    state_ = State::CallingKv;
    ENVOY_LOG(trace, "Select src_ip from remote blacklist cache, src_ip={}", key);
    blacklist_kv_db_->getAsync(key, [&](int result, const std::string&, const void* value, size_t) {
      ENVOY_LOG(trace, "Select complete. result:{} src_ip:{}", result, key);
      getFromRemoteCacheCallback(result, key, value);
    });
  }
}

void AntiCCFilter::insertToRemoteCache(const std::string& key, const std::string& value,
                                       uint32_t ttl) {
  if (blacklist_kv_db_) {
    blacklist_kv_db_->insertAsync(key, nullptr, value.c_str(), value.size() + 1, ttl);
  }
}

void AntiCCFilter::getFromRemoteCacheCallback(int result, const std::string& key,
                                              const void* value) {
  state_ = State::Complete;

  if (result > 0) {
    ENVOY_LOG(debug, "This IP has hit the blacklist, ip={}", key);
    const char* duration = reinterpret_cast<const char*>(value);
    ASSERT(absl::SimpleAtoi(duration, &hit_duration_));
    // 请求DDos插件添加黑名单
    std::string body;
    requestBodyAddBlacklist(body);
    // 多节点情况，查询到此IP已被其他节点封禁，则此次封禁IP不进行上报可视化日志
    is_report_access_log_ = true;
    request_ = sendAsyncHttpRequest(RequestMode::BlockIp, body);
    return;
  }

  // 获取人机验证状态
  findVerifyStatus(src_ip_and_host_ + FIX_VERIFY, verify_status_kv_db_, [this](int result) {
    if ((result > 0) ^ (filter_hcm_config_->mode() == v3::EMERGENCY &&
                        filter_hcm_config_->action()->allRunVerification())) {
      // 请求验证服务器,请求验证界面
      std::string body;
      requestBodyAddPath(body);
      requestBodyAddSrcIp(body, body);
      request_ = sendAsyncHttpRequest(RequestMode::GetHtml, body);
    } else if (result <= 0) {
      sendAsyncGrpcRateLimitRequest(process_verify_quota_);
    } else {
      deleteVerifyStatus(src_ip_and_host_ + FIX_VERIFY, blacklist_kv_db_);
      decoder_callbacks_->continueDecoding();
    }
  });
}

inline const Network::Address::InstanceConstSharedPtr AntiCCFilter::getDownstreamAddress() const {
  return decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
}

const std::string& AntiCCFilter::getLocalAddress() const {
  return decoder_callbacks_->streamInfo().downstreamAddressProvider().localAddress()->asString();
}

bool AntiCCFilter::requestBodyAddJsonPair(std::string& request_body, const std::string& key,
                                          const std::string& value) {
  rapidjson::Document json_data(rapidjson::kObjectType);
  rapidjson::Value key_value(key.c_str(), json_data.GetAllocator());
  rapidjson::Value value_value(value.c_str(), json_data.GetAllocator());
  if (!(json_data.AddMember(key_value, value_value, json_data.GetAllocator()).IsObject())) {
    return false;
  }
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  json_data.Accept(writer);
  request_body = buffer.GetString();
  ENVOY_LOG(trace, "Add Json pair to the request body sent to the HTTP service, key={},value={}",
            key, value);
  return true;
}

void AntiCCFilter::findVerifyStatus(const std::string& src_ip,
                                    Filters::Common::CentralDatabase::DatabasePtr& kv_db,
                                    std::function<void(int)> cb) {
  if (kv_db) {
    ENVOY_LOG(trace, "Select src_ip from remote cache, src_ip={}", src_ip);
    kv_db->getAsync(src_ip, [&, cb](int result, const std::string& key, const void*, size_t) {
      ENVOY_LOG(trace, "Select complete. result:{} src_ip:{}", result, key);
      if (cb) {
        cb(result);
      }
    });
  }
}

void AntiCCFilter::insertVerifyStatus(const std::string& src_ip,
                                      Filters::Common::CentralDatabase::DatabasePtr& kv_db) {
  if (kv_db) {
    ENVOY_LOG(trace, "insert src_ip into remote cache, src_ip={}", src_ip);
    std::string value("");
    kv_db->insertAsync(src_ip, nullptr, &value, sizeof(value));
  }
}

void AntiCCFilter::deleteVerifyStatus(const std::string& src_ip,
                                      Filters::Common::CentralDatabase::DatabasePtr& kv_db) {
  if (kv_db) {
    ENVOY_LOG(trace, "delete src_ip from remote cache, src_ip={}", src_ip);
    kv_db->delAsync(src_ip, nullptr);
  }
}

template <typename ValueType>
bool AntiCCFilter::requestBodyAddJsonPair(const std::string& old_body, std::string& new_body,
                                          const std::string& key, const ValueType& value) {
  rapidjson::Document json_data;
  if (!json_data.Parse(old_body.c_str()).HasParseError()) {
    rapidjson::Value key_value(key.c_str(), json_data.GetAllocator());
    if constexpr (std::is_same_v<ValueType, std::string>) {
      rapidjson::Value value_value(value.c_str(), json_data.GetAllocator());
      json_data.AddMember(key_value, value_value, json_data.GetAllocator());
    } else {
      rapidjson::Value value_value(value);
      json_data.AddMember(key_value, value_value, json_data.GetAllocator());
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json_data.Accept(writer);
    new_body = buffer.GetString();
    ENVOY_LOG(trace, "Add Json pair to the request body sent to the HTTP service, key={},value={}",
              key, value);
    return true;
  }
  return false;
}

bool AntiCCFilter::requestBodyAddSrcIp(std::string& request_body) {
  return requestBodyAddJsonPair(request_body, src_ip_str_, src_ip_and_host_);
}

bool AntiCCFilter::requestBodyAddSrcIp(const std::string& old_body, std::string& new_body) {
  return requestBodyAddJsonPair(old_body, new_body, src_ip_str_, src_ip_and_host_);
}

bool AntiCCFilter::requestBodyAddPath(std::string& request_body) {
  return requestBodyAddJsonPair(request_body, path_str_,
                                std::string(request_headers_->getPathValue()));
}

bool AntiCCFilter::requestBodyAddBlacklist(std::string& request_body) {
  return requestBodyAddJsonPair(request_body, ip_str_, src_ip_and_host_) &&
         requestBodyAddJsonPair(request_body, request_body, ttl_str_,
                                filter_hcm_config_->action()->blockTime()) &&
         requestBodyAddJsonPair(request_body, request_body, type_str_, source_blacklist_str_);
}

Http::AsyncClient::Request* AntiCCFilter::sendAsyncHttpRequest(RequestMode mode,
                                                               const std::string& body) {
  Envoy::Upstream::ThreadLocalCluster* cluster = nullptr;
  if (mode == RequestMode::BlockIp) {
    ENVOY_LOG(trace, "Requesting PolyCube service DDos filter to add blacklist");
    cluster = filter_hcm_config_->getPolyCubeCluster();
    is_add_blacklist_request_ = true;
  } else {
    cluster = filter_hcm_config_->getVerifyCluster();
  }
  if (cluster == nullptr) {
    return nullptr;
  }

  Http::RequestMessagePtr request(new Http::RequestMessageImpl(
      Http::createHeaderMap<Http::RequestHeaderMapImpl>(*request_headers_)));
  switch (mode) {
  case RequestMode::GetHtml:
    request->headers().setPath(header_path_get_html_);
    request->headers().setMethod(Http::Headers::get().MethodValues.Post);
    request->headers().setCopy(Http::LowerCaseString("Stone-Label"), header_verification_result_);
    break;
  case RequestMode::GetCode:
    request->headers().setPath(header_path_get_code_);
    request->headers().setMethod(Http::Headers::get().MethodValues.Post);
    request->headers().setCopy(header_verification_result_, CODE_GET_CODE);
    break;
  case RequestMode::Verify:
    request->headers().setPath(header_path_code_verify_);
    request->headers().setMethod(Http::Headers::get().MethodValues.Post);
    request->headers().setCopy(header_verification_result_, CODE_VERIFY_CODE);
    break;
  case RequestMode::BlockIp:
    request->headers().setPath(header_path_add_blacklist_);
    request->headers().setMethod(Http::Headers::get().MethodValues.Post);
    break;
  }
  request->headers().setContentType("application/json;charset=UTF-8");
  request->headers().setContentLength(body.size());

  request->body().add(body);
  state_ = State::CallingHttp;
  ENVOY_LOG(debug, "requestBody={}", request->bodyAsString());
  return cluster->httpAsyncClient().send(
      std::move(request), *this,
      Http::AsyncClient::RequestOptions().setTimeout(filter_hcm_config_->httpRequestTimeout()));
}

void AntiCCFilter::sendAsyncGrpcRateLimitRequest(bool is_verify) {
  Filters::Common::RatelimitClient::Impl::Quotas quotas{
      filter_hcm_config_->action()->verifyQuota()};
  ratelimit_policys_.emplace_back(
      std::make_shared<Filters::Common::RatelimitClient::Impl::RateLimitPolicy>(
          is_verify ? src_ip_and_host_ + FIX_VERIFY : src_ip_and_host_,
          is_verify ? quotas : filter_hcm_config_->action()->rateLimitQuota(),
          filter_hcm_config_->action()->blockTime()));
  state_ = State::CallingLimit;
  ENVOY_LOG(trace, "Request Ratelimit service to limit, key={}",
            is_verify ? src_ip_and_host_ + FIX_VERIFY : src_ip_and_host_);
  ratelimit_client_->limit(*this, ratelimit_policys_, decoder_callbacks_->activeSpan(),
                           decoder_callbacks_->streamInfo());
}

void AntiCCFilter::sendAsyncGrpcCleanQuotaRequest(const std::string& src_ip) {
  state_ = State::CallingLimit;
  std::vector<std::string> keys;
  keys.emplace_back(src_ip);              // 访问频率的key
  keys.emplace_back(src_ip + FIX_VERIFY); // 人机验证频率的key
  ENVOY_LOG(trace, "Request Ratelimit service to clean quota, key={}", src_ip);
  ratelimit_client_->clean(*this, keys, decoder_callbacks_->activeSpan(),
                           decoder_callbacks_->streamInfo());
}

void AntiCCFilter::onSuccess(const Http::AsyncClient::Request&,
                             Http::ResponseMessagePtr&& response) {
  request_ = nullptr;
  state_ = State::Responded;
  // 判断是否为新增ddos黑名单请求cb
  ENVOY_LOG(trace, "Http response body={}", response->bodyAsString());

  // response body 校验
  rapidjson::Document json_data;
  if (!json_data.Parse(response->bodyAsString().c_str()).HasParseError()) {
    if (json_data.HasMember(result_str_)) {
      // PolyCube响应体json结构校验
      const rapidjson::Value& result = json_data[result_str_];
      if (result.IsBool()) {
        bool result_str = result.GetBool();
        if (result_str == true) {
          ENVOY_LOG(trace, "PolyCube service HTTP request completed");

          // 判断封禁IP的日志是否已上报
          if (!is_report_access_log_) {
            // 可视化日志
            if (process_verify_quota_) {
              setLog(v3::Act::VERIFY_FAILURE);
            } else {
              setLog(v3::Act::REFUSE, hit_duration_);
            }
          }

          // 本地回复429
          decoder_callbacks_->sendLocalReply(Http::Code::TooManyRequests, "", nullptr,
                                             absl::nullopt, "request_rate_limited");
          return;
        }
      }
    } else if (json_data.HasMember(data_str_)) {

      // 人机验证响应体json结构校验
      const rapidjson::Value& data = json_data[data_str_];
      if (data.HasMember(mode_str_) && data.HasMember(value_str_)) {
        const rapidjson::Value& mode = data[mode_str_];
        const rapidjson::Value& value = data[value_str_];
        if (mode.IsString() && value.IsString()) {
          std::string mode_str = mode.GetString();
          std::string value_str = value.GetString();
          ENVOY_LOG(trace, "Verify service HTTP request completed");
          if (value_str == VERIFY_RESULT_SUCCESS) {
            ENVOY_LOG(trace, "Human-machine verification passed");
            if (filter_hcm_config_->mode() == v3::EMERGENCY &&
                filter_hcm_config_->action()->allRunVerification()) {

              // 紧急模式且立即人机验证时验证通过后将状态存入kv服务
              insertVerifyStatus(src_ip_and_host_ + FIX_VERIFY, verify_status_kv_db_);
            } else {
              // 其余模式清除人机验证状态
              deleteVerifyStatus(src_ip_and_host_ + FIX_VERIFY, blacklist_kv_db_);
            }
            // 请求限速服务清理访问配额和人机验证次数配额
            sendAsyncGrpcCleanQuotaRequest(src_ip_and_host_);
            return;
          }

          // 本地回复给前端，人机验证服务返回的结果
          decoder_callbacks_->sendLocalReply(
              Http::Code::OK, value_str,
              [&mode_str](Http::HeaderMap& headers) {
                if (mode_str == CODE_GET_HTML) {
                  headers.setCopy(Http::LowerCaseString("content-type"),
                                  "text/html; charset=utf-8");
                }
              },
              absl::nullopt, "");
          return;
        }
      }
    }
  }

  ENVOY_LOG(debug, "service request error");
  decoder_callbacks_->sendLocalReply(Http::Code::InternalServerError, "", nullptr, absl::nullopt,
                                     "service_error");
}

void AntiCCFilter::onFailure(const Http::AsyncClient::Request&, Http::AsyncClient::FailureReason) {
  state_ = State::Responded;
  request_ = nullptr;
  decoder_callbacks_->sendLocalReply(Http::Code::RequestTimeout, "", nullptr, absl::nullopt, "");
}

void AntiCCFilter::complete(Filters::Common::RatelimitClient::LimitStatus status,
                            Filters::Common::RatelimitClient::LimitGrpcResponsePtr&& response) {
  state_ = State::Complete;
  ENVOY_LOG(trace, "Limit gRPC request complete, status={}", status);
  switch (status) {
  case Filters::Common::RatelimitClient::LimitStatus::OverLimit: {
    // 判断是否为空转
    if (!process_verify_quota_ && filter_hcm_config_->action()->dryrun()) {
      ENVOY_LOG(trace, "Over quota, but filter dryrun");
      setLog(v3::Act::PASS);
      request_headers_->setReferenceKey(header_ratelimit_drop_, "drop");
      decoder_callbacks_->continueDecoding();

    }
    // 判断是否人机验证
    else if (!process_verify_quota_ && (filter_hcm_config_->action()->manMachineVerification() ||
                                        (filter_hcm_config_->mode() == v3::EMERGENCY &&
                                         !filter_hcm_config_->action()->allRunVerification()))) {
      ENVOY_LOG(trace, "Over quota, perform human-machine verification");
      // 新增人机验证状态
      insertVerifyStatus(src_ip_and_host_ + FIX_VERIFY, blacklist_kv_db_);
      std::string body;
      requestBodyAddSrcIp(body);
      requestBodyAddPath(body);
      request_ = sendAsyncHttpRequest(RequestMode::GetHtml, body);

    } else {
      // 插入远程缓存
      // key：源ip_目的ip:目的端口， value:触发封禁的配额的duration
      insertToRemoteCache(src_ip_and_host_, std::to_string(response->duration()),
                          filter_hcm_config_->action()->blockTime());
      // 请求DDos插件添加黑名单
      std::string body;
      requestBodyAddBlacklist(body);
      request_ = sendAsyncHttpRequest(RequestMode::BlockIp, body);
    }
    break;
  }
  case Filters::Common::RatelimitClient::LimitStatus::Error: {
    state_ = State::Responded;
    decoder_callbacks_->sendLocalReply(Http::Code::InternalServerError, "", nullptr, absl::nullopt,
                                       "rate_limiter_error");
    break;
  }
  case Filters::Common::RatelimitClient::LimitStatus::OK: {
    if (process_verify_quota_) {
      // 请求验证服务器,请求验证界面
      std::string body;
      requestBodyAddSrcIp(body);
      request_ = sendAsyncHttpRequest(RequestMode::GetCode, body);
    } else {
      decoder_callbacks_->continueDecoding();
    }
    break;
  }
  }
}

void AntiCCFilter::complete(Filters::Common::RatelimitClient::CleanStatus status) {
  state_ = State::Responded;
  ENVOY_LOG(trace, "Clean quota gRPC request complete, status={}", status);
  if (status == Filters::Common::RatelimitClient::CleanStatus::SUCCESS) {
    setLog(v3::Act::VERIFY_PASS);
    decoder_callbacks_->sendLocalReply(Http::Code::OK, VERIFY_RESULT_SUCCESS, nullptr,
                                       absl::nullopt, "clean_quota_success");
  } else {
    decoder_callbacks_->sendLocalReply(Http::Code::OK, VERIFY_RESULT_FAILURE, nullptr,
                                       absl::nullopt, "clean_quota_failure");
  }
}

void AntiCCFilter::onDestroy() {
  switch (state_) {
  case State::CallingLimit:
    ratelimit_client_->cancel();
    state_ = State::Complete;
    break;
  case State::CallingHttp:
    request_->cancel();
    request_ = nullptr;
    state_ = State::Complete;
    break;
  case State::CallingKv:
    blacklist_kv_db_->cancel();
    if (verify_status_kv_db_) {
      verify_status_kv_db_->cancel();
    }
    state_ = State::Complete;
    break;
  default:
    break;
  }
}

void AntiCCFilter::onStreamComplete() {
  if (is_log_request_) {
    log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true));
  }
}

/**
 * 设置可视化数据，仅在触发防护规则（限速配额满的情况下）上报日志
 * @param act 防护行为
 * @param duration
 * 限速服务返回的响应携带的触发访问配额的单位时间段。自动模式为两组限速配额，但仅会触发一组，以duration来确认quota
 */
void AntiCCFilter::setLog(v3::Act act, uint32_t duration) {
  is_log_request_ = true;
  log_.set_act(act);
  log_.set_block_time(filter_hcm_config_->action()->blockTime());
  v3::Quota* new_quota = log_.mutable_quota();
  for (auto& quota : filter_hcm_config_->action()->rateLimitQuota()) {
    if (duration != 0 && quota.duration_ != duration) {
      continue;
    }
    new_quota->set_max_count(quota.max_count_);
    new_quota->set_duration(quota.duration_);
    break;
  }
}

} // namespace AntiCC
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy