#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <iostream>
#include "filters/source/extensions/filters/http/strong_stateful_session/config.h"
#include "filters/source/extensions/filters/http/strong_stateful_session/filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {

namespace v3 = envoy::extensions::filters::http::strong_stateful_session::v3;

// 验证配置是否正确
TEST(StrongStatefulSessionConfigTest, GlobalConfig) {
  const std::vector<std::string> yamls = {
    // src_ip
    R"(
      src_ip:
        prefix_len: 32
      ttl: 5s
    )",
    // ssl session
    R"(
      ssl_session: {}
      ttl: 5s
    )",
    // http header
    R"(
      header:
        name: lb-rhino
      ttl: 5s
    )",
    // cookie insert
    R"(
      cookie:
        insert: true
        session: true
        name: lb-rhino
        domain: ouryun.com
        path: /
      ttl: 5s
    )",
    // cookie rewrite
    R"(
      cookie:
        rewrite: true
        session: true
        name: lb-rhino
        domain: ouryun.com
        path: /
      ttl: 5s
    )",
    // cookie read
    R"(
      cookie:
        read: true
        session: true
        name: lb-rhino
        domain: ouryun.com
        path: /
      ttl: 5s
    )"
  };

  for (auto yaml : yamls) {
    v3::StrongStatefulSessionGlobal proto;
    TestUtility::loadFromYamlAndValidate(yaml, proto);
    NiceMock<Event::MockDispatcher> dispatcher_;
    NiceMock<Stats::MockIsolatedStatsStore> stats_;
    NiceMock<Server::Configuration::MockFactoryContext> grpc_context_;
    auto config = std::make_shared<FilterGlobalConfig>(proto, grpc_context_);
  }
}

} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy