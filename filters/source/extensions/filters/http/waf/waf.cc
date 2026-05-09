#include <string>
#include <vector>
#include <string.h>
#include <fstream>
#include <dirent.h>
#include <boost/algorithm/string.hpp>

#include "openssl/aes.h"

#include "waf.h"
#include "filters/source/extensions/filters/http/common/utility/utility.h"

#include "filters/api/envoy/extensions/filters/http/waf/v3/waf.pb.h"
#include "envoy/stats/scope.h"

#include "envoy/server/filter_config.h"

#include "source/common/common/macros.h"
#include "source/common/http/path_utility.h"
#include "source/common/config/metadata.h"
#include "source/common/http/utility.h"
#include "source/common/http/headers.h"

#include "absl/container/fixed_array.h"

#include "modsecurity/rules_set_properties.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WafFilter {

using namespace Envoy::Extensions::Filters::Common::Utility;
const std::string FilterGlobalConfig::conf_path_base_("plugins/waf/conf");

FilterRouteConfig::FilterRouteConfig(const v3::WafRoute& proto_config)
    : disable_request_(proto_config.disable() || proto_config.disable_request()),
      disable_response_(proto_config.disable() || proto_config.disable_response()),
      enable_ratelimit_(proto_config.enable()), ip_whitelist_([&proto_config]() {
        std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr> ip_whitelists;
        for (const auto& ip_whitelist : proto_config.ip_whitelist()) {
          ip_whitelists.push_back(
              std::make_unique<Filters::Common::IpWhitelist::IpWhitelist>(ip_whitelist));
        }
        return ip_whitelists;
      }()) {
  // 限速route构造
  setRatelimitRouteProtoConfig(proto_config, ratelimit_route_proto_config_);
  ratelimit_route_config_ =
      std::make_shared<StrongGlobalRatelimit::StrongGlobalRateLimitFilterRouteConfig>(
          ratelimit_route_proto_config_);
}

FilterGlobalConfig::FilterGlobalConfig(const v3::WafGlobal& proto_config,
                                       const std::string& stats_prefix,
                                       Server::Configuration::FactoryContext& context)
    : rules_path_before_(PROTOBUF_GET_STRING_OR_DEFAULT(proto_config, rules_path_before,
                                                        conf_path_base_ + "/base/before.conf")),
      rules_path_after_(PROTOBUF_GET_STRING_OR_DEFAULT(proto_config, rules_path_after,
                                                       conf_path_base_ + "/base/after.conf")),
      tar_rules_path_(PROTOBUF_GET_STRING_OR_DEFAULT(proto_config, tar_rules_path,
                                                     conf_path_base_ + "/waf_rule.tar.gz")),
      paranoia_level_(proto_config.paranoia_level()), webhook_(proto_config.webhook()),
      detection_only_(proto_config.detection_only()), mode_(proto_config.mode()),
      disable_rules_(generateRuleInfo(proto_config.disable_rules())),
      pass_rules_(generateRuleInfo(proto_config.pass_rules())),
      tls_(context.threadLocal().allocateSlot()),
      // 需要保证请求分数约束规则必须添加
      rule_types_(proto_config.rule_types() |
                  v3::WafGlobal_RuleType::WafGlobal_RuleType_RT_BlockingEvaluation),
      stats_(generateStats(stats_prefix + "waf.", context.scope())), runtime_(context.runtime()),
      context_(context) {
  modsec_.reset(new modsecurity::ModSecurity());
  modsec_->setConnectorInformation(filter_name);
  modsec_->setServerLogCb(Filter::_logCb, modsecurity::RuleMessageLogProperty |
                                              modsecurity::IncludeFullHighlightLogProperty);
  modsec_rules_.reset(new modsecurity::RulesSet());

  if (proto_config.has_grpc()) {
    // 限速global client构造
    setRatelimitGlobalProtoConfig(proto_config, ratelimit_global_proto_config_);
    ratelimit_global_config_ =
        std::make_shared<StrongGlobalRatelimit::StrongGlobalRateLimitFilterGlobalConfig>(
            ratelimit_global_proto_config_);
    const std::chrono::milliseconds timeout = std::chrono::milliseconds(
        PROTOBUF_GET_MS_OR_DEFAULT(ratelimit_global_proto_config_.grpc(), timeout, 20));
    makeClient_ = [&, timeout]() {
      return Filters::Common::RatelimitClient::Impl::rateLimitClient(
          context_, ratelimit_global_proto_config_.grpc(), timeout);
    };
  }

  // 解析rule conf
  std::shared_ptr<char[]> passwd = parseRuleConf();

  // rule conf加载路径还原
  const std::vector<std::string> rule_conf_path = restoreRulePath(passwd);

  // 设置仅检测模式
  setDetectOnly();

  // 防护等级需要在初始化之前设置
  setDefendLevel();

  // load rule path before
  loadCoreRulesBefore();

  // 设置性能模式
  setPerformanceMode();

  // load core rule set
  loadCoreRulesSet();

  // load rule path after
  loadCoreRulesAfter();

  // 对不同等级的规则重设分数
  setLevelScore();

  // 规则放行
  setPassRules();

  // 规则禁用
  setDisableRules();

  if (proto_config.has_webhook()) {
    tls_->set([this, &context](Event::Dispatcher&) -> ThreadLocal::ThreadLocalObjectSharedPtr {
      return std::make_shared<ThreadLocalWebhook>(new WebhookFetcher(
          context.clusterManager(), webhook_.http_uri(), webhook_.secret(), *this));
    });
  }

  // 删除明文rule conf
  removeRulePath(rule_conf_path);
}

// destructor
FilterGlobalConfig::~FilterGlobalConfig() {}

// webhook
void FilterGlobalConfig::invoke_webhook(const modsecurity::RuleMessage* ruleMessage) {
  if (tls_->currentThreadRegistered()) {
    ENVOY_LOG(debug, "invoke_webhook");
    const auto fetcher = tls_->getTyped<ThreadLocalWebhook>().webhook_fetcher_;
    fetcher->invoke(Utility::getRuleMessageAsJsonString(ruleMessage));
  }
  ENVOY_LOG(debug, "invoke_webhook out");
}

// on success
void FilterGlobalConfig::onSuccess(const Http::ResponseMessagePtr&) {
  ENVOY_LOG(info, "webhook success!");
}

// on fail
void FilterGlobalConfig::onFailure(FailureReason) { ENVOY_LOG(info, "webhook failure!"); }

bool FilterGlobalConfig::decryptWafConf(std::shared_ptr<char[]> passwd) {
  // 获取tmp.tar.gz数据流
  const std::string tmp_file = conf_path_base_ + "/tmp.tar.gz";
  std::ifstream tar_file(tmp_file, std::ios::in | std::ios::binary);
  if (!tar_file) {
    ENVOY_LOG(error, "Failed to open rule conf tar");
    return false;
  }
  std::vector<unsigned char> encrypted((std::istreambuf_iterator<char>(tar_file)),
                                       std::istreambuf_iterator<char>());
  tar_file.close();
  // 解密数据流
  std::string iv_hex = "b39a46f5cc4f0d45b39a46f5cc4f0d45"; // iv与加密脚本中的值保持一致
  std::vector<uint8_t> keys = Hex::decode(passwd.get());
  std::vector<uint8_t> iv = Hex::decode(iv_hex);
  int num = 0;
  std::vector<unsigned char> decrypted(encrypted.size());
  AES_KEY decrypt_key;
  if (AES_set_encrypt_key(keys.data(), 128, &decrypt_key)) {
    ENVOY_LOG(error, "Failed to set encrypt key");
    return false;
  }
  AES_cfb128_encrypt(encrypted.data(), decrypted.data(), encrypted.size(), &decrypt_key, iv.data(),
                     &num, AES_DECRYPT);
  if (decrypted.empty()) {
    ENVOY_LOG(error, "Failed to decrtpted");
    return false;
  }
  // 将解密数据流写成压缩包后解压
  const std::string decode_tar_file = conf_path_base_ + "/tmp_decode.tar.gz";
  std::ofstream tmp_decode_tar_file(decode_tar_file, std::ios::out | std::ios::binary);
  if (!tmp_decode_tar_file) {
    ENVOY_LOG(error, "Failed to create {}", decode_tar_file);
    return false;
  }
  tmp_decode_tar_file.write(reinterpret_cast<char*>(decrypted.data()), decrypted.size());
  if (!tmp_decode_tar_file) {
    ENVOY_LOG(error, "Failed to write data to {}", decode_tar_file);
    remove(decode_tar_file.c_str());
    return false;
  }
  tmp_decode_tar_file.close();
  const std::string uncompress_cmd =
      fmt::format("tar zxf {} -C {}", decode_tar_file, conf_path_base_);
  int stat = system(uncompress_cmd.c_str());
  if (stat != 0) {
    ENVOY_LOG(error, "Failed to uncompress the decodeed tar file");
    remove(decode_tar_file.c_str());
    return false;
  }
  remove(decode_tar_file.c_str());
  return true;
}

std::shared_ptr<char[]> FilterGlobalConfig::parseRuleConf() {
  int passwd_pos;
  int passwd_len;
  void* ptr_pos = &passwd_pos;
  void* ptr_len = &passwd_len;
  struct stat info;
  stat(tar_rules_path_.c_str(), &info);
  int file_size = info.st_size;

  std::ifstream tar_file(tar_rules_path_.c_str(), std::ios::in | std::ios::binary);
  if (!tar_file) {
    ENVOY_LOG(error, "Failed to open rule conf tar: {}", tar_rules_path_.c_str());
    return NULL;
  }
  const std::string tmp_file = conf_path_base_ + "/tmp.tar.gz";
  std::ofstream tmp_tar_file(tmp_file.c_str(), std::ios::out | std::ios::binary);
  if (!tmp_tar_file) {
    ENVOY_LOG(error, "Failed to create {}", tmp_file);
    return NULL;
  }
  std::shared_ptr<char[]> buf(new char[file_size + 1]);
  tar_file.read(static_cast<char*>(ptr_pos), 4);
  tar_file.read(static_cast<char*>(ptr_len), 4);
  tar_file.read(buf.get(), passwd_pos - 8);
  tmp_tar_file.write(buf.get(), passwd_pos - 8);
  std::shared_ptr<char[]> passwd(new char[passwd_len + 1]);
  memset(passwd.get(), 0, passwd_len + 1);
  tar_file.read(passwd.get(), passwd_len);
  memset(buf.get(), 0, file_size + 1);
  tar_file.read(buf.get(), file_size - passwd_pos - passwd_len);
  tmp_tar_file.write(buf.get(), file_size - passwd_pos - passwd_len);
  tar_file.close();
  tmp_tar_file.close();
  return passwd;
}

std::vector<std::string> FilterGlobalConfig::restoreRulePath(std::shared_ptr<char[]> passwd) {
  std::vector<std::string> rule_conf_path;
  if (passwd.get()) {
    if (!decryptWafConf(passwd)) {
      ENVOY_LOG(error, "Failed to decode rule conf");
      return rule_conf_path;
    }
    const std::string tmp_dir = conf_path_base_ + "/tmp";
    DIR* dir = opendir(tmp_dir.c_str());
    dirent* rule_dir = NULL;
    if (dir != NULL) {
      while (true) {
        rule_dir = readdir(dir);
        if (rule_dir == NULL) {
          break;
        }
        if (strcmp(rule_dir->d_name, ".") == 0 || strcmp(rule_dir->d_name, "..") == 0) {
          continue;
        }
        const char* tmp_name = rule_dir->d_name;
        const std::string old_name = fmt::format("{}/tmp/{}", conf_path_base_, tmp_name);
        const std::string new_name = fmt::format("{}/{}", conf_path_base_, tmp_name);
        int stat = rename(old_name.c_str(), new_name.c_str());
        if (stat != 0) {
          ENVOY_LOG(error, "move rule conf of {} failed", tmp_name);
        }
        rule_conf_path.push_back(new_name);
      }
      closedir(dir);
    } else {
      ENVOY_LOG(error, "open {}/tmp failed", conf_path_base_);
    }
  } else {
    ENVOY_LOG(error, "Failed to get tar passwd");
  }
  const std::string clean_tmp_dir = fmt::format("rm -rf {}/tmp", conf_path_base_);
  const std::string clean_tmp_tar = fmt::format("{}/tmp.tar.gz", conf_path_base_);
  system(clean_tmp_dir.c_str());
  remove(clean_tmp_tar.c_str());
  return rule_conf_path;
}

void FilterGlobalConfig::removeRulePath(const std::vector<std::string>& rule_conf_path) {
  const std::string waf_rule_flag = fmt::format("{}/tools/show_waf_rules", conf_path_base_);
  if (access(waf_rule_flag.c_str(), F_OK)) {
    for (auto it = rule_conf_path.begin(); it != rule_conf_path.end(); ++it) {
      const std::string cmd = fmt::format("rm -rf {}", *it);
      system(cmd.c_str());
    }
  }
}

void FilterGlobalConfig::setDetectOnly() {
  if (detection_only_) {
    runRuleAction("SecRuleEngine DetectionOnly");
  }
}

void FilterGlobalConfig::setDefendLevel() {
  const std::string cmd = fmt::format(
      "SecAction \"id:205, phase:1,nolog,pass,t:none,setvar:tx.blocking_paranoia_level={}\"",
      paranoia_level() + 1);
  runRuleAction(cmd.c_str());
}

void FilterGlobalConfig::loadCoreRulesBefore() {
  if (!rules_path_before().empty()) {
    runRuleAction(rules_path_before().c_str(), "file");
  }
}

void FilterGlobalConfig::setPerformanceMode() {
  std::string rule_head;
  std::string rule_body;
  moderationOption(mode_, rule_head, rule_body);
  runRuleAction(rule_head.c_str());
  runRuleAction(rule_body.c_str());
}

void FilterGlobalConfig::loadCoreRulesSet() {
  Utility::traverseRuleFiles(rule_types_, [&](v3::WafGlobal::RuleType rule_type) {
    const char* file_name = Utility::parseRuleFileName(rule_type).data();
    std::string file_path = conf_path_base_;
    // if (rule_type > v3::WafGlobal::RT_WebShells) {
    //   file_path = file_path + "/srhinoruleset/rules";
    // } else {
    //   file_path = file_path + "/coreruleset/rules";
    // }
    // file_path = file_path + "/" + file_name;
    // runRuleAction(file_path.c_str(), "file");
    if (rule_type <= v3::WafGlobal::RT_WebShells) {
      file_path = file_path + "/coreruleset/rules";
      file_path = file_path + "/" + file_name;
      runRuleAction(file_path.c_str(), "file");
    }
  });
}

void FilterGlobalConfig::loadCoreRulesAfter() {
  if (!rules_path_after().empty()) {
    runRuleAction(rules_path_after().c_str(), "file");
  }
}

void FilterGlobalConfig::setLevelScore() {
  const std::string cmd = fmt::format(
      "SecRuleUpdateActionById 901100 \"setvar:\'tx.inbound_anomaly_score_threshold={}\'\"",
      ((4 - paranoia_level()) * 5));
  runRuleAction(cmd.c_str());
}

void FilterGlobalConfig::setPassRules() {
  if (!pass_rules_.empty()) {
    getRealRuleId(pass_rules_);
    for (auto it = pass_rules_.begin(); it != pass_rules_.end(); ++it) {
      std::string rule_cmd = "";
      int phase = getPhaseById(atoi(((*it).rule_id).c_str()));
      setPassRuleAction(phase, (*it).rule_id, (*it).level, rule_cmd);
      runRuleAction(rule_cmd.c_str());
      pass_rules_id_.insert(atoi(((*it).rule_id).c_str()));
    }
  }
}

void FilterGlobalConfig::setDisableRules() {
  if (!disable_rules_.empty()) {
    getRealRuleId(disable_rules_);
    for (auto it = disable_rules_.begin(); it != disable_rules_.end(); ++it) {
      std::string disable_command = "SecRuleRemoveByid " + (*it).rule_id;
      runRuleAction(disable_command.c_str());
      disable_rules_id_.insert(atoi(((*it).rule_id).c_str()));
    }
  }
}

void FilterGlobalConfig::moderationOption(const v3::WafGlobal::Moderation mode_,
                                          std::string& rule_head, std::string& rule_body) {
  switch (mode_) {
  case v3::WafGlobal::performance:
    rule_head = R"(SecDefaultAction "phase:1,log,auditlog,deny,status:403")";
    rule_body = R"(SecDefaultAction "phase:2,log,auditlog,deny,status:403")";
    break;
  case v3::WafGlobal::precise:
    rule_head = R"(SecDefaultAction "phase:1,log,auditlog,pass")";
    rule_body = R"(SecDefaultAction "phase:2,log,auditlog,pass")";
    break;
  // 默认精度优先
  default:
    rule_head = R"(SecDefaultAction "phase:1,log,auditlog,pass")";
    rule_body = R"(SecDefaultAction "phase:2,log,auditlog,pass")";
  }
}

int FilterGlobalConfig::getPhaseById(int64_t rule_id) {
  for (int i = modsecurity::Phases::RequestHeadersPhase;
       i <= modsecurity::Phases::NUMBER_OF_PHASES - 2; i++) {
    for (size_t z = 0; z < (*((modsec_rules_->m_rulesSetPhases)[i])).size(); z++) {
      modsecurity::RuleWithOperator* rule_ckc = dynamic_cast<modsecurity::RuleWithOperator*>(
          (*((modsec_rules_->m_rulesSetPhases)[i])).at(z).get());
      if (!rule_ckc) {
        continue;
      }
      if (rule_ckc->m_ruleId == rule_id) {
        return i;
      }
    }
  }
  ENVOY_LOG(error, "Failed to find rule_id: {}", rule_id);
  return -1;
}

void FilterGlobalConfig::setPassRuleAction(int phase, const std::string& rule_id, uint32_t level,
                                           std::string& rule_cmd) {
  if (phase != -1) {
    // SecRuleUpdateActionById {id} "pass,setvar:'tx.{inbound}_anomaly_score_pl{level}=0'"
    if (phase == modsecurity::Phases::RequestHeadersPhase ||
        phase == modsecurity::Phases::RequestBodyPhase) {
      rule_cmd = "SecRuleUpdateActionById " + rule_id + " " +
                 "\"pass,setvar:\'tx.inbound_anomaly_score_pl" + std::to_string(level) + "=0\'\"";

    } else {
      rule_cmd = "SecRuleUpdateActionById " + rule_id + " " +
                 "\"pass,setvar:\'tx.outbound_anomaly_score_pl" + std::to_string(level) + "=0\'\"";
    }
  }
}

void FilterGlobalConfig::runRuleAction(const char* const rule_cmd, const char* const option) {
  if (strcmp(rule_cmd, "") != 0) {
    int rulesLoaded = -1;
    if (strcmp(option, "cmd") == 0) {
      rulesLoaded = modsec_rules_->load(rule_cmd);
    } else {
      rulesLoaded = modsec_rules_->loadFromUri(rule_cmd);
    }
    if (rulesLoaded == -1) {
      ENVOY_LOG(error, "Failed to load rules: {}", modsec_rules_->getParserError());
    } else {
      ENVOY_LOG(info, "Loaded  rules success: {}", rule_cmd);
    }
  }
}
// Srhino-10491105------>941150
void FilterGlobalConfig::getRealRuleId(std::list<RuleInfo>& rule) {
  auto it = rule.begin();
  while (it != rule.end()) {
    int pos = (*it).rule_id.find('-');
    pos += 3;
    std::string str = (*it).rule_id.substr(pos);
    for (int i = 0; i < str.length(); i = i + 2) {
      char tmp = str[i];
      str[i] = str[i + 1];
      str[i + 1] = tmp;
    }
    (*it).rule_id = str;
    ++it;
  }
}

// 设置 ratelimit global proto
void FilterGlobalConfig::setRatelimitGlobalProtoConfig(
    const v3::WafGlobal& proto_config,
    ratelimit_v3::StrongGlobalRateLimitGlobal& ratelimit_global_proto_config) {
  // 设置grpc
  const std::chrono::milliseconds timeout =
      std::chrono::milliseconds(PROTOBUF_GET_MS_OR_DEFAULT(proto_config.grpc(), timeout, 20));
  auto time_ms = timeout.count();
  int32_t seconds = time_ms / 1000;
  int32_t nanos = (time_ms % 1000) * 1000000;
  google::protobuf::Duration* duration_ptr = new google::protobuf::Duration();
  duration_ptr->set_seconds(seconds);
  duration_ptr->set_nanos(nanos);
  const std::string grpc_name = proto_config.grpc().envoy_grpc().cluster_name();
  envoy::config::core::v3::GrpcService::EnvoyGrpc* envoy_grpc =
      new envoy::config::core::v3::GrpcService::EnvoyGrpc();
  envoy_grpc->set_cluster_name(grpc_name);
  envoy::config::core::v3::GrpcService* grpc = new envoy::config::core::v3::GrpcService();
  grpc->set_allocated_envoy_grpc(envoy_grpc);
  grpc->set_allocated_timeout(duration_ptr);
  ratelimit_global_proto_config.set_allocated_grpc(grpc);
}

// 设置 ratelimit route proto
void FilterRouteConfig::setRatelimitRouteProtoConfig(
    const v3::WafRoute& proto_config,
    ratelimit_v3::StrongGlobalRateLimitRoute& ratelimit_route_proto_config) {
  // 限速开关赋值
  // ratelimit_route_proto_config.set_enable(proto_config.enable());

  // rules赋值
  for (const auto& rule : proto_config.rules()) {
    ratelimit_route_proto_config.add_rules()->CopyFrom(rule);
  }
  // 版本兼容，限速rule中增加的enable字段控制面不会下发，在这里赋值
  for (int i = 0; i < ratelimit_route_proto_config.rules_size(); i++) {
    ratelimit_route_proto_config.mutable_rules(i)->set_enable(true);
  }

  // 白名单赋值
  // for (const auto& ip_whitelist : proto_config.ip_whitelist()) {
  //   ratelimit_route_proto_config.add_ip_whitelist()->CopyFrom(ip_whitelist);
  // }

  // waf调用限速标记，防止限速内部在grpc响应后调用continueDecoding
  ratelimit_route_proto_config.set_external_invoke(proto_config.waf_ratelimit());
}

// 重写ratelimit::getRouteConfig
const StrongGlobalRatelimit::StrongGlobalRateLimitFilterRouteConfig*
WafStrongGlobalRateLimitFilter::getRouteConfig() const {
  const Envoy::Router::VirtualHostImpl* vh = waf_filter_ptr_->getVirtualHost();
  const FilterRouteConfig* filter_vh_config = nullptr;
  const FilterRouteConfig* filter_route_config = nullptr;
  if (vh != nullptr) {
    auto config = vh->perFilterConfig(filter_name);
    if (config != nullptr) {
      filter_vh_config = dynamic_cast<const FilterRouteConfig*>(config);
      if (filter_vh_config != nullptr) {
        auto route = decoder_callbacks_->streamInfo().route();
        filter_route_config =
            dynamic_cast<const FilterRouteConfig*>(route->mostSpecificPerFilterConfig(filter_name));
        if (filter_route_config != nullptr) {
          return (filter_route_config->ratelimit_route_config()).get();
        }
        return (filter_vh_config->ratelimit_route_config()).get();
      }
    }
  }
  return nullptr;
}

void WafStrongGlobalRateLimitFilter::complete(
    Filters::Common::RatelimitClient::LimitStatus status,
    Filters::Common::RatelimitClient::LimitGrpcResponsePtr&& response) {
  StrongGlobalRatelimit::StrongGlobalRateLimitFilter::complete(status, std::move(response));
  // grpc响应回来后如果没有触发限速，继续处理waf规则检测
  if (ratelimitState() == StrongGlobalRatelimit::StrongGlobalRateLimitFilter::State::Complete) {
    waf_filter_ptr_->wafRequestHeadersProc(*request_headers_, end_stream_);
    decoder_callbacks_->continueDecoding();
  }
}

// Filter
// constructor
Filter::Filter(FilterGlobalConfigSharedPtr config,
               Server::Configuration::ServerFactoryContext& context)
    : PassThroughFilterEx(context), config_(config), white_list_flag_(false) {
  modsec_transaction_.reset(
      new modsecurity::Transaction(config_->modsec().get(), config_->modsec_rules().get(), this));
  if (config_->ratelimit_global_proto_config().has_grpc()) {
    ratelimit_filter_.reset(new WafStrongGlobalRateLimitFilter(config_->ratelimit_global_config(),
                                                               config_->makeClient(), context));
  }
}

void Filter::onStreamComplete() {
  // 限速可视化字段
  if (config_->ratelimit_global_proto_config().has_grpc() &&
      ratelimit_filter_->ratelimitState() ==
          StrongGlobalRatelimit::StrongGlobalRateLimitFilter::State::Responded) {
    setRatelimitVisual();
  }
  // log_带add_rule_id 才上报， 限速也上报
  if (log_.rule_id_size() > 0 || log_.ratelimit_refuse() == true) {
    log(MessageUtil::getJsonStringFromMessageOrDie(log_));
    ENVOY_LOG(debug, "to upload for rule_size:{}, limit:{}, log: {}", log_.rule_id_size(),
              log_.ratelimit_refuse(), MessageUtil::getJsonStringFromMessageOrDie(log_));
  }
  if (log_.rule_id_size() > 0) {
    // 借用ResponseFlag::DelayInjected标记来打个标记
    // 以便访问日志可以将WAF检测模式下触发规则的日志写入到“紧急通道”(execption_body.log)
    // fixme: 需要为envoy增加patch：增加StreamInfo::ResponseFlag::WafDetection标志
    encoder_callbacks_->streamInfo().setResponseFlag(StreamInfo::ResponseFlag::DelayInjected);
  }
}

void Filter::onDestroy() {
  if (config_->ratelimit_global_proto_config().has_grpc()) {
    ratelimit_filter_->onDestroy();
  }
  modsec_transaction_->processLogging();
}

const char* getProtocolString(const Http::Protocol protocol) {
  switch (protocol) {
  case Http::Protocol::Http10:
    return "1.0";
  case Http::Protocol::Http11:
    return "1.1";
  case Http::Protocol::Http2:
    return "2.0";
  case Http::Protocol::Http3:
    return "3.0";
  }
  NOT_REACHED_GCOVR_EXCL_LINE;
}

bool Filter::requestDisabled() {
  const auto route = decoder_callbacks_->route();
  if (route) {
    const auto* route_local =
        dynamic_cast<const FilterRouteConfig*>(route->mostSpecificPerFilterConfig(filter_name));
    return route_local && route_local->disable_request();
  }
  return true;
}

inline const Envoy::Router::VirtualHostImpl* Filter::getVirtualHost() const {
  auto route = decoder_callbacks_->streamInfo().route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      auto& vh = entry->virtualHost();
      return dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl && route_impl->isDirectResponse()) {
        const Envoy::Router::VirtualHost& vh = route_impl->virtualHost();
        return dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
      }
    }
  }
  return nullptr;
}

const FilterRouteConfig*
Filter::getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const {
  const FilterRouteConfig* filter_vh_config = nullptr;
  if (vh != nullptr) {
    auto config = vh->perFilterConfig(filter_name);
    if (config != nullptr) {
      filter_vh_config = dynamic_cast<const FilterRouteConfig*>(config);
    }
  }
  return filter_vh_config;
}

bool Filter::wafRequestHeadersProc(Http::RequestHeaderMap& headers, bool end_stream) {
  auto downstreamAddress =
      decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
  auto uri = headers.getPathValue();
  auto method = headers.getMethodValue();

  if (status_.intervined || status_.request_processed) {
    return getRequestHeadersStatus() == Http::FilterHeadersStatus::Continue;
  }
  if (requestDisabled()) {
    status_.request_processed = true;
    return true;
  }
  ASSERT(decoder_callbacks_->connection() != nullptr);
  auto localAddress =
      decoder_callbacks_->connection()->connectionInfoProvider().localAddress(); // v1.20
  ASSERT(localAddress != nullptr);
  ASSERT(localAddress->type() == Network::Address::Type::Ip);
  block_stage_ = BlockStage::DecodeRequestHeader;
  modsec_transaction_->processConnection(
      downstreamAddress->ip()->addressAsString().c_str(), downstreamAddress->ip()->port(),
      localAddress->ip()->addressAsString().c_str(), localAddress->ip()->port());
  if (intervention()) {
    return false;
  }
  modsec_transaction_->processURI(
      std::string(uri).c_str(), std::string(method).c_str(),
      getProtocolString(
          decoder_callbacks_->streamInfo().protocol().value_or(Http::Protocol::Http11)));
  if (intervention()) {
    return false;
  }
  headers.iterate([this](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    std::string k = std::string(header.key().getStringView());
    std::string v = std::string(header.value().getStringView());
    // 修复类似REQUEST-944-APPLICATION-ATTACK-JAVA.conf::944240这种规则的BUG
    // 原因为args里匹配了关键字，而:path头里也包含查询参数，导致被匹配两次，得分翻倍
    if (k == Http::Headers::get().Path.get()) {
      return Http::HeaderMap::Iterate::Continue;
    }
    this->modsec_transaction_->addRequestHeader(k.c_str(), v.c_str());
    // TODO - does this special case makes sense? it doesn't exist on apache/nginx modsecurity
    // bridges. host header is cannonized to :authority even on http older than 2 see
    // https://github.com/envoyproxy/envoy/issues/2209
    if (k == Http::Headers::get().Host.get()) {
      this->modsec_transaction_->addRequestHeader(Http::Headers::get().HostLegacy.get().c_str(),
                                                  v.c_str());
    }
    return Http::HeaderMap::Iterate::Continue;
  });
  modsec_transaction_->processRequestHeaders();
  if (end_stream) {
    status_.request_processed = true;
    modsec_transaction_->processRequestBody();
  }
  if (intervention()) {
    return false;
  }
  return getRequestHeadersStatus() == Http::FilterHeadersStatus::Continue;
}

bool Filter::wafRequestDataProc(const Buffer::Instance& data, bool end_stream) {
  if (status_.intervined || status_.request_processed) {
    return getRequestStatus() == Http::FilterDataStatus::Continue;
  }
  block_stage_ = BlockStage::DecodeRequestBody;
  for (const Buffer::RawSlice& slice : data.getRawSlices()) {
    size_t requestLen = modsec_transaction_->getRequestBodyLength();
    // If append fails or append reached the limit, test for intervention (in case
    // SecRequestBodyLimitAction is set to Reject) Note, we can't rely solely on the return value
    // of append, when SecRequestBodyLimitAction is set to Reject it returns true and sets the
    // intervention
    if (modsec_transaction_->appendRequestBody(static_cast<unsigned char*>(slice.mem_),
                                               slice.len_) == false ||
        (slice.len_ > 0 && requestLen == modsec_transaction_->getRequestBodyLength())) {
      ENVOY_LOG(debug, "Filter::decodeData appendRequestBody reached limit");
      if (intervention()) {
        return false;
      }
      // Otherwise set to process request
      end_stream = true;
      break;
    }
  }
  if (end_stream) {
    status_.request_processed = true;
    modsec_transaction_->processRequestBody();
  }
  if (intervention()) {
    return false;
  }
  return getRequestStatus() == Http::FilterDataStatus::Continue;
}

bool Filter::wafResponseHeadersProc(Http::ResponseHeaderMap& headers, bool end_stream) {
  if (status_.intervined || status_.response_processed) {
    return getResponseHeadersStatus() == Http::FilterHeadersStatus::Continue;
  }
  if (responseDisabled()) {
    status_.response_processed = true;
    return true;
  }
  block_stage_ = BlockStage::EncodeResponseHeader;
  uint64_t response_code = Http::Utility::getResponseStatus(headers);
  headers.iterate([this](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    this->modsec_transaction_->addResponseHeader(
        std::string(header.key().getStringView()).c_str(),
        std::string(header.value().getStringView()).c_str());
    return Http::HeaderMap::Iterate::Continue;
  });
  modsec_transaction_->processResponseHeaders(
      response_code, getProtocolString(encoder_callbacks_->streamInfo().protocol().value_or(
                         Http::Protocol::Http11)));
  if (end_stream) {
    status_.response_processed = true;
    modsec_transaction_->processResponseBody();
  }
  if (intervention()) {
    return false;
  }
  return getResponseHeadersStatus() == Http::FilterHeadersStatus::Continue;
}

bool Filter::wafResponseDataProc(const Buffer::Instance& data, bool end_stream) {
  if (status_.intervined || status_.response_processed) {
    return getResponseStatus() == Http::FilterDataStatus::Continue;
  }
  block_stage_ = BlockStage::EncodeResponseBody;
  for (const Buffer::RawSlice& slice : data.getRawSlices()) {
    size_t responseLen = modsec_transaction_->getResponseBodyLength();
    // If append fails or append reached the limit, test for intervention (in case
    // SecResponseBodyLimitAction is set to Reject) Note, we can't rely solely on the return value
    // of append, when SecResponseBodyLimitAction is set to Reject it returns true and sets the
    // intervention
    if (modsec_transaction_->appendResponseBody(static_cast<unsigned char*>(slice.mem_),
                                                slice.len_) == false ||
        (slice.len_ > 0 && responseLen == modsec_transaction_->getResponseBodyLength())) {
      ENVOY_LOG(debug, "Filter::encodeData appendResponseBody reached limit");
      if (intervention()) {
        return false;
      }
      // Otherwise set to process response
      end_stream = true;
      break;
    }
  }
  if (end_stream) {
    status_.response_processed = true;
    modsec_transaction_->processResponseBody();
  }
  if (intervention()) {
    return false;
  }
  return getResponseStatus() == Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(debug, "Filter::decodeHeaders");
  auto downstreamAddress =
      decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
  // According to documentation, downstreamAddress should never be nullptr
  ASSERT(downstreamAddress != nullptr);
  ASSERT(downstreamAddress->type() == Network::Address::Type::Ip);
  auto uri = headers.getPathValue();
  // 添加基础统计信息
  size_t offset = uri.find_first_of("?#");
  std::string path;
  std::string param;
  if (offset != absl::string_view::npos) {
    path = std::string(uri.substr(0, offset));
    param = std::string(uri.substr(offset));
  } else {
    path = std::string(uri);
  }
  log_.set_path(path);
  log_.set_param(param);
  log_.set_downstream(downstreamAddress->ip()->addressAsString());
  log_.set_method(std::string(headers.getMethodValue()));

  const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
  const FilterRouteConfig* filter_vh_config = getVirtualHostConfig(vh);
  if (filter_vh_config != nullptr) {
    if (isWhiteList(filter_vh_config->white_list(), downstreamAddress)) {
      white_list_flag_ = true;
      ENVOY_LOG(info, "white_list: {}", downstreamAddress->ip()->addressAsString());
      return Http::FilterHeadersStatus::Continue;
    }
  }
  // 限速查询，异步响应
  if (filter_vh_config != nullptr) {
    if (filter_vh_config->enable_ratelimit()) {
      ratelimit_filter_->setDecoderFilterCallbacks(*(this->decoder_callbacks_));
      WafStrongGlobalRateLimitFilter* waf_ratelimit_filter =
          dynamic_cast<WafStrongGlobalRateLimitFilter*>(ratelimit_filter_.get());
      ASSERT(waf_ratelimit_filter != nullptr);
      waf_ratelimit_filter->setWafFilter(this);
      waf_ratelimit_filter->setRequestHeaders(&headers);
      waf_ratelimit_filter->setRequestStreamFlag(end_stream);
      ratelimit_filter_->decodeHeaders(headers, end_stream);
      return Http::FilterHeadersStatus::StopAllIterationAndWatermark;
    }
  }
  if (wafRequestHeadersProc(headers, end_stream)) {
    return Http::FilterHeadersStatus::Continue;
  }
  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(debug, "Filter::decodeData");
  // 白名单放行
  if (white_list_flag_) {
    return Http::FilterDataStatus::Continue;
  }
  PROCESS_DECODER_BUFFER_LIMIT(is_decode_buffer_full_);
  decoding_buffer =
      decoder_callbacks_->decodingBuffer() ? decoder_callbacks_->decodingBuffer() : &data;
  ASSERT(decoding_buffer != nullptr);
  if (wafRequestDataProc(*decoding_buffer, end_stream)) {
    return Http::FilterDataStatus::Continue;
  } else {
    if (status_.intervined) {
      return Http::FilterDataStatus::StopIterationNoBuffer;
    }
    return Http::FilterDataStatus::StopIterationAndBuffer;
  }
}

Http::FilterTrailersStatus Filter::decodeTrailers(Http::RequestTrailerMap& trailers) {
  // TODO
  return Http::FilterTrailersStatus::Continue;
}

bool Filter::responseDisabled() {
  const auto route = encoder_callbacks_->route();
  if (route) {
    const auto* route_local =
        dynamic_cast<const FilterRouteConfig*>(route->mostSpecificPerFilterConfig(filter_name));
    return route_local && route_local->disable_response();
  }
  return true;
}

bool Filter::isWhiteList(
    const std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr>& white_list,
    const Network::Address::InstanceConstSharedPtr downstreamAddress) {
  for (auto& ip_whitelist : white_list) {
    if (ip_whitelist->enable() && ip_whitelist->match(downstreamAddress)) {
      return true;
    }
  }
  return false;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(debug, "Filter::encodeHeaders");
  // 白名单放行
  if (white_list_flag_) {
    return Http::FilterHeadersStatus::Continue;
  }
  if (wafResponseHeadersProc(headers, end_stream)) {
    return Http::FilterHeadersStatus::Continue;
  }
  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(debug, "Filter::encodeData");
  // 白名单放行
  if (white_list_flag_) {
    return Http::FilterDataStatus::Continue;
  }
  PROCESS_ENCODER_BUFFER_LIMIT(is_encode_buffer_full_);
  encoding_buffer =
      encoder_callbacks_->encodingBuffer() ? encoder_callbacks_->encodingBuffer() : &data;
  if (wafResponseDataProc(*encoding_buffer, end_stream)) {
    return Http::FilterDataStatus::Continue;
  } else {
    if (modsec_transaction_->m_it.disruptive) {
      return Http::FilterDataStatus::StopIterationNoBuffer;
    }
    return Http::FilterDataStatus::StopIterationAndBuffer;
  }
}

bool Filter::intervention() {
  if (!status_.intervined && modsec_transaction_->m_it.disruptive) {
    // status_.intervined must be set to true before sendLocalReply to avoid reentrancy when
    // encoding the reply
    status_.intervined = true;
    ENVOY_LOG(debug, "intervention");
    // 更新统计信息拒绝标志
    log_.set_refuse(true);
    if (config_->mode() == v3::WafGlobal::performance) {
      auto it = modsec_transaction_->m_rulesMessages.begin();
      while (it != modsec_transaction_->m_rulesMessages.end()) {
        log_.set_filenames(log_.filenames() |
                           Utility::parseRuleType(basename(it->m_ruleFile->c_str())));
        ++it;
      }
    }
    decoder_callbacks_->sendLocalReply(
        static_cast<Http::Code>(modsec_transaction_->m_it.status), "",
        [this](Http::HeaderMap& headers) {
          if (modsec_transaction_->m_it.status == 302) {
            headers.addCopy(Http::Headers::get().Location, modsec_transaction_->m_it.url);
          }
        },
        absl::nullopt, "");
  }
  return status_.intervined;
}

void Filter::setRatelimitVisual() {
  log_.set_ratelimit_refuse(true);
  // 限速配额可视
  // const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
  const StrongGlobalRatelimit::StrongGlobalRateLimitFilterRouteConfig* ratelimit_route_config =
      ratelimit_filter_->getRouteConfig();
  ASSERT(ratelimit_route_config != nullptr);
  Filters::Common::RatelimitClient::Impl::Quota ratelimit_quota =
      ratelimit_route_config->rules()[0]->action().getQuotas()[0];
  v3::Quota* quota_ptr = new v3::Quota();
  quota_ptr->set_duration(ratelimit_quota.duration_);
  quota_ptr->set_max_count(ratelimit_quota.max_count_);
  log_.set_allocated_quota(quota_ptr);
}

void Filter::setRequestHeadPayload(std::pair<absl::string_view, absl::string_view> match_details,
                                   v3::MatchPayload* match_payload) {
  size_t start = (match_details.first).find(modsec_transaction_->m_variableRequestHeaders.m_name);
  if (start != absl::string_view::npos) {
    start += (modsec_transaction_->m_variableRequestHeaders.m_name).length();
    setMatchPayload(match_payload, start + 1, match_details, v3::PayloadStage::REQUESTHEAD);
  } else {
    // 头部cookie有自己的标识
    start = (match_details.first).find(modsec_transaction_->m_variableRequestCookiesNames.m_name);
    if (start != absl::string_view::npos) {
      start += (modsec_transaction_->m_variableRequestCookiesNames.m_name).length();
      setMatchPayload(match_payload, start + 1, match_details, v3::PayloadStage::REQUESTHEAD);
    } else {
      start = (match_details.first).find(modsec_transaction_->m_variableRequestCookies.m_name);
      if (start != absl::string_view::npos) {
        start += (modsec_transaction_->m_variableRequestCookies.m_name).length();
        setMatchPayload(match_payload, start + 1, match_details, v3::PayloadStage::REQUESTHEAD);
      }
    }
  }
}

size_t Filter::getMatchDetailsPos(const std::string& decode_str, absl::string_view match_value,
                                  absl::string_view orignal_info) {
  boost::iterator_range<std::string::const_iterator> it;
  if (modsec_transaction_->m_matched.empty()) {
    it = boost::algorithm::ifind_first(decode_str,
                                       Http::Utility::PercentEncoding::decode(match_value));
  } else {
    it = boost::algorithm::ifind_first(decode_str, std::string(match_value));
    if (it.empty()) {
      // 转换后的报文没有匹配到，从原始报文中去匹配
      absl::string_view value("", 0);
      size_t value_start = orignal_info.find("(Value: `");
      if (value_start != absl::string_view::npos) {
        value_start += sizeof("(Value: `") - 1;
        size_t value_end = orignal_info.rfind("'") - 1;
        value = orignal_info.substr(value_start, value_end - value_start + 1);
        it = boost::algorithm::ifind_first(decode_str,
                                           Http::Utility::PercentEncoding::decode(value));
      }
    }
  }
  if (it) {
    return std::distance(decode_str.begin(), it.begin());
  } else {
    return std::string::npos;
  }
}

std::string Filter::fromHexIfNeeded(const std::string& str) {
  std::stringstream res;
  for (int i = 0; i < str.size(); i++) {
    if (str.at(i) == '\\' && i + 1 < str.size() && str.at(i + 1) == 'x') {
      if (i + 3 < str.size()) {
        std::string hex_byte = str.substr(i + 2, 2);
        unsigned int hex_value;
        std::stringstream ss(hex_byte);
        ss >> std::hex >> hex_value;
        if (!ss.fail()) {
          res << static_cast<unsigned char>(hex_value);
          i += 3;
          continue;
        }
      }
    }
    res << str.at(i);
  }
  return res.str();
}

void Filter::setMatchPayload(v3::MatchPayload* match_payload, size_t start,
                             std::pair<absl::string_view, absl::string_view> match_details,
                             v3::PayloadStage stage) {
  match_payload->set_pos_start(start);
  if (stage == v3::PayloadStage::REQUESTHEAD || stage == v3::PayloadStage::RESPONSEHEAD) {
    match_payload->set_pos_end(match_details.first.length() - 1);
  } else {
    if (modsec_transaction_->m_matched.empty()) {
      match_payload->set_pos_end(
          start + Http::Utility::PercentEncoding::decode(match_details.second).length() - 1);
    } else {
      match_payload->set_pos_end(start + match_details.second.length() - 1);
    }
  }
  switch (stage) {
  case v3::PayloadStage::REQUESTLINE:
    match_payload->set_payload_key("URL");
    break;
  case v3::PayloadStage::REQUESTHEAD:
    if ((match_details.first).find(modsec_transaction_->m_variableRequestCookiesNames.m_name) !=
            std::string::npos ||
        (match_details.first).find(modsec_transaction_->m_variableRequestCookies.m_name) !=
            std::string::npos) {
      match_payload->set_payload_key("cookie");
    } else {
      match_payload->set_payload_key(std::string(match_details.first.substr(start)));
    }
    break;
  case v3::PayloadStage::REQUESTBODY:
    match_payload->set_payload_key("Request Body");
    break;
  case v3::PayloadStage::RESPONSELINE:
    match_payload->set_payload_key("Response Line");
    break;
  case v3::PayloadStage::RESPONSEHEAD:
    match_payload->set_payload_key(std::string(match_details.first.substr(start)));
    break;
  case v3::PayloadStage::RESPONSEBODY:
    match_payload->set_payload_key("Response Body");
    break;
  default:
    break;
  }
  if (modsec_transaction_->m_matched.empty()) {
    match_payload->set_payload_value(Http::Utility::PercentEncoding::decode(match_details.second));
  } else {
    match_payload->set_payload_value(
        std::string(match_details.second.data(), match_details.second.size()));
  }
  match_payload->set_stage(stage);
}

std::pair<absl::string_view, absl::string_view>
Filter::getMatchRuleDetails(absl::string_view match_view) {
  absl::string_view key("", 0);
  absl::string_view value("", 0);
  // 提取key的值
  size_t key_start = match_view.find("against variable");
  if (key_start != absl::string_view::npos) {
    key_start += sizeof("against variable") + 1;
    size_t key_end = match_view.find("'", key_start);
    if (key_end != absl::string_view::npos) {
      key = match_view.substr(key_start, key_end - key_start);
    }
  }
  // 提取value的值
  if (!modsec_transaction_->m_matched.empty()) {
    // pcre匹配，取正则匹配的group0的值
    value = modsec_transaction_->m_matched.front();
  } else {
    size_t value_start = match_view.find("(Value: `");
    if (value_start != absl::string_view::npos) {
      value_start += sizeof("(Value: `") - 1;
      size_t value_end = match_view.rfind("'") - 1;
      value = match_view.substr(value_start, value_end - value_start + 1);
    }
  }
  return std::make_pair(key, value);
}

void Filter::setMatchRuleDetails(const modsecurity::RuleMessage* ruleMessage) {
  size_t start = 0;
  v3::MatchPayload* match_payload = log_.add_match_payloads();
  std::pair<absl::string_view, absl::string_view> match_details =
      getMatchRuleDetails(ruleMessage->m_match);
  switch (block_stage_) {
  case BlockStage::DecodeRequestHeader:
    // 请求行匹配
    start = getMatchDetailsPos(
        Http::Utility::PercentEncoding::decode(modsec_transaction_->m_variableRequestLine.m_value),
        match_details.second, ruleMessage->m_match);
    if (start != std::string::npos) {
      setMatchPayload(match_payload, start, match_details, v3::PayloadStage::REQUESTLINE);
    } else {
      // 请求头匹配,由于请求头存储在map中，顺序已经打乱，位置索引失效，提供头部key-value
      setRequestHeadPayload(match_details, match_payload);
    }
    break;
  case BlockStage::DecodeRequestBody:
    ASSERT(decoding_buffer != nullptr);
    start = getMatchDetailsPos(Http::Utility::PercentEncoding::decode(decoding_buffer->toString()),
                               match_details.second, ruleMessage->m_match);
    if (start != std::string::npos) {
      setMatchPayload(match_payload, start, match_details, v3::PayloadStage::REQUESTBODY);
    } else {
      // 带请求体，但是请求行中带了参数并触发规则
      start = getMatchDetailsPos(Http::Utility::PercentEncoding::decode(
                                     modsec_transaction_->m_variableRequestLine.m_value),
                                 match_details.second, ruleMessage->m_match);
      if (start != std::string::npos) {
        setMatchPayload(match_payload, start, match_details, v3::PayloadStage::REQUESTLINE);
      }
    }
    break;
  case BlockStage::EncodeResponseHeader:
    // 响应头匹配，由于响应头存储在map中，顺序已经打乱，位置索引只匹配头部key
    start = (match_details.first).find(modsec_transaction_->m_variableResponseHeaders.m_name);
    if (start != absl::string_view::npos) {
      start += (modsec_transaction_->m_variableResponseHeaders.m_name).length();
      setMatchPayload(match_payload, start + 1, match_details, v3::PayloadStage::RESPONSEHEAD);
    } else {
      // 响应行仅检测状态码，起始位置跳过协议版本和空格 如HTTP/1.1 503
      start = 5 + (modsec_transaction_->m_variableResponseProtocol.m_value).length() + 1;
      setMatchPayload(match_payload, start, match_details, v3::PayloadStage::RESPONSELINE);
    }
    break;
  case BlockStage::EncodeResponseBody:
    ASSERT(encoding_buffer != nullptr);
    start = getMatchDetailsPos(Http::Utility::PercentEncoding::decode(encoding_buffer->toString()),
                               match_details.second, ruleMessage->m_match);
    if (start != std::string::npos) {
      setMatchPayload(match_payload, start, match_details, v3::PayloadStage::RESPONSEBODY);
    }
    break;
  case BlockStage::NoneBlock:
    break;
  }
}

void Filter::setRuleDetectStatus(const modsecurity::RuleMessage* ruleMessage) {
  if (!config_->getPassRulesId().empty()) {
    const std::set<int64_t> pass_rules_id = config_->getPassRulesId();
    if (pass_rules_id.find(ruleMessage->m_ruleId) != pass_rules_id.end()) {
      log_.add_rule_detect_status(true);
    } else {
      log_.add_rule_detect_status(false);
    }
  } else {
    log_.add_rule_detect_status(false);
  }
}

void Filter::setRuleVisual(const modsecurity::RuleMessage* ruleMessage) {
  // 统计信息添加规则类型
  log_.set_filenames(log_.filenames() |
                     Utility::parseRuleType(basename(ruleMessage->m_ruleFile->c_str())));
  log_.add_rule_id(ruleMessage->m_ruleId);
  // 记录规则状态，防护或检测
  setRuleDetectStatus(ruleMessage);
  // 记录防护模式和等级
  if (config_->detectionOnly()) {
    log_.set_mode(v3::DefendModeration::DETECT);
  } else {
    log_.set_mode(v3::DefendModeration::DEFEND);
  }
  log_.set_paranoia_level(config_->paranoia_level());
  // 记录规则命中的匹配位置
  setMatchRuleDetails(ruleMessage);
}

Http::FilterHeadersStatus Filter::getRequestHeadersStatus() {
  if (status_.intervined) {
    config_->stats().request_processed_.inc();
    ENVOY_LOG(debug, "StopIteration");
    return Http::FilterHeadersStatus::StopIteration;
  }
  if (status_.request_processed) {
    config_->stats().request_processed_.inc();
    ENVOY_LOG(debug, "Continue");
    return Http::FilterHeadersStatus::Continue;
  }
  config_->stats().request_processed_.inc();
  // If disruptive, hold until status_.request_processed, otherwise let the data flow.
  ENVOY_LOG(debug, "RuleEngine");
  return modsec_transaction_->getRuleEngineState() ==
                 modsecurity::RulesSetProperties::EnabledRuleEngine
             ? Http::FilterHeadersStatus::StopIteration
             : Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::getRequestStatus() {
  if (status_.intervined) {
    config_->stats().request_processed_.inc();
    ENVOY_LOG(debug, "StopIterationNoBuffer");
    return Http::FilterDataStatus::StopIterationNoBuffer;
  }
  if (status_.request_processed) {
    config_->stats().request_processed_.inc();
    ENVOY_LOG(debug, "Continue");
    return Http::FilterDataStatus::Continue;
  }
  config_->stats().request_processed_.inc();
  // If disruptive, hold until status_.request_processed, otherwise let the data flow.
  ENVOY_LOG(debug, "RuleEngine");
  return modsec_transaction_->getRuleEngineState() ==
                 modsecurity::RulesSetProperties::EnabledRuleEngine
             ? Http::FilterDataStatus::StopIterationAndBuffer
             : Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus Filter::getResponseHeadersStatus() {
  if (status_.intervined || status_.response_processed) {
    config_->stats().response_processed_.inc();
    // If intervined, let encodeData return the localReply
    ENVOY_LOG(debug, "Continue");
    return Http::FilterHeadersStatus::Continue;
  }
  config_->stats().response_processed_.inc();
  // If disruptive, hold until status_.response_processed, otherwise let the data flow.
  ENVOY_LOG(debug, "RuleEngine");
  return modsec_transaction_->getRuleEngineState() ==
                 modsecurity::RulesSetProperties::EnabledRuleEngine
             ? Http::FilterHeadersStatus::StopIteration
             : Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::getResponseStatus() {
  if (status_.intervined || status_.response_processed) {
    config_->stats().response_processed_.inc();
    // If intervined, let encodeData return the localReply
    ENVOY_LOG(debug, "Continue");
    return Http::FilterDataStatus::Continue;
  }
  config_->stats().response_processed_.inc();
  // If disruptive, hold until status_.response_processed, otherwise let the data flow.
  ENVOY_LOG(debug, "RuleEngine");
  return modsec_transaction_->getRuleEngineState() ==
                 modsecurity::RulesSetProperties::EnabledRuleEngine
             ? Http::FilterDataStatus::StopIterationAndBuffer
             : Http::FilterDataStatus::Continue;
}

void Filter::_logCb(void* data, const void* ruleMessage) {
  auto filter_ = reinterpret_cast<Filter*>(data);

  filter_->logCb(reinterpret_cast<const modsecurity::RuleMessage*>(ruleMessage));
}

void Filter::logCb(const modsecurity::RuleMessage* ruleMessage) {
  if (ruleMessage == nullptr) {
    ENVOY_LOG(error, "ruleMessage == nullptr");
    return;
  }
  setRuleVisual(ruleMessage);
  ENVOY_LOG(info, "Rule Id: {} phase: {}", ruleMessage->m_ruleId, ruleMessage->m_phase);
  ENVOY_LOG(
      info, "* {} action. {}",
      // Note - since ModSecurity >= v3.0.3 disruptive actions do not invoke the callback
      // see
      // https://github.com/SpiderLabs/ModSecurity/commit/91daeee9f6a61b8eda07a3f77fc64bae7c6b7c36
      ruleMessage->m_isDisruptive ? "Disruptive" : "Non-disruptive",
      modsecurity::RuleMessage::log(ruleMessage));

  // config_->invoke_webhook(ruleMessage);
}

} // namespace WafFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy