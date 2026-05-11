#include "contrib/smtp_proxy/smtp_filter.h"
#include "absl/strings/match.h"
#include "source/common/buffer/buffer_impl.h"
#include "envoy/event/dispatcher.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {

SmtpConfig::SmtpConfig(const std::string& stat_prefix,
                       uint32_t max_line_length,
                       bool mime_enabled,
                       uint32_t max_body_bytes,
                       const std::vector<std::string>& denied_senders)
    : stat_prefix_(stat_prefix),
      max_line_length_(max_line_length),
      mime_enabled_(mime_enabled),
      max_body_bytes_(max_body_bytes),
      denied_senders_(denied_senders) {}

SmtpFilter::SmtpFilter(SmtpConfigSharedPtr config)
    : config_(config),
      decoder_(std::make_unique<SmtpDecoder>(*this, config->maxLineLength())) {}

void SmtpFilter::initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) {
  read_callbacks_ = &callbacks;
}

Network::FilterStatus SmtpFilter::onNewConnection() {
  decoder_->session().state = State::WaitHeloOrEhlo;
  return Network::FilterStatus::Continue;
}

Network::FilterStatus SmtpFilter::onData(Buffer::Instance& data, bool /*end_stream*/) {
  if (is_blocked_) {
    data.drain(data.length());
    return Network::FilterStatus::StopIteration;
  }
  
  decoder_->onData(data);

  if (is_blocked_) {
    data.drain(data.length());
    return Network::FilterStatus::StopIteration;
  }

  if (intercept_starttls_) {
    intercept_starttls_ = false;
    decoder_->session().is_tls_active = true;
    data.drain(data.length());
    waiting_for_tls_flush_ = true;

    read_callbacks_->connection().addBytesSentCallback([this](uint64_t bytes_sent) {
      if (waiting_for_tls_flush_) {
        waiting_for_tls_flush_ = false;
        ENVOY_CONN_LOG(info, "220 response flushed ({} bytes), upgrading TLS",
                       read_callbacks_->connection(), bytes_sent);
        read_callbacks_->connection().startSecureTransport();
        read_callbacks_->continueReading();
        return false;
      }
      return false;
    });

    Buffer::OwnedImpl response("220 2.0.0 Ready to start TLS\r\n");
    read_callbacks_->connection().write(response, false);
    return Network::FilterStatus::StopIteration;
  }

  return Network::FilterStatus::Continue;
}

bool SmtpFilter::checkPolicyBlock(absl::string_view sender) {
  for (const auto& denied : config_->deniedSenders()) {
    if (absl::StrContains(sender, denied)) {
      return true;
    }
  }
  return false;
}

void SmtpFilter::onCommand(absl::string_view command, absl::string_view args) {
  ENVOY_CONN_LOG(info, "SMTP cmd [state={}, TLS={}]: {} {}", read_callbacks_->connection(),
                 stateToString(decoder_->session().state), decoder_->session().is_tls_active, command, args);

  if (absl::EqualsIgnoreCase(command, "MAIL FROM")) {
    if (checkPolicyBlock(args)) {
      sendToClientAndClose("550 5.7.1 Sender rejected by Envoy Policy\r\n");
      return;
    }
  }
}

void SmtpFilter::onStartTlsRequested() { intercept_starttls_ = true; }

void SmtpFilter::onQuitRequested() {
  decoder_->session().reset();
  decoder_->session().state = State::WaitHeloOrEhlo;
  is_blocked_ = true;
  read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
}

void SmtpFilter::onResetRequested() {
  ENVOY_CONN_LOG(debug, "SMTP session reset by RSET", read_callbacks_->connection());
}

void SmtpFilter::onDataChunk(Buffer::Instance& data, uint64_t chunk_length) {
  decoder_->session().total_body_bytes += chunk_length;

  if (config_->mimeEnabled()) {
    if (mime_buffer_.length() + chunk_length > config_->maxBodyBytes()) {
      ENVOY_CONN_LOG(warn, "SMTP DATA exceeds max body size, aborting MIME parse", 
                     read_callbacks_->connection());
      parse_mime_failed_ = true;
    } else {
      mime_buffer_.move(data, chunk_length);
    }
  }
}

void SmtpFilter::onDataEnd() {
  ENVOY_CONN_LOG(info, "SMTP DATA END. Total bytes: {}", read_callbacks_->connection(), 
                 decoder_->session().total_body_bytes);

  if (config_->mimeEnabled() && !parse_mime_failed_) {
    processMimePayload();
  }

  mime_buffer_.drain(mime_buffer_.length());
  parse_mime_failed_ = false;
  decoder_->session().reset();
}

void SmtpFilter::processMimePayload() {
  std::string raw_payload = mime_buffer_.toString();
  try {
    mimetic::MimeEntity entity(raw_payload.begin(), raw_payload.end());
    std::string subject = entity.header().subject();
    std::string content_type = entity.header().contentType().str();

    ENVOY_CONN_LOG(info, "MIME Parsed successfully. Subject: {}, Content-Type: {}", 
                   read_callbacks_->connection(), subject, content_type);

    if (entity.header().contentType().isMultipart()) {
      auto body_parts = entity.body().parts();
      for (auto it = body_parts.begin(); it != body_parts.end(); ++it) {
        ENVOY_CONN_LOG(debug, "Found Attachment/Part: {}", 
                       read_callbacks_->connection(), (*it)->header().contentType().str());
      }
    }
  } catch (const std::exception& e) {
    ENVOY_CONN_LOG(error, "Mimetic parsing exception: {}", read_callbacks_->connection(), e.what());
  }
}

void SmtpFilter::onProtocolError(absl::string_view error_msg) {
  sendToClientAndClose(error_msg);
}

void SmtpFilter::sendToClientAndClose(absl::string_view msg) {
  is_blocked_ = true;
  Buffer::OwnedImpl buf(msg);
  read_callbacks_->connection().write(buf, false);
  read_callbacks_->connection().close(Network::ConnectionCloseType::FlushWrite);
}

absl::string_view SmtpFilter::stateToString(State state) {
  switch (state) {
    case State::Init: return "Init";
    case State::WaitHeloOrEhlo: return "WaitHeloOrEhlo";
    case State::WaitAuth: return "WaitAuth";
    case State::WaitMailFrom: return "WaitMailFrom";
    case State::WaitRcptTo: return "WaitRcptTo";
    case State::WaitDataCmd: return "WaitDataCmd";
    case State::StreamingData: return "StreamingData";
    case State::WaitQuitOrReset: return "WaitQuitOrReset";
    default: return "Unknown";
  }
}

} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy