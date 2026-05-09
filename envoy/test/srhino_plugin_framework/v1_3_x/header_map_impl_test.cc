#include "envoy/registry/registry.h"

#include "source/common/srhino_plugin_framework/v1_3_x/header_map_impl.h"

#include "gmock/gmock.h"

namespace TestSrhinoPluginFrameWork {
namespace v1_3_x {
namespace {

TEST(HeaderMapImplTest, get) {
  SrhinoPluginFramework::v1_3_x::HeaderMapImpl header;
  std::vector<std::string_view> values;
  header.get("content-type", values);
  EXPECT_TRUE(values.empty());
}

} // namespace
} // namespace v1_3_x
} // namespace TestSrhinoPluginFrameWork
