#include "envoy/registry/registry.h"

#include "source/common/srhino_plugin_framework/file_name_matcher.h"

#include "gmock/gmock.h"

namespace TestSrhinoPluginFrameWork {
namespace {

TEST(FileNameMatcherTest, match) {
  {
    std::string file_name = "wwww.srhino.com.weak_password_check.so.1.0.0";
    EXPECT_TRUE(SrhinoPluginFramework::FileNameMatcher::match(file_name));
  }

  {
    std::string file_name = "wwww.com.weak_password_check.so.1.0.0";
    EXPECT_FALSE(SrhinoPluginFramework::FileNameMatcher::match(file_name));
  }

  {
    std::string file_name = "wwww.srhino.com.weak_password_check.so.1.0";
    EXPECT_FALSE(SrhinoPluginFramework::FileNameMatcher::match(file_name));
  }
}

TEST(FileNameMatcherTest, version) {
  EXPECT_EQ(SrhinoPluginFramework::FileNameMatcher::version("1.0.0"), 1 << 28);
  EXPECT_EQ(SrhinoPluginFramework::FileNameMatcher::version("2.1.3"), (2 << 28) + (1 << 14) + 3);
}

} // namespace
} // namespace TestSrhinoPluginFramework
