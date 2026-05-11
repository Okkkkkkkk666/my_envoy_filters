#include "contrib/smtp_proxy/smtp_decoder.h"
#include "absl/strings/match.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {

SmtpDecoder::SmtpDecoder(SmtpDecoderCallbacks& callbacks, uint32_t max_line_length)
    : callbacks_(callbacks), max_line_length_(max_line_length) {}

void SmtpDecoder::onData(const Buffer::Instance& data) {
  decode_buffer_.add(data);

  if (session_.state == State::StreamingData) {
    ssize_t pos = decode_buffer_.search("\r\n.\r\n", 5, 0, decode_buffer_.length());

    if (pos >= 0) {
      callbacks_.onDataChunk(decode_buffer_, pos);
      decode_buffer_.drain(pos + 5); 
      switchToCmdMode();
      callbacks_.onDataEnd(); 

      if (decode_buffer_.length() > 0) {
        Buffer::OwnedImpl empty;
        onData(empty);
      }
    } else {
      callbacks_.onDataChunk(decode_buffer_, decode_buffer_.length());
      decode_buffer_.drain(decode_buffer_.length());
    }
    return;
  }

  while (decode_buffer_.length() > 0) {
    ssize_t index = decode_buffer_.search("\r\n", 2, 0, decode_buffer_.length());
    if (index == -1) {
      if (decode_buffer_.length() > max_line_length_) {
        callbacks_.onProtocolError("500 Line length exceeds maximum\r\n");
        decode_buffer_.drain(decode_buffer_.length());
        return;
      }
      return;
    }
    if (index > max_line_length_) {
      callbacks_.onProtocolError("500 Line length exceeds maximum\r\n");
      decode_buffer_.drain(decode_buffer_.length());
      return;
    }

    std::string line;
    line.resize(index);
    decode_buffer_.copyOut(0, index, line.data());
    decode_buffer_.drain(index + 2);

    bool is_starttls = absl::StartsWithIgnoreCase(line, "STARTTLS");

    if (is_starttls && decode_buffer_.length() > 0) {
      callbacks_.onProtocolError("554 5.5.1 Pipelining not allowed after STARTTLS\r\n");
      decode_buffer_.drain(decode_buffer_.length());
      return; 
    }

    if (!parseLine(line)) {
      decode_buffer_.drain(decode_buffer_.length());
      return;
    }

    if (is_starttls) return;
    
    if (session_.state == State::StreamingData) {
      if (decode_buffer_.length() > 0) {
        Buffer::OwnedImpl empty;
        onData(empty);
      }
      return;
    }
  }
}

bool SmtpDecoder::parseLine(absl::string_view line) {
  absl::string_view command;
  absl::string_view args;

  if (absl::StartsWithIgnoreCase(line, "MAIL FROM:")) {
    command = "MAIL FROM";
    args = line.substr(10);
  } else if (absl::StartsWithIgnoreCase(line, "RCPT TO:")) {
    command = "RCPT TO";
    args = line.substr(8);
  } else {
    size_t space_idx = line.find(' ');
    if (space_idx == absl::string_view::npos) {
      command = line;
      args = "";
    } else {
      command = line.substr(0, space_idx);
      args = line.substr(space_idx + 1);
    }
  }

  if (!handleCommandWithState(command, args)) {
    callbacks_.onProtocolError("503 5.5.1 Bad sequence of commands\r\n");
    return false;
  }
  return true;
}

bool SmtpDecoder::handleCommandWithState(absl::string_view command, absl::string_view args) {
  if (absl::EqualsIgnoreCase(command, "NOOP") || 
      absl::EqualsIgnoreCase(command, "HELP") || 
      absl::EqualsIgnoreCase(command, "VRFY") || 
      absl::EqualsIgnoreCase(command, "EXPN")) {
    callbacks_.onCommand(command, args);
    return true;
  }

  if (absl::EqualsIgnoreCase(command, "RSET")) {
    session_.reset();
    session_.state = (session_.state == State::Init || session_.state == State::WaitHeloOrEhlo)
                         ? State::WaitHeloOrEhlo : State::WaitMailFrom;
    callbacks_.onResetRequested();
    return true;
  }

  if (absl::EqualsIgnoreCase(command, "QUIT")) {
    callbacks_.onQuitRequested();
    return true;
  }

  switch (session_.state) {
    case State::Init:
    case State::WaitHeloOrEhlo:
      if (absl::EqualsIgnoreCase(command, "EHLO") || absl::EqualsIgnoreCase(command, "HELO")) {
        session_.helo_domain = std::string(args);
        session_.reset();
        session_.state = State::WaitAuth; 
        callbacks_.onCommand(command, args);
        return true;
      }
      return false;

    case State::WaitAuth:
      if (absl::EqualsIgnoreCase(command, "AUTH")) {
        session_.state = State::WaitMailFrom;
        callbacks_.onCommand(command, args);
        return true;
      }
      [[fallthrough]];

    case State::WaitMailFrom:
      if (absl::EqualsIgnoreCase(command, "STARTTLS")) {
        if (session_.is_tls_active) return false;
        session_.reset();
        session_.helo_domain.clear();
        session_.state = State::WaitHeloOrEhlo;
        callbacks_.onStartTlsRequested();
        return true;
      }
      if (absl::EqualsIgnoreCase(command, "MAIL FROM")) {
        session_.reset();
        session_.mail_from = std::string(args);
        session_.state = State::WaitRcptTo;
        callbacks_.onCommand(command, args);
        return true;
      }
      if (absl::EqualsIgnoreCase(command, "EHLO") || absl::EqualsIgnoreCase(command, "HELO")) {
        session_.helo_domain = std::string(args);
        session_.reset();
        callbacks_.onCommand(command, args);
        return true;
      }
      return false;

    case State::WaitRcptTo:
      if (absl::EqualsIgnoreCase(command, "RCPT TO")) {
        session_.rcpt_to_list.push_back(std::string(args));
        session_.state = State::WaitDataCmd;
        callbacks_.onCommand(command, args);
        return true;
      }
      return false;

    case State::WaitDataCmd:
      if (absl::EqualsIgnoreCase(command, "RCPT TO")) {
        session_.rcpt_to_list.push_back(std::string(args));
        callbacks_.onCommand(command, args);
        return true;
      }
      if (absl::EqualsIgnoreCase(command, "DATA")) {
        if (session_.rcpt_to_list.empty()) return false;
        session_.state = State::StreamingData;
        callbacks_.onCommand(command, args);
        return true;
      }
      return false;

    case State::StreamingData:
      return false;

    case State::WaitQuitOrReset:
      if (absl::EqualsIgnoreCase(command, "MAIL FROM")) {
        session_.reset();
        session_.mail_from = std::string(args);
        session_.state = State::WaitRcptTo;
        callbacks_.onCommand(command, args);
        return true;
      }
      if (absl::EqualsIgnoreCase(command, "EHLO") || absl::EqualsIgnoreCase(command, "HELO")) {
        session_.helo_domain = std::string(args);
        session_.reset();
        session_.state = State::WaitMailFrom;
        callbacks_.onCommand(command, args);
        return true;
      }
      return false;
  }
  return false;
}

} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy