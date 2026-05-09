#include "identify_rule_interface.h"
#include "identify_rule.h"
#include <string>
#include <cstring>
#include <sstream>
#include <vector>

namespace Replaces = Envoy::Extensions::Filters::Common::Replaces;

extern "C" {

void* identifyRule_New(identify_rule_t rule) {
  Replaces::identify_rule cpp_rule;
  cpp_rule.identify_type = static_cast<Replaces::IdentifyType>(rule.identify_type);
  cpp_rule.recognition_logic = static_cast<Replaces::RecognitionLogic>(rule.recognition_logic);
  cpp_rule.field_name = rule.field_name;
  cpp_rule.data_content = rule.data_content;
  return new Replaces::IdentifyRule(cpp_rule);
}

void identifyRule_Delete(void* instance) { delete static_cast<Replaces::IdentifyRule*>(instance); }

void identifyRule_FreeResult(const char* result) {
  if (result != nullptr) {
    delete[] result;
  }
}

const char* identifyRule_Rewrite(void* instance, const char* data) {
  if (!instance) {
    fprintf(stderr, "IdentifyRule Invalid instance\n");
    return NULL;
  }
  if (!data) {
    fprintf(stderr, "IdentifyRule Invalid data\n");
    return NULL;
  }
  std::vector<std::string> result;

  auto* identifyRule = static_cast<Replaces::IdentifyRule*>(instance);
  result = identifyRule->matchALL(std::string(data));

  std::string final_result;
  std::stringstream ss;

  ss << "共找到" << result.size() << "处匹配";
  if (result.empty()) {
    final_result = ss.str();
    char* res = new char[final_result.size() + 1];
    std::strcpy(res, final_result.c_str());
    return res;
  }
  ss << "\n";
  for (size_t i = 0; i < result.size(); ++i) {
    ss << result[i];
    if (i < result.size() - 1) {
      ss << "\n";
    }
  }

  final_result = ss.str();
  char* res = new char[final_result.size() + 1];
  std::strcpy(res, final_result.c_str());
  return res;
}
} // extern "C"
