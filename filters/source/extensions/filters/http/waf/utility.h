#pragma once

#include <string>
#include <vector>
#include <set>
#include "modsecurity/modsecurity.h"
#include "modsecurity/rule_message.h"

#include "filters/api/envoy/extensions/filters/http/waf/v3/waf.pb.h"

#define RULE_FILE_MAP(FIELD)                                                                       \
  FIELD(v3::WafGlobal::RT_ScannerDetection, "REQUEST-913-SCANNER-DETECTION.conf")                  \
  FIELD(v3::WafGlobal::RT_ProtocolEnforcement, "REQUEST-920-PROTOCOL-ENFORCEMENT.conf")            \
  FIELD(v3::WafGlobal::RT_ProtocolAttack, "REQUEST-921-PROTOCOL-ATTACK.conf")                      \
  FIELD(v3::WafGlobal::RT_MultipartAttack, "REQUEST-922-MULTIPART-ATTACK.conf")                    \
  FIELD(v3::WafGlobal::RT_ApplicationAttackLfi, "REQUEST-930-APPLICATION-ATTACK-LFI.conf")         \
  FIELD(v3::WafGlobal::RT_ApplicationAttackRfi, "REQUEST-931-APPLICATION-ATTACK-RFI.conf")         \
  FIELD(v3::WafGlobal::RT_ApplicationAttackRce, "REQUEST-932-APPLICATION-ATTACK-RCE.conf")         \
  FIELD(v3::WafGlobal::RT_ApplicationAttackPhp, "REQUEST-933-APPLICATION-ATTACK-PHP.conf")         \
  FIELD(v3::WafGlobal::RT_ApplicationAttackGeneric, "REQUEST-934-APPLICATION-ATTACK-GENERIC.conf") \
  FIELD(v3::WafGlobal::RT_ApplicationAttackXss, "REQUEST-941-APPLICATION-ATTACK-XSS.conf")         \
  FIELD(v3::WafGlobal::RT_ApplicationAttackSqli, "REQUEST-942-APPLICATION-ATTACK-SQLI.conf")       \
  FIELD(v3::WafGlobal::RT_ApplicationAttackSessionFixation,                                        \
        "REQUEST-943-APPLICATION-ATTACK-SESSION-FIXATION.conf")                                    \
  FIELD(v3::WafGlobal::RT_ApplicationAttackJava, "REQUEST-944-APPLICATION-ATTACK-JAVA.conf")       \
  FIELD(v3::WafGlobal::RT_BlockingEvaluation, "REQUEST-949-BLOCKING-EVALUATION.conf")              \
  FIELD(v3::WafGlobal::RT_DataLeakages, "RESPONSE-950-DATA-LEAKAGES.conf")                         \
  FIELD(v3::WafGlobal::RT_DataLeakagesSql, "RESPONSE-951-DATA-LEAKAGES-SQL.conf")                  \
  FIELD(v3::WafGlobal::RT_DataLeakagesJava, "RESPONSE-952-DATA-LEAKAGES-JAVA.conf")                \
  FIELD(v3::WafGlobal::RT_DataLeakagesPhp, "RESPONSE-953-DATA-LEAKAGES-PHP.conf")                  \
  FIELD(v3::WafGlobal::RT_DataLeakagesIis, "RESPONSE-954-DATA-LEAKAGES-IIS.conf")                  \
  FIELD(v3::WafGlobal::RT_WebShells, "RESPONSE-955-WEB-SHELLS.conf")                               \
  FIELD(v3::WafGlobal::RT_APILEAK, "apis_leak.conf")                                               \
  FIELD(v3::WafGlobal::RT_BACKUPLEAK, "backups_leak.conf")                                         \
  FIELD(v3::WafGlobal::RT_CONFIGLEAK, "configs_leak.conf")                                         \
  FIELD(v3::WafGlobal::RT_FILELEAK, "files_leak.conf")                                             \
  FIELD(v3::WafGlobal::RT_LOGLEAK, "logs_leak.conf")                                               \
  FIELD(v3::WafGlobal::RT_PANELLEAK, "panels_leak.conf")                                           \
  FIELD(v3::WafGlobal::RT_TOKENLEAK, "tokens_leak.conf")                                           \
  FIELD(v3::WafGlobal::RT_CRS_SETUP, "crs-setup.conf")                                             \
  FIELD(v3::WafGlobal::RT_ENGINE_SETUP, "engin-setup.conf")

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WafFilter {

namespace v3 = envoy::extensions::filters::http::waf::v3;

class Utility {
public:
  /**
   * Converts a RuleMessage to json
   * @return A json string
   */
  static std::string getRuleMessageAsJsonString(const modsecurity::RuleMessage* ruleMessage);

  static void traverseRuleFiles(u_int64_t flags, std::function<void(v3::WafGlobal::RuleType)> cb);
  static std::string_view parseRuleFileName(v3::WafGlobal::RuleType rule_type);
  static v3::WafGlobal::RuleType parseRuleType(const std::string& file_name);

private:
  static std::unordered_map<u_int64_t, std::string> initRuleFileMap() {
    std::unordered_map<u_int64_t, std::string> rule_file_map;
#define FIELD(key, value) rule_file_map[key] = value;
    RULE_FILE_MAP(FIELD);
#undef FIELD
    return rule_file_map;
  }

  static std::unordered_map<std::string, u_int64_t> initRuleFileInverseMap() {
    std::unordered_map<std::string, u_int64_t> rule_file_inverse_map;
#define FIELD(key, value) rule_file_inverse_map[value] = key;
    RULE_FILE_MAP(FIELD);
#undef FIELD
    return rule_file_inverse_map;
  }

private:
  static const std::unordered_map<u_int64_t, std::string> rule_file_map_;
  static const std::unordered_map<std::string, u_int64_t> rule_file_inverse_map_;
};
} // namespace WafFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

#undef RULE_FILE_MAP
