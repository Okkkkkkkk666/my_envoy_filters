#pragma once

#include <string>
#include <regex>

namespace SrhinoPluginFramework {
class FileNameMatcher {
public:
  static bool match(const std::string& file_name);
  static bool match(const std::string& file_name, std::string& directory, std::string& plugin_name,
                    std::string& version);
  static bool matchInstanceName(const std::string& name);
  static uint32_t version(const std::string& version);

private:
  const static std::regex file_name_regex_;
  const static std::regex instance_name_regex_;
  const static std::regex version_regex_;
};
} // namespace SrhinoPluginFramework