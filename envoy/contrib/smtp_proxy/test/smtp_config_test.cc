#include "contrib/smtp_proxy/smtp_config.h"
#include "contrib/smtp_proxy/smtp_filter.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/server/factory_context.h"

#include "contrib/envoy/extensions/filters/network/smtp_proxy/v3/smtp_proxy.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {
namespace {

using SmtpProxyProto = envoy::extensions::filters::network::smtp_proxy::v3::SmtpProxy;

TEST(SmtpConfigTest, StoresConfiguredValues) {
  std::vector<std::string> blocked = {"badguy.com"};
  auto config = std::make_shared<SmtpConfig>("smtp_test", 2048, true, 50000, blocked);

  EXPECT_EQ(config->statPrefix(), "smtp_test");
  EXPECT_EQ(config->maxLineLength(), 2048);
  EXPECT_TRUE(config->mimeEnabled());
  EXPECT_EQ(config->maxBodyBytes(), 50000);
  EXPECT_EQ(config->deniedSenders().size(), 1);
  EXPECT_EQ(config->deniedSenders()[0], "badguy.com");
}

TEST(SmtpFilterConfigFactoryTest, SanitizesAndCreatesFilterFactoryFromProto) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  SmtpFilterConfigFactory factory;

  SmtpProxyProto proto_config;
  proto_config.set_stat_prefix("smtp");
  proto_config.set_max_line_length(0); 

  Network::FilterFactoryCb cb = factory.createFilterFactoryFromProto(proto_config, context);

  Network::MockConnection connection;
  EXPECT_CALL(connection, addReadFilter(_));
  cb(connection);
}

TEST(SmtpFilterConfigFactoryTest, SupportsEmptyProtoFromFactory) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  SmtpFilterConfigFactory factory;

  auto proto = factory.createEmptyConfigProto();
  auto* typed_proto = dynamic_cast<SmtpProxyProto*>(proto.get());
  ASSERT_NE(typed_proto, nullptr);

  Network::FilterFactoryCb cb = factory.createFilterFactoryFromProto(*typed_proto, context);

  Network::MockConnection connection;
  EXPECT_CALL(connection, addReadFilter(_));
  cb(connection);
}

} // namespace
} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy