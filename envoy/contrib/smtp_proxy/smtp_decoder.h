#pragma once

#include "absl/strings/string_view.h"
#include "contrib/smtp_proxy/smtp_session.h"
#include "envoy/buffer/buffer.h"
#include "source/common/buffer/buffer_impl.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {

class SmtpDecoderCallbacks {
public:
  virtual ~SmtpDecoderCallbacks() = default;

  virtual void onCommand(absl::string_view command, absl::string_view args) PURE;
  virtual void onStartTlsRequested() PURE;
  virtual void onQuitRequested() PURE;
  virtual void onResetRequested() PURE;
  virtual void onDataChunk(uint64_t chunk_size) PURE;
  virtual void onProtocolError(absl::string_view error_msg) PURE;
};

class SmtpDecoder {
public:
  SmtpDecoder(SmtpDecoderCallbacks& callbacks, uint32_t max_line_length);

  SmtpSession& session() { return session_; }
  const SmtpSession& session() const { return session_; }

  void onData(const Buffer::Instance& data);
  void switchToStreamingMode() {
    session_.state = State::StreamingData;
  }

  void switchToCmdMode() {
    if (session_.state == State::StreamingData) {
      session_.state = State::WaitQuitOrReset;
    }
  }

private:
  bool parseLine(absl::string_view line);
  bool handleCommandWithState(absl::string_view command, absl::string_view args);
  SmtpDecoderCallbacks& callbacks_;
  uint32_t max_line_length_;
  SmtpSession session_;
  Buffer::OwnedImpl decode_buffer_;
};
} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
