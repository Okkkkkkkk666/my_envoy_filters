#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {

enum class State {
  Init = 0,        
  WaitHeloOrEhlo,  
  WaitAuth,        
  WaitMailFrom,    
  WaitRcptTo,      
  WaitDataCmd,     
  StreamingData,   
  WaitQuitOrReset, 
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