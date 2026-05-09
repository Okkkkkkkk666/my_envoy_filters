#include "contrib/mysql_proxy/filters/network/source/mysql_filter.h"

#include "envoy/config/core/v3/base.pb.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/assert.h"
#include "source/common/config/datasource.h"
#include "source/common/common/logger.h"
#include "source/extensions/filters/network/well_known_names.h"

#include "contrib/mysql_proxy/filters/network/source/mysql_codec.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_codec_clogin_resp.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_decoder_impl.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_utils.h"
#include "contrib/common/fpe/fpe.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MySQLProxy {

MySQLFilterConfig::MySQLFilterConfig(const std::string& stat_prefix, Stats::Scope& scope)
    : scope_(scope), stats_(generateStats(stat_prefix, scope)) {}

MySQLFilterConfig::MySQLFilterConfig(const std::string& stat_prefix, Stats::Scope& scope,
                                     std::vector<ProtectedColumnRule> protected_columns,
                                     Secret::GenericSecretConfigProviderSharedPtr fpe_secret_provider,
                                     Api::Api& api)
    : scope_(scope), stats_(generateStats(stat_prefix, scope)),
      protected_columns_(std::move(protected_columns)),
      fpe_secret_provider_(std::move(fpe_secret_provider)), api_(&api) {
  if (fpe_secret_provider_ != nullptr) {
    updateFpeKey();
    fpe_secret_update_callback_ = fpe_secret_provider_->addUpdateCallback([this]() {
      updateFpeKey();
    });
  }
}

bool MySQLFilterConfig::updateFpeKey() {
  if (fpe_secret_provider_ == nullptr || api_ == nullptr) {
    fpe_ready_ = false;
    fpe_secret_.clear();
    return false;
  }

  const auto* secret = fpe_secret_provider_->secret();
  if (secret == nullptr) {
    fpe_ready_ = false;
    fpe_secret_.clear();
    return false;
  }

  fpe_secret_ = Config::DataSource::read(secret->secret(), true, *api_);
  if (fpe_secret_.empty()) {
    fpe_ready_ = false;
    return false;
  }

  fpe_ready_ = ::Envoy::Extensions::Common::FPE::getFPE().initFromKey(fpe_secret_);
  return fpe_ready_;
}

MySQLFilter::MySQLFilter(MySQLFilterConfigSharedPtr config)
    : config_(std::move(config)),
      query_rewriter_(config_->protectedColumns(), ::Envoy::Extensions::Common::FPE::getFPE()),
      resultset_rewriter_(config_->protectedColumns()) {}

void MySQLFilter::initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) {
  read_callbacks_ = &callbacks;
}

Network::FilterStatus MySQLFilter::onData(Buffer::Instance& data, bool) {
  if (!sniffing_) {
    return Network::FilterStatus::Continue;
  }
  return processData(data, read_buffer_, true);
}

Network::FilterStatus MySQLFilter::onWrite(Buffer::Instance& data, bool) {
  if (!sniffing_) {
    return Network::FilterStatus::Continue;
  }
  return processData(data, write_buffer_, false);
}

void MySQLFilter::doDecode(Buffer::Instance& buffer) {
  // Clear dynamic metadata.
  envoy::config::core::v3::Metadata& dynamic_metadata =
      read_callbacks_->connection().streamInfo().dynamicMetadata();
  auto& metadata =
      (*dynamic_metadata.mutable_filter_metadata())[NetworkFilterNames::get().MySQLProxy];
  metadata.mutable_fields()->clear();

  if (!decoder_) {
    decoder_ = createDecoder(*this);
  }

  try {
    decoder_->onData(buffer);
  } catch (EnvoyException& e) {
    ENVOY_LOG(info, "mysql_proxy: decoding error: {}", e.what());
    config_->stats_.decoder_errors_.inc();
    sniffing_ = false;
    read_buffer_.drain(read_buffer_.length());
    write_buffer_.drain(write_buffer_.length());
  }
}

DecoderPtr MySQLFilter::createDecoder(DecoderCallbacks& callbacks) {
  return std::make_unique<DecoderImpl>(callbacks);
}

void MySQLFilter::onProtocolError() { config_->stats_.protocol_errors_.inc(); }

void MySQLFilter::onNewMessage(MySQLSession::State state) {
  if (state == MySQLSession::State::ChallengeReq) {
    config_->stats_.login_attempts_.inc();
  }
}

void MySQLFilter::onClientLogin(ClientLogin& client_login) {
  if (client_login.isSSLRequest()) {
    config_->stats_.upgraded_to_ssl_.inc();
    ENVOY_LOG(info, "mysql_proxy: client requested SSL, query rewrite will be bypassed");
  }
  if (client_login.isConnectWithDb() && !client_login.getDb().empty()) {
    current_database_ = QueryRewriter::canonicalIdentifier(client_login.getDb());
    ENVOY_LOG(trace, "mysql_proxy: current database set from login to {}", current_database_);
  }
}

void MySQLFilter::onClientLoginResponse(ClientLoginResponse& client_login_resp) {
  if (client_login_resp.getRespCode() == MYSQL_RESP_AUTH_SWITCH) {
    config_->stats_.auth_switch_request_.inc();
  } else if (client_login_resp.getRespCode() == MYSQL_RESP_ERR) {
    config_->stats_.login_failures_.inc();
  }
}

void MySQLFilter::onMoreClientLoginResponse(ClientLoginResponse& client_login_resp) {
  if (client_login_resp.getRespCode() == MYSQL_RESP_ERR) {
    config_->stats_.login_failures_.inc();
  }
}

void MySQLFilter::onCommand(Command& command) {
  if (!command.isQuery()) {
    return;
  }

  // Parse a given query
  envoy::config::core::v3::Metadata& dynamic_metadata =
      read_callbacks_->connection().streamInfo().dynamicMetadata();
  ProtobufWkt::Struct metadata(
      (*dynamic_metadata.mutable_filter_metadata())[NetworkFilterNames::get().MySQLProxy]);

  auto result = Common::SQLUtils::SQLUtils::setMetadata(command.getData(),
                                                        decoder_->getAttributes(), metadata);

  ENVOY_CONN_LOG(trace, "mysql_proxy: query processed {}, result {}, cmd type {}",
                 read_callbacks_->connection(), command.getData(), result, command.getCmd());

  if (!result) {
    config_->stats_.queries_parse_error_.inc();
    return;
  }
  config_->stats_.queries_parsed_.inc();

  const auto& fields = metadata.fields();
  auto comment_it = fields.find("sql_comments");
  
  if (comment_it != fields.end()) {
    std::string captured_comment = comment_it->second.string_value();
    
    // 成功抓取！打印 info 级别的日志供我们在控制台直接观测
    ENVOY_CONN_LOG(info, "mysql_proxy: Successfully captured SQL comment: [{}]", 
                   read_callbacks_->connection(), captured_comment);
  }

  read_callbacks_->connection().streamInfo().setDynamicMetadata(
      NetworkFilterNames::get().MySQLProxy, metadata);
}

void MySQLFilter::onCommandResponse(CommandResponse& resp) {
  (void)resp;
}

Network::FilterStatus MySQLFilter::onNewConnection() {
  config_->stats_.sessions_.inc();
  return Network::FilterStatus::Continue;
}

Network::FilterStatus MySQLFilter::processData(Buffer::Instance& data, Buffer::OwnedImpl& pending,
                                               bool from_client) {
  pending.move(data);
  Buffer::OwnedImpl rewritten;
  while (pending.length() >= MYSQL_HDR_SIZE) {
    uint32_t len = 0;
    uint8_t seq = 0;
    if (BufferHelper::peekHdr(pending, len, seq) != DecodeStatus::Success ||
        pending.length() < MYSQL_HDR_SIZE + len) {
      break;
    }
    Buffer::OwnedImpl packet;
    packet.move(pending, MYSQL_HDR_SIZE + len);
    processPacket(packet, rewritten, from_client);
  }
  data.move(rewritten);
  return Network::FilterStatus::Continue;
}

bool MySQLFilter::processPacket(Buffer::OwnedImpl& packet, Buffer::OwnedImpl& output,
                                bool from_client) {
  uint32_t len = 0;
  uint8_t seq = 0;
  if (BufferHelper::peekHdr(packet, len, seq) != DecodeStatus::Success) {
    return false;
  }
  BufferHelper::consumeHdr(packet);

  Buffer::OwnedImpl payload;
  payload.move(packet);
  bool changed = false;
  if (from_client) {
    changed = rewriteClientPayload(payload, seq);
  } else {
    changed = rewriteServerPayload(payload);
  }

  BufferHelper::encodeHdr(payload, seq);
  Buffer::OwnedImpl decode_copy;
  decode_copy.add(payload);
  doDecode(decode_copy);
  output.move(payload);
  return changed;
}

bool MySQLFilter::rewriteClientPayload(Buffer::OwnedImpl& payload, uint8_t seq) {
  if (!config_->fpeReady()) {
    ENVOY_LOG(info, "mysql_proxy: rewrite skipped because FPE is not ready");
    return false;
  }
  if (!decoder_) {
    decoder_ = createDecoder(*this);
  }

  const auto state = decoder_->getSession().getState();
  const bool can_rewrite =
      state == MySQLSession::State::Req || state == MySQLSession::State::Resync ||
      (state == MySQLSession::State::ReqResp && seq == MYSQL_REQUEST_PKT_NUM);
  if (!can_rewrite) {
    ENVOY_LOG(trace, "mysql_proxy: rewrite skipped due to session state {}",
              static_cast<int>(state));
    return false;
  }
  if (seq != MYSQL_REQUEST_PKT_NUM) {
    ENVOY_LOG(trace, "mysql_proxy: rewrite skipped due to seq {}", seq);
    return false;
  }

  Buffer::OwnedImpl copy;
  copy.add(payload);
  Command command;
  if (command.decode(copy, seq, payload.length()) != DecodeStatus::Success) {
    return false;
  }

  if (command.getCmd() == Command::Cmd::InitDb && !command.getDb().empty()) {
    current_database_ = QueryRewriter::canonicalIdentifier(command.getDb());
    resultset_rewriter_.clear();
    ENVOY_LOG(trace, "mysql_proxy: current database set from COM_INIT_DB to {}",
              current_database_);
    return false;
  }

  if (!command.isQuery()) {
    return false;
  }

  const std::string query = command.getData();
  ENVOY_LOG(trace, "mysql_proxy: evaluating client query for rewrite, current_db='{}', query='{}'",
            current_database_, query);
  std::string new_database;
  if (query_rewriter_.isUseStatement(query, new_database)) {
    current_database_ = new_database;
    resultset_rewriter_.clear();
    ENVOY_LOG(trace, "mysql_proxy: current database set from USE to {}", current_database_);
    return false;
  }

  SelectDecryptPlan plan;
  if (query_rewriter_.buildSelectPlan(query, current_database_, plan)) {
    ENVOY_LOG(trace,
              "mysql_proxy: select decrypt plan enabled db='{}' table='{}' wildcard={} columns={}",
              plan.database, plan.table, plan.wildcard,
              fmt::join(plan.projected_columns, ","));
    resultset_rewriter_.startQuery(plan);
  } else {
    ENVOY_LOG(trace, "mysql_proxy: no select decrypt plan for query='{}'", query);
    resultset_rewriter_.clear();
  }

  std::string rewritten_sql;
  if (!query_rewriter_.rewriteInsertOrUpdate(query, current_database_, rewritten_sql)) {
    ENVOY_LOG(trace, "mysql_proxy: query rewrite not applied, current_db='{}', query='{}'",
              current_database_, query);
    return false;
  }

  try {
    ENVOY_LOG(trace, "mysql_proxy: query rewritten from '{}' to '{}'", query, rewritten_sql);
    command.setData(rewritten_sql);
    payload.drain(payload.length());
    command.encode(payload);
    config_->stats_.queries_rewritten_.inc();
    return true;
  } catch (...) {
    config_->stats_.query_rewrite_errors_.inc();
  }
  return false;
}

bool MySQLFilter::rewriteServerPayload(Buffer::OwnedImpl& payload) {
  if (!config_->fpeReady() || !resultset_rewriter_.active()) {
    ENVOY_LOG(trace, "mysql_proxy: server payload rewrite skipped, fpe_ready={}, active={}",
              config_->fpeReady(), resultset_rewriter_.active());
    return false;
  }

  Buffer::OwnedImpl rewritten_payload;
  bool changed = false;
  bool completed = false;
  if (!resultset_rewriter_.rewritePacket(payload, rewritten_payload,
                                         ::Envoy::Extensions::Common::FPE::getFPE(), changed,
                                         completed)) {
    config_->stats_.result_decrypt_errors_.inc();
    resultset_rewriter_.clear();
    return false;
  }
  if (completed && !changed) {
    return false;
  }
  if (changed) {
    ENVOY_LOG(trace, "mysql_proxy: server payload rewritten successfully");
    payload.drain(payload.length());
    payload.move(rewritten_payload);
  }
  return changed;
}

} // namespace MySQLProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
