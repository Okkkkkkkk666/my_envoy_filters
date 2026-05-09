#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/api_check/config.h"
#include "filters/source/extensions/filters/http/api_check/api_check.h"
using testing::_;
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ApiCheckFilter {

namespace v3 = envoy::extensions::filters::http::api_check::v3;

class ApiCheckConfigTest : public testing::Test {
public:
  ApiCheckConfigTest() = default;

  std::shared_ptr<FilterGlobalConfig> makeConfig() {
    v3::ApiCheckGlobal proto;
    return std::make_shared<FilterGlobalConfig>(proto, stats_prefix_, context_);
  }

protected:
  std::string stats_prefix_;
  NiceMock<Server::Configuration::MockFactoryContext> context_;
};

// 验证全局配置是否创建正确
TEST(Factory, GlobalConfig) {
  const std::string yaml = R"(
  )";
  ApiCheckFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();
  NiceMock<Server::Configuration::MockFactoryContext> context;
  Http::FilterFactoryCb callback =
      factory.createFilterFactoryFromProto(*proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  callback(filter_callback);
}

// 验证api_check的各项配置
TEST_F(ApiCheckConfigTest, GlobalApiCheckConfig) {
  auto config = makeConfig();
  EXPECT_EQ(config->waf_global_config()->paranoia_level(), 3);
  EXPECT_EQ(config->waf_global_config()->ruleTypes(), 134217727);
  EXPECT_EQ(config->waf_global_config()->detectionOnly(), true);
  EXPECT_EQ(config->waf_global_config()->mode(), 1);
}

} // namespace ApiCheckFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
