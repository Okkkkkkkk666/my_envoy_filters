#include "test/mocks/server/mocks.h"
#include "test/mocks/server/instance.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/api/mocks.h"
#include "test/test_common/utility.h"
#include "test/test_common/environment.h"
#include "filters/source/extensions/filters/http/super_glue/super_glue_client.h"
#include "filters/source/extensions/filters/http/super_glue/client_config.h"
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

namespace v3 = envoy::extensions::filters::http::super_glue::v3;

TEST(Factory, RuleIsMatch) {
  const std::string yaml = R"(
        rules:
        - rule_name: "rule1"
          enable: true
          matchers:
          - prefix: /
        - rule_name: "rule2"
          enable: false
          matchers:
          - prefix: /
        percent:
          numerator: 100
          denominator: HUNDRED
    )";
  v3::SuperGlueClientRoute route_config;
  TestUtility::loadFromYamlAndValidate(yaml, route_config);
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  auto config = std::make_shared<ClientFilterRouteConfig>(route_config, context_);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(config->rules().size(), 1);
  EXPECT_EQ(config->rules()[0]->ruleName(), "rule1");
  EXPECT_TRUE(config->rules()[0]->match("xiaoming", nullptr, headers));
}

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
