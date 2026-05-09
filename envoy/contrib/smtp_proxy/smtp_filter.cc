#include "contrib/smtp_proxy/smtp_filter.h"
#include "absl/strings/match.h"
#include "source/common/buffer/buffer_impl.h"
#include "envoy/event/dispatcher.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {


SmtpConfig::SmtpConfig(const std::string stat_prefix, const uint32_t max_line_length)
    :stat_prefix_(stat_prefix), max_line_length_(max_line_length > 0 ? max_line_length : 1024) {}

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
    ENVOY_CONN_LOG(info, "Intercepted STARTTLS, waiting flush callback before TLS upgrade",
                   read_callbacks_->connection());
    return Network::FilterStatus::StopIteration;
  }

  return Network::FilterStatus::Continue;
}

void SmtpFilter::onCommand(absl::string_view command, absl::string_view args) {
  ENVOY_CONN_LOG(info, "SMTP cmd [state={}, TLS={}]: {} {}", read_callbacks_->connection(),
                 stateToString(decoder_->session().state), decoder_->session().is_tls_active, command,
                 args);

  if (absl::EqualsIgnoreCase(command, "MAIL FROM") && absl::StrContains(args, "hacker@evil.com")) {
    sendToClientAndClose("550 5.7.1 Sender rejected by Envoy Policy\r\n");
    return;
  }
}

void SmtpFilter::onStartTlsRequested() { 
  intercept_starttls_ = true; 
}

void SmtpFilter::onQuitRequested() {
  decoder_->session().reset();
  decoder_->session().state = State::WaitHeloOrEhlo;
  is_blocked_ = true;
  read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
}

void SmtpFilter::onResetRequested() {
  ENVOY_CONN_LOG(debug, "SMTP session reset by RSET", read_callbacks_->connection());
}

absl::string_view SmtpFilter::stateToString(State state) {
  switch (state) {
    case State::Init:
      return "Init";
    case State::WaitHeloOrEhlo:
      return "WaitHeloOrEhlo";
    case State::WaitMailFrom:
      return "WaitMailFrom";
    case State::WaitRcptTo:
      return "WaitRcptTo";
    case State::WaitDataCmd:
      return "WaitDataCmd";
    case State::StreamingData:
      return "StreamingData";
    case State::WaitQuitOrReset:
      return "WaitQuitOrReset";
    default:
      return "Unknown";
  }
}

void SmtpFilter::onDataChunk(uint64_t chunk_length) {
  decoder_->session().total_body_bytes += chunk_length;
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

} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
