#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {

enum class State {
  Init = 0,        // Connection just established.
  WaitHeloOrEhlo,  // Waiting for greeting command.
  WaitMailFrom,    // Greeting accepted, waiting for MAIL FROM.
  WaitRcptTo,      // MAIL FROM accepted, waiting for first RCPT TO.
  WaitDataCmd,     // At least one RCPT TO accepted, waiting for DATA.
  StreamingData,   // DATA body streaming mode.
  WaitQuitOrReset, // DATA transaction finished, waiting for next transaction or QUIT.
};

struct SmtpSession {
  State state {State::Init};
  std::string helo_domain;
  std::string mail_from;
  std::vector<std::string> rcpt_to_list;
  uint64_t total_body_bytes {0};
  bool is_tls_active {false};

  void reset() {
    mail_from.clear();
    rcpt_to_list.clear();
    total_body_bytes = 0;
  }
};

} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
