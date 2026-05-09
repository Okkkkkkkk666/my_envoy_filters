#pragma once

#include <cstdint>
#include <string>

namespace SrhinoPluginFramework {
namespace v1_4_x {
class ConnectionInfo {
public:
  virtual ~ConnectionInfo() = default;

public:
  enum class Protocol { Http10, Http11, Http2, Http3 };

public:
  virtual uint32_t downstreamRemoteAddress() const = 0;
  virtual uint16_t downstreamRemotePort() const = 0;
  virtual uint32_t downstreamLocalAddress() const = 0;
  virtual uint16_t downstreamLocalPort() const = 0;
  virtual uint32_t upstreamRemoteAddress() const = 0;
  virtual uint16_t upstreamRemotePort() const = 0;
  virtual uint32_t upstreamLocalAddress() const = 0;
  virtual uint16_t upstreamLocalPort() const = 0;
  virtual const std::string& upstreamName() const = 0;
  virtual Protocol protocol() const = 0;
  virtual std::string sessionId() const = 0;
};
} // namespace v1_4_x
} // namespace SrhinoPluginFramework