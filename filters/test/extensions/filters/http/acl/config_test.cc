#include "test/mocks/server/factory_context.h"
#include "test/mocks/server/instance.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/api/envoy/extensions/filters/http/acl/v3/acl.pb.h"
#include "filters/api/envoy/extensions/filters/http/acl/v3/acl.pb.validate.h"
#include "filters/source/extensions/filters/http/acl/acl_filter.h"
#include "filters/source/extensions/filters/http/acl/config.h"

using testing::_;
using testing::NiceMock;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AclFilter {
namespace {

TEST(AclFilterConfigFactoryTest, AclFilterCorrectYaml) {
  envoy::extensions::filters::http::acl::v3::Acl proto_config;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  AclFilterConfigFactory factory;
  Http::FilterFactoryCb cb = factory.createFilterFactoryFromProto(proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamDecoderFilter(_));
  cb(filter_callback);
}

TEST(AclFilterConfigFactoryTest, AclFilterCorrectProto) {
  envoy::extensions::filters::http::acl::v3::Acl config;

  NiceMock<Server::Configuration::MockFactoryContext> context;
  AclFilterConfigFactory factory;
  Http::FilterFactoryCb cb = factory.createFilterFactoryFromProto(config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamDecoderFilter(_));
  cb(filter_callback);
}

TEST(AclFilterConfigFactoryTest, AclFilterEmptyProto) {
  AclFilterConfigFactory factory;
  auto empty_proto = factory.createEmptyConfigProto();
  envoy::extensions::filters::http::acl::v3::Acl config =
      *dynamic_cast<envoy::extensions::filters::http::acl::v3::Acl*>(empty_proto.get());

  NiceMock<Server::Configuration::MockFactoryContext> context;
  Http::FilterFactoryCb cb = factory.createFilterFactoryFromProto(config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamDecoderFilter(_));
  cb(filter_callback);
}

// TEST(AclFilterConfigFactoryTest, AclFilterNoId) {
//   AclFilterConfigFactory factory;
//   auto empty_proto = factory.createEmptyConfigProto();
//   envoy::extensions::filters::http::acl::v3::Acl config =
//       *dynamic_cast<envoy::extensions::filters::http::acl::v3::Acl*>(empty_proto.get());

//   NiceMock<Server::Configuration::MockFactoryContext> context;
//   EXPECT_THROW_WITH_REGEX(factory.createFilterFactoryFromProto(config, "stats", context),
//                           EnvoyException, "Proto constraint validation failed");
// }

TEST(AclFilterConfigFactoryTest, AclFilterEmptyRouteProto) {
  AclFilterConfigFactory factory;
  EXPECT_NO_THROW({
    EXPECT_NE(nullptr, dynamic_cast<envoy::extensions::filters::http::acl::v3::AclPerRoute*>(
                           factory.createEmptyRouteConfigProto().get()));
  });
}

TEST(AclFilterConfigFactoryTest, AclFilterRouteSpecificConfig) {
  AclFilterConfigFactory factory;
  NiceMock<Server::Configuration::MockServerFactoryContext> factory_context;

  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  EXPECT_TRUE(proto_config.get());

  Router::RouteSpecificFilterConfigConstSharedPtr route_config =
      factory.createRouteSpecificFilterConfig(*proto_config, factory_context,
                                              ProtobufMessage::getNullValidationVisitor());
  EXPECT_TRUE(route_config.get());

  const auto* inflated = dynamic_cast<const AclFilterRouteConfig*>(route_config.get());
  EXPECT_TRUE(inflated);
}


} // namespace
} // namespace AclFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
