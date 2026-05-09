#pragma once

#include <string>

namespace Envoy {
namespace Extensions {
namespace Common {
namespace FPT {

class FPTInterface {
public:
  virtual ~FPTInterface() = default;
  virtual std::string tokenize(const std::string& plaintext, const std::string& salt) const = 0;
};

FPTInterface& getFPT();
} // namespace FPT
} // namespace Common
} // namespace Extensions
} // namespace Envoy