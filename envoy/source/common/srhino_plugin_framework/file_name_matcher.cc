#include "source/common/srhino_plugin_framework/file_name_matcher.h"

namespace SrhinoPluginFramework {
/**
 *E.g. "www.srhino.com.acl.so.1.0.0"
 *      ------┬-------  |     | | |
 *          domain      |     | | \_ patch
 *                     name   | \_ minor
 *                            \_ major
 */
const std::regex
    FileNameMatcher::file_name_regex_(R"(^(.+\..+\..+)\.(.+)\.(S|s)(O|o)\.(\d+\.\d+\.\d+)$)");

const std::regex FileNameMatcher::version_regex_(R"(^(\d+)\.(\d+)\.(\d+)$)");

const std::regex
    FileNameMatcher::instance_name_regex_(R"(^(.+\..+\..+)\.(.+)\.(\d+\.\d+\.\d+)(.+)$)");

bool FileNameMatcher::match(const std::string& name) {
  return std::regex_match(name, file_name_regex_);
}

bool FileNameMatcher::matchInstanceName(const std::string& name) {
  return std::regex_match(name, instance_name_regex_);
}

bool FileNameMatcher::match(const std::string& name, std::string& directory,
                            std::string& plugin_name, std::string& version) {
  std::smatch sm;
  if (std::regex_match(name, sm, file_name_regex_)) {
    directory = sm.str(1);
    plugin_name = sm.str(2);
    version = sm.str(5);
    return true;
  }
  return false;
}

/**
 * E.g. "1.0.0"
 *       | | |
 *       | | \____________________________________________________________________________________
 *       | \____________________________________________                                          \
 *       \____                                          \                                         |
 *            \                                         |                                         |
 *            |                                         |                                         |
 * +-----------+-----------------------------------------+-----------------------------------------+
 * |31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
 * +-----------+-----------------------------------------+-----------------------------------------+
 */
uint32_t FileNameMatcher::version(const std::string& version) {
  int32_t ver = 0;
  std::smatch sm;
  if (std::regex_match(version, sm, version_regex_)) {
    int32_t major = std::atoi(sm.str(1).c_str());
    int32_t minor = std::atoi(sm.str(2).c_str());
    int32_t patch = std::atoi(sm.str(3).c_str());
    ver |= major << 28;
    ver |= minor << 14;
    ver |= patch;
  }

  return ver;
}
} // namespace SrhinoPluginFramework