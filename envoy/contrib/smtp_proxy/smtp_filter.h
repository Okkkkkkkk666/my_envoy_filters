#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mimetic/mimetic.h>

#include "contrib/smtp_proxy/smtp_decoder.h"
#include "contrib/smtp_proxy/smtp_session.h"
#include "envoy/network/filter.h"
#include "source/common/common/logger.h"
#include "envoy/network/connection.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {

class SmtpConfig {
public:
  SmtpConfig(const std::string& stat_prefix,
             uint32_t max_line_length,
             bool mime_enabled,
             uint32_t max_body_bytes,
             const std::vector<std::string>& denied_senders);

  const std::string& statPrefix() const { return stat_prefix_; }
  uint32_t maxLineLength() const { return max_line_length_; }
  bool mimeEnabled() const { return mime_enabled_; }
  uint32_t maxBodyBytes() const { return max_body_bytes_; }
  const std::vector<std::string>& deniedSenders() const { return denied_senders_; }

private:
  const std::string stat_prefix_;
  const uint32_t max_line_length_;
  const bool mime_enabled_;
  const uint32_t max_body_bytes_;
  const std::vector<std::string> denied_senders_;
};

using SmtpConfigSharedPtr = std::shared_ptr<SmtpConfig>;

class SmtpFilter: public Network::ReadFilter,
                  public SmtpDecoderCallbacks,
                  public Logger::Loggable<Logger::Id::filter> {

public:
  SmtpFilter(SmtpConfigSharedPtr config);

  Network::FilterStatus onData(Buffer::Instance& data, bool end_stream) override;
  Network::FilterStatus onNewConnection() override;
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override;

  void onCommand(absl::string_view command, absl::string_view args) override;
  void onStartTlsRequested() override;
  void onQuitRequested() override;
  void onResetRequested() override;
  void onDataChunk(Buffer::Instance& data, uint64_t chunk_size) override;
  void onDataEnd() override;
  void onProtocolError(absl::string_view error_msg) override;

private:
  static absl::string_view stateToString(State state);
  void sendToClientAndClose(absl::string_view);
  void processMimePayload();
  bool checkPolicyBlock(absl::string_view sender);

  SmtpConfigSharedPtr config_;
  std::unique_ptr<SmtpDecoder> decoder_;
  Network::ReadFilterCallbacks* read_callbacks_{};                  
  
  bool is_blocked_ {false};
  bool intercept_starttls_ {false};
  bool waiting_for_tls_flush_ {false};

  Buffer::OwnedImpl mime_buffer_; 
  bool parse_mime_failed_ {false};
};

} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy