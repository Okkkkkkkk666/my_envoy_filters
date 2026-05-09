#pragma once

#include "envoy/srhino_plugin_framework/v1_2_x/connection_info.h"
#include "envoy/http/filter.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
class ConnectionInfoImpl : public ConnectionInfo {
public:
  ConnectionInfoImpl(Envoy::Http::StreamFilterCallbacks* callbacks);

public:
  uint32_t downstreamRemoteAddress() const override;
  uint16_t downstreamRemotePort() const override;
  uint32_t downstreamLocalAddress() const override;
  uint16_t downstreamLocalPort() const override;
  uint32_t upstreamRemoteAddress() const override;
  uint16_t upstreamRemotePort() const override;
  uint32_t upstreamLocalAddress() const override;
  uint16_t upstreamLocalPort() const override;
  const std::string& upstreamName() const override;
  Protocol protocol() const override;
  std::string sessionId() const override;

private:
  Envoy::Http::StreamFilterCallbacks* callbacks_;
};
} // namespace v1_2_x
} // namespace SrhinoPluginFramework