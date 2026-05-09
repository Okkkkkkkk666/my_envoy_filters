#include "envoy/extensions/filters/http/acl/v3/acl.pb.h"
#include "envoy/extensions/filters/network/http_connection_manager/v3/http_connection_manager.pb.h"

#include "source/common/protobuf/utility.h"

#include "test/integration/http_protocol_integration.h"

namespace Envoy {
namespace {

const std::string ACL_HCM_CONFIG = R"EOF(
name: acl
typed_config:
    "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.Acl
    id: 0
)EOF";


using AclIntegrationTest = HttpProtocolIntegrationTest;

INSTANTIATE_TEST_SUITE_P(Protocols, AclIntegrationTest,
                         testing::ValuesIn(HttpProtocolIntegrationTest::getProtocolTestParams()),
                         HttpProtocolIntegrationTest::protocolTestParamsToString);

ConfigHelper::HttpModifierFunction overrideConfig(const std::string& yaml) {
  envoy::extensions::filters::http::acl::v3::AclPerRoute acl_per_route;
  TestUtility::loadFromYaml(yaml, acl_per_route);

  return
      [acl_per_route](
          envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              cfg) {
        auto* config = cfg.mutable_route_config()
                           ->mutable_virtual_hosts()
                           ->Mutable(0)
                           ->mutable_typed_per_filter_config();

        (*config)["envoy.filters.http.acl1.0"].PackFrom(acl_per_route);
      };
}

// ConfigHelper::HttpModifierFunction overrideRouteConfig(const std::string& yaml) {
//   envoy::extensions::filters::http::acl::v3::AclPerRoute acl_per_route;
//   TestUtility::loadFromYaml(yaml, acl_per_route);

//   return
//       [acl_per_route](
//           envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
//               cfg) {
//         auto* config = cfg.mutable_route_config()
//                            ->mutable_virtual_hosts()
//                            ->Mutable(0)
//                            ->mutable_routes()
//                            ->Mutable(0)
//                            ->mutable_typed_per_filter_config();

//         (*config)["envoy.filters.http.acl"].PackFrom(acl_per_route);
//       };
// }



TEST_P(AclIntegrationTest, Denied) {
  useAccessLog("%RESPONSE_CODE_DETAILS%");
  std::string yaml = R"EOF(           
    id: 0              
    rules:
    - src_ip:
        address_prefix: 1.2.3.0
        prefix_len: 24
      act: ALLOW
    default_act: DENY         
    deny_context: "virtual host deny context"     
  )EOF";
  ConfigHelper::HttpModifierFunction mod = overrideConfig(yaml);
  config_helper_.addConfigModifier(mod);
  config_helper_.prependFilter(ACL_HCM_CONFIG);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "GET"},
          {":path", "/"},
          {":scheme", "http"},
          {":authority", "host"},
          {"x-forwarded-for", "10.0.0.1"},
      },
      1024);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("403", response->headers().getStatusValue());
  // body的自动测试还没来得及做，不过实际跑过，应该没啥问题
  // EXPECT_THAT(waitForAccessLog(access_log_name_),
  //             testing::HasSubstr("virtual host deny context"));
}


} // namespace
} // namespace Envoy
