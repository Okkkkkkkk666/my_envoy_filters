#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/api/api.h"
#include "envoy/access_log/access_log.h"
#include "envoy/extensions/transport_sockets/tls/v3/secret.pb.h"
#include "envoy/network/connection.h"
#include "envoy/network/filter.h"
#include "envoy/secret/secret_provider.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats.h"
#include "envoy/stats/stats_macros.h"

#include "source/common/common/logger.h"

#include "contrib/mysql_proxy/filters/network/source/mysql_codec.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_codec_clogin.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_codec_clogin_resp.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_codec_command.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_codec_greeting.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_codec_switch_resp.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_decoder.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_query_rewrite.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_resultset.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_session.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MySQLProxy {

/**
 * All MySQL proxy stats. @see stats_macros.h
 */
#define ALL_MYSQL_PROXY_STATS(COUNTER)                                                             \
  COUNTER(sessions)                                                                                \
  COUNTER(login_attempts)                                                                          \
  COUNTER(login_failures)                                                                          \
  COUNTER(decoder_errors)                                                                          \
  COUNTER(protocol_errors)                                                                         \
  COUNTER(upgraded_to_ssl)                                                                         \
  COUNTER(auth_switch_request)                                                                     \
  COUNTER(queries_parsed)                                                                          \
  COUNTER(queries_parse_error)                                                                     \
  COUNTER(queries_rewritten)                                                                       \
  COUNTER(query_rewrite_errors)                                                                    \
  COUNTER(result_decrypt_errors)

/**
 * Struct definition for all MySQL proxy stats. @see stats_macros.h
 */
struct MySQLProxyStats {
  ALL_MYSQL_PROXY_STATS(GENERATE_COUNTER_STRUCT)
};

/**
 * Configuration for the MySQL proxy filter.
 */
class MySQLFilterConfig {
public:
  MySQLFilterConfig(const std::string& stat_prefix, Stats::Scope& scope);
  MySQLFilterConfig(const std::string& stat_prefix, Stats::Scope& scope,
                    std::vector<ProtectedColumnRule> protected_columns,
                    Secret::GenericSecretConfigProviderSharedPtr fpe_secret_provider,
                    Api::Api& api);

  const MySQLProxyStats& stats() const { return stats_; }
  const std::vector<ProtectedColumnRule>& protectedColumns() const { return protected_columns_; }
  bool fpeReady() const { return fpe_ready_; }
  const std::string& fpeSecret() const { return fpe_secret_; }

  Stats::Scope& scope_;
  MySQLProxyStats stats_;

private:
  bool updateFpeKey();

  MySQLProxyStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    return MySQLProxyStats{ALL_MYSQL_PROXY_STATS(POOL_COUNTER_PREFIX(scope, prefix))};
  }

  std::vector<ProtectedColumnRule> protected_columns_;
  Secret::GenericSecretConfigProviderSharedPtr fpe_secret_provider_;
  Api::Api* api_{};
  std::string fpe_secret_;
  bool fpe_ready_{false};
  Envoy::Common::CallbackHandlePtr fpe_secret_update_callback_;
};

using MySQLFilterConfigSharedPtr = std::shared_ptr<MySQLFilterConfig>;

/**
 * Implementation of MySQL proxy filter.
 */
class MySQLFilter : public Network::Filter, DecoderCallbacks, Logger::Loggable<Logger::Id::filter> {
public:
  MySQLFilter(MySQLFilterConfigSharedPtr config);
  ~MySQLFilter() override = default;

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance& data, bool end_stream) override;
  Network::FilterStatus onNewConnection() override;
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override;

  // Network::WriteFilter
  Network::FilterStatus onWrite(Buffer::Instance& data, bool end_stream) override;

  // MySQLProxy::DecoderCallback
  void onProtocolError() override;
  void onNewMessage(MySQLSession::State state) override;
  void onServerGreeting(ServerGreeting&) override{};
  void onClientLogin(ClientLogin& message) override;
  void onClientLoginResponse(ClientLoginResponse& message) override;
  void onClientSwitchResponse(ClientSwitchResponse&) override{};
  void onMoreClientLoginResponse(ClientLoginResponse& message) override;
  void onCommand(Command& message) override;
  void onCommandResponse(CommandResponse&) override;

  void doDecode(Buffer::Instance& buffer);
  DecoderPtr createDecoder(DecoderCallbacks& callbacks);
  MySQLSession& getSession() { return decoder_->getSession(); }

private:
  Network::FilterStatus processData(Buffer::Instance& data, Buffer::OwnedImpl& pending,
                                    bool from_client);
  bool processPacket(Buffer::OwnedImpl& packet, Buffer::OwnedImpl& output, bool from_client);
  bool rewriteClientPayload(Buffer::OwnedImpl& payload, uint8_t seq);
  bool rewriteServerPayload(Buffer::OwnedImpl& payload);

  Network::ReadFilterCallbacks* read_callbacks_{};
  MySQLFilterConfigSharedPtr config_;
  Buffer::OwnedImpl read_buffer_;
  Buffer::OwnedImpl write_buffer_;
  std::unique_ptr<Decoder> decoder_;
  QueryRewriter query_rewriter_;
  MySQLResultsetRewriter resultset_rewriter_;
  std::string current_database_;
  bool sniffing_{true};
};

} // namespace MySQLProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
