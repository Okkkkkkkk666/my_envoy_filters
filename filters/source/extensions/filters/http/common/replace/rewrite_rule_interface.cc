
#include "rewrite_rule_interface.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "rewrite_rule.h"
#include "transform.h"

namespace Replaces = Envoy::Extensions::Filters::Common::Replaces;
using RewriteMethod = Replaces::rewrite_method;

extern "C" {

void initEncryptAlgorithm(Replaces::rewrite_rule& cpp_rule, rewrite_rule_t& rule) {
  cpp_rule.method = static_cast<RewriteMethod>(RewriteMethod::EncryptAlgorithmMethod);
  cpp_rule.encrpyt_algorithm.encryption_algorithm =
      static_cast<Replaces::EncryptionAlgorithm>(rule.encrpyt_algorithm.encryption_algorithm);
  if (rule.encrpyt_algorithm.encryption_key) {
    cpp_rule.encrpyt_algorithm.encryption_key = std::string(rule.encrpyt_algorithm.encryption_key);
  } else {
    cpp_rule.encrpyt_algorithm.encryption_key.clear();
  }
}

void initHashEncrypt(Replaces::rewrite_rule& cpp_rule, rewrite_rule_t& rule) {
  cpp_rule.method = static_cast<RewriteMethod>(RewriteMethod::HashEncryptMethod);
  cpp_rule.hash_encrpyt.encryption_algorithm =
      static_cast<Replaces::HashEncryptionAlgorithm>(rule.hash_encrpyt.encryption_algorithm);
  cpp_rule.hash_encrpyt.salt_value = rule.hash_encrpyt.salt_value;
  if (rule.hash_encrpyt.salt_value) {
    cpp_rule.hash_encrpyt.salt_value = std::string(rule.hash_encrpyt.salt_value);
  } else {
    cpp_rule.hash_encrpyt.salt_value.clear();
  }
}

void initCover(Replaces::rewrite_rule& cpp_rule, rewrite_rule_t& rule) {
  cpp_rule.method = static_cast<RewriteMethod>(RewriteMethod::CoverMethod);
  cpp_rule.cover.cover_mode = static_cast<Replaces::CoverMode>(rule.cover.cover_mode);
  cpp_rule.cover.cover_type = static_cast<Replaces::CoverType>(rule.cover.cover_type);
  for (int i = 0; i < rule.cover.rules_size; ++i) {
    Replaces::CoverRule cover_rule;
    cover_rule.start = rule.cover.rules[i].start;
    cover_rule.end = rule.cover.rules[i].end;
    cpp_rule.cover.rules.emplace_back(cover_rule);
  }
  if (rule.cover.cover_character) {
    cpp_rule.cover.cover_character = std::string(rule.cover.cover_character);
  } else {
    cpp_rule.cover.cover_character.clear();
  }
}

void initReplace(Replaces::rewrite_rule& cpp_rule, rewrite_rule_t& rule) {
  cpp_rule.method = static_cast<RewriteMethod>(RewriteMethod::ReplaceMethod);
  cpp_rule.replace.value_type = static_cast<Replaces::ValueType>(rule.replace.value_type);
  cpp_rule.replace.rule_type = static_cast<Replaces::RuleType>(rule.replace.rule_type);
  if (rule.replace.replace_value) {
    cpp_rule.replace.replace_value = std::string(rule.replace.replace_value);
  } else {
    cpp_rule.replace.replace_value.clear();
  }

  if (rule.replace.rule_type == Replaces::RuleType::REGEX_MATCH) { // 正则匹配
    if (rule.replace.regex) {
      cpp_rule.replace.regex_match.regex = std::string(rule.replace.regex);
    } else {
      cpp_rule.replace.regex_match.regex.clear();
    }
  } else if (rule.replace.rule_type == Replaces::RuleType::REPLACE_CUSTOM) { // 自定义
    for (int i = 0; i < rule.replace.rules_size; ++i) {
      Replaces::ReplaceRuleDefinition replace_rule;
      replace_rule.start = rule.replace.rules[i].start;
      replace_rule.end = rule.replace.rules[i].end;
      cpp_rule.replace.custom.rules.emplace_back(replace_rule);
    }
    cpp_rule.replace.custom.cover_type =
        static_cast<Replaces::ReplaceCoverType>(rule.replace.cover_type);
  }
}

void initTransform(Replaces::rewrite_rule& cpp_rule, rewrite_rule_t& rule) {
  cpp_rule.method = static_cast<RewriteMethod>(RewriteMethod::TransformMethod);
  cpp_rule.transform.type = static_cast<Replaces::TransformType>(rule.transform.trans_type);
  switch (rule.transform.trans_type) {
  case Replaces::TransformType::NUMERIC_ROUND: // 数字取整
    cpp_rule.transform.numeric_round.decimal_places = rule.transform.decimal_places;
    break;
  case Replaces::TransformType::DATE_ROUND: // 日期取整
    cpp_rule.transform.date_round.level =
        static_cast<Replaces::DateRoundLevel>(rule.transform.level);
    break;
  case Replaces::TransformType::CHARACTER_SHIFT: // 字符位移
    cpp_rule.transform.character_shift.direction =
        static_cast<Replaces::ShiftDirection>(rule.transform.direction);
    cpp_rule.transform.character_shift.shift_amount = rule.transform.shift_amount;
    break;
  }
}

void* rewriteRule_New(rewrite_rule_t rule) {
  if (rule.method < 0) {
    fprintf(stderr, "Invalid method: %d\n", rule.method);
    return nullptr;
  }
  Replaces::rewrite_rule cpp_rule;

  switch (rule.method) {
  case RewriteMethod::EncryptAlgorithmMethod:
    // 加密算法
    initEncryptAlgorithm(cpp_rule, rule);
    break;
  case RewriteMethod::HashEncryptMethod:
    // 哈希加密
    initHashEncrypt(cpp_rule, rule);
    break;
  case RewriteMethod::CoverMethod:
    // 遮盖
    initCover(cpp_rule, rule);
    break;
  case RewriteMethod::ReplaceMethod:
    // 替换
    initReplace(cpp_rule, rule);
    break;
  case RewriteMethod::TransformMethod:
    // 变换
    initTransform(cpp_rule, rule);
    break;
  case RewriteMethod::ShuffleMethod:
    // 洗牌
    cpp_rule.method = static_cast<RewriteMethod>(RewriteMethod::ShuffleMethod);
    break;
  default:
    fprintf(stderr, "Unknown method: %d\n", rule.method);
    return nullptr;
  }

  return new Replaces::Rewrite(cpp_rule);
}

void rewriteRule_Delete(void* instance) {
  if (instance) {
    delete static_cast<Replaces::Rewrite*>(instance);
  }
}

void rewriteRule_FreeResult(const char* result) {
  if (result != nullptr) {
    delete[] result;
  }
}

const char* rewriteRule_Rewrite(void* instance, const char* data) {
  try {
    if (!instance) {
      fprintf(stderr, "RewriteRule Invalid instance\n");
      return nullptr;
    }
    if (!data) {
      fprintf(stderr, "RewriteRule Invalid data\n");
      return nullptr;
    }
    auto* rewriteRule = static_cast<Replaces::Rewrite*>(instance);
    std::string result = rewriteRule->rewrite(std::string(data));
    char* cResult = new char[result.length() + 1];
    std::strcpy(cResult, result.c_str());
    return cResult;
  } catch (const std::exception& e) {
    fprintf(stderr, "Exception: %s\n", e.what());
    return nullptr;
  } catch (...) {
    fprintf(stderr, "Unknown exception\n");
    return nullptr;
  }
}
} // extern "C"