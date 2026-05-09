#include "source/common/srhino_plugin_framework/config_impl.h"
#include "envoy/srhino_plugin_framework/v1_0_x/utility/proto_tools.hpp"

namespace SrhinoPluginFramework {
ConfigPerRouteImpl::ConfigPerRouteImpl(google::protobuf::Struct&& proto_config)
    : proto_config_(std::move(proto_config)) {}

std::string ConfigPerRouteImpl::configJsonString() const {
  std::string json;
  v1_0_x::Utility::ProtoTools::messageToJson(proto_config_, json);
  return json;
}

} // namespace SrhinoPluginFramework