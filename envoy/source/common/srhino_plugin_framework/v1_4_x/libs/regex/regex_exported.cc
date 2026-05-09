#include "source/common/srhino_plugin_framework/v1_4_x/libs/regex/regex_impl.h"
#include <iostream>

extern const char build_scm_revision[];
extern const char build_scm_status[];
const char build_scm_revision[] = "";
const char build_scm_status[] = "";

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {

extern "C" {

#define EXPORT __attribute__((visibility("default")))
EXPORT void* createRegexImpl() {
  std::cout << "regex success" << std::endl;
  return new Regex::RegexImpl();
}
}

} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework