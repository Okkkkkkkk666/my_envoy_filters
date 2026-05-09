#include "contrib/smtp_proxy/smtp_config.h"
#include "contrib/smtp_proxy/smtp_filter.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/server/factory_context.h"

#include "contrib/envoy/extensions/filters/network/smtp_proxy/smtp_proxy.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {
namespace {

using SmtpProxyProto = envoy::extensions::filters::network::smtp_proxy::v3::SmtpProxy;

TEST(SmtpConfigTest, UsesDefaultMaxLineLengthWhenConfiguredAsZero) {
  auto config = std::make_shared<SmtpConfig>("smtp", 0);

  EXPECT_EQ(config->statPrefix(), "smtp");
  EXPECT_EQ(config->maxLineLength(), 1024);
}

TEST(SmtpConfigTest, KeepsConfiguredMaxLineLength) {
  auto config = std::make_shared<SmtpConfig>("smtp", 2048);

  EXPECT_EQ(config->statPrefix(), "smtp");
  EXPECT_EQ(config->maxLineLength(), 2048);
}

TEST(SmtpFilterConfigFactoryTest, CreatesReadFilterFromProto) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  SmtpFilterConfigFactory factory;

  SmtpProxyProto proto_config;
  proto_config.set_stat_prefix("smtp");
  proto_config.set_max_line_length(256);

  Network::FilterFactoryCb cb = factory.createFilterFactoryFromProto(proto_config, context);

  Network::MockConnection connection;
  EXPECT_CALL(connection, addReadFilter(_));
  cb(connection);
}

TEST(SmtpFilterConfigFactoryTest, SupportsEmptyProtoFromFactory) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  SmtpFilterConfigFactory factory;

  auto* proto = dynamic_cast<SmtpProxyProto*>(factory.createEmptyConfigProto().get());
  ASSERT_NE(proto, nullptr);

  Network::FilterFactoryCb cb = factory.createFilterFactoryFromProto(*proto, context);

  Network::MockConnection connection;
  EXPECT_CALL(connection, addReadFilter(_));
  cb(connection);
}

} // namespace
} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
