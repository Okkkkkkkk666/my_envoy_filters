#include "source/common/srhino_plugin_framework/v1_2_x/connection_info_impl.h"
#include "source/common/common/empty_string.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
ConnectionInfoImpl::ConnectionInfoImpl(Envoy::Http::StreamFilterCallbacks* callbacks)
    : callbacks_(callbacks) {}

uint32_t ConnectionInfoImpl::downstreamRemoteAddress() const {
  if (callbacks_->streamInfo().downstreamAddressProvider().remoteAddress()->ip()->version() ==
      Envoy::Network::Address::IpVersion::v4) {
    return callbacks_->streamInfo()
        .downstreamAddressProvider()
        .remoteAddress()
        ->ip()
        ->ipv4()
        ->address();
  }
  return 0;
}

uint16_t ConnectionInfoImpl::downstreamRemotePort() const {
  return callbacks_->streamInfo().downstreamAddressProvider().remoteAddress()->ip()->port();
}

uint32_t ConnectionInfoImpl::downstreamLocalAddress() const {
  if (callbacks_->streamInfo().downstreamAddressProvider().localAddress()->ip()->version() ==
      Envoy::Network::Address::IpVersion::v4) {
    return callbacks_->streamInfo()
        .downstreamAddressProvider()
        .localAddress()
        ->ip()
        ->ipv4()
        ->address();
  }
  return 0;
}

uint16_t ConnectionInfoImpl::downstreamLocalPort() const {
  return callbacks_->streamInfo().downstreamAddressProvider().localAddress()->ip()->port();
}

uint32_t ConnectionInfoImpl::upstreamRemoteAddress() const {
  auto upstream_info = callbacks_->streamInfo().upstreamInfo();
  if (upstream_info && (upstream_info->upstreamHost()->address()->ip()->version() ==
                        Envoy::Network::Address::IpVersion::v4)) {
    return upstream_info->upstreamHost()->address()->ip()->ipv4()->address();
  }
  return 0;
}

uint16_t ConnectionInfoImpl::upstreamRemotePort() const {
  auto upstream_info = callbacks_->streamInfo().upstreamInfo();
  if (upstream_info) {
    return upstream_info->upstreamHost()->address()->ip()->port();
  }
  return 0;
}

uint32_t ConnectionInfoImpl::upstreamLocalAddress() const {
  auto upstream_info = callbacks_->streamInfo().upstreamInfo();
  if (upstream_info && (upstream_info->upstreamLocalAddress()->ip()->version() ==
                        Envoy::Network::Address::IpVersion::v4)) {
    return upstream_info->upstreamLocalAddress()->ip()->ipv4()->address();
  }
  return 0;
}

uint16_t ConnectionInfoImpl::upstreamLocalPort() const {
  auto upstream_info = callbacks_->streamInfo().upstreamInfo();
  if (upstream_info) {
    return upstream_info->upstreamLocalAddress()->ip()->port();
  }

  return 0;
}

const std::string& ConnectionInfoImpl::upstreamName() const {
  Envoy::Upstream::ClusterInfoConstSharedPtr cluster =
      callbacks_->streamInfo().upstreamClusterInfo().has_value()
          ? callbacks_->streamInfo().upstreamClusterInfo().value()
          : nullptr;

  return cluster ? cluster->name() : Envoy::EMPTY_STRING;
}

ConnectionInfo::Protocol ConnectionInfoImpl::protocol() const {
  return static_cast<Protocol>(
      callbacks_->streamInfo().protocol().value_or(Envoy::Http::Protocol::Http11));
}

std::string ConnectionInfoImpl::sessionId() const {
  auto ssl = callbacks_->streamInfo().downstreamAddressProvider().sslConnection();
  if (ssl) {
    return ssl->sessionId();
  }
  return Envoy::EMPTY_STRING;
}

} // namespace v1_2_x
} // namespace SrhinoPluginFramework