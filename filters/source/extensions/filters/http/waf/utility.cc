#include "utility.h"

#include "source/common/protobuf/protobuf.h"
#include "source/common/protobuf/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WafFilter {

const std::unordered_map<u_int64_t, std::string>
    Utility::rule_file_map_(Utility::initRuleFileMap());
const std::unordered_map<std::string, u_int64_t>
    Utility::rule_file_inverse_map_(Utility::initRuleFileInverseMap());

std::string Utility::getRuleMessageAsJsonString(const modsecurity::RuleMessage* ruleMessage) {
  ProtobufWkt::Struct document;
  auto* document_fields = document.mutable_fields();
  (*document_fields)["accuracy"] = ValueUtil::numberValue(ruleMessage->m_accuracy);
  (*document_fields)["clientIpAddress"] = ValueUtil::stringValue(*ruleMessage->m_clientIpAddress);
  (*document_fields)["data"] = ValueUtil::stringValue(ruleMessage->m_data);
  (*document_fields)["id"] = ValueUtil::stringValue(*ruleMessage->m_id);
  (*document_fields)["isDisruptive"] = ValueUtil::boolValue(ruleMessage->m_isDisruptive);
  (*document_fields)["match"] = ValueUtil::stringValue(ruleMessage->m_match);
  (*document_fields)["maturity"] = ValueUtil::numberValue(ruleMessage->m_maturity);
  (*document_fields)["message"] = ValueUtil::stringValue(ruleMessage->m_message);
  (*document_fields)["noAuditLog"] = ValueUtil::boolValue(ruleMessage->m_noAuditLog);
  (*document_fields)["phase"] = ValueUtil::numberValue(ruleMessage->m_phase);
  (*document_fields)["reference"] = ValueUtil::stringValue(ruleMessage->m_reference);
  (*document_fields)["rev"] = ValueUtil::stringValue(ruleMessage->m_rev);
  (*document_fields)["ruleFile"] = ValueUtil::stringValue(*ruleMessage->m_ruleFile);
  (*document_fields)["ruleId"] = ValueUtil::numberValue(ruleMessage->m_ruleId);
  (*document_fields)["ruleLine"] = ValueUtil::numberValue(ruleMessage->m_ruleLine);
  (*document_fields)["saveMessage"] = ValueUtil::boolValue(ruleMessage->m_saveMessage);
  (*document_fields)["serverIpAddress"] = ValueUtil::stringValue(*ruleMessage->m_serverIpAddress);
  (*document_fields)["severity"] = ValueUtil::numberValue(ruleMessage->m_severity);
  (*document_fields)["uriNoQueryStringDecoded"] =
      ValueUtil::stringValue(*ruleMessage->m_uriNoQueryStringDecoded);
  (*document_fields)["ver"] = ValueUtil::stringValue(ruleMessage->m_ver);
  std::vector<ProtobufWkt::Value> tag_array;
  for (const auto& tag : ruleMessage->m_tags) {
    tag_array.push_back(ValueUtil::stringValue(tag));
  }
  (*document_fields)["tags"] = ValueUtil::listValue(tag_array);
  return MessageUtil::getJsonStringFromMessageOrError(document);
}

void Utility::traverseRuleFiles(u_int64_t flags, std::function<void(v3::WafGlobal::RuleType)> cb) {
  for (u_int64_t i = 0; i < sizeof(flags) * 8 /*按位遍历*/; ++i) {
    // 按位设置测试标志
    u_int64_t flag = 1UL << i;

    // 测试位标志
    if (flags & flag) {
      auto iter = rule_file_map_.find(flag);
      RELEASE_ASSERT(iter != rule_file_map_.end(), "Rule file map invalid!");

      cb(static_cast<v3::WafGlobal::RuleType>(flag));
    }
  }
}

std::string_view Utility::parseRuleFileName(v3::WafGlobal::RuleType rule_type) {
  std::string_view file_name;

  auto iter = rule_file_map_.find(rule_type);
  ASSERT(iter != rule_file_map_.end(), "Rule file map invalid!");
  if (iter != rule_file_map_.end()) {
    file_name = iter->second;
  }

  return file_name;
}

v3::WafGlobal::RuleType Utility::parseRuleType(const std::string& file_name) {
  v3::WafGlobal::RuleType rule_type = v3::WafGlobal::RuleType::WafGlobal_RuleType_RT_Placeholder;

  auto iter = rule_file_inverse_map_.find(file_name);
  ASSERT(iter != rule_file_inverse_map_.end(), "Rule file inverse map invalid!");
  if (iter != rule_file_inverse_map_.end()) {
    rule_type = static_cast<v3::WafGlobal::RuleType>(iter->second);
  }

  return rule_type;
}
} // namespace WafFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy