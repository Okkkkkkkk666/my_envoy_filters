#pragma once

#include <memory>
#include <optional>

#include "envoy/srhino_plugin_framework/v1_3_x/plugin_interface.h"
#include "source/common/srhino_plugin_framework/v1_3_x/context_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SrhinoPluginLoaderFilter {
namespace v1_3_x {
struct FilterStatus {
  std::unique_ptr<SrhinoPluginFramework::v1_3_x::HeaderContextImpl> header_context_;
  std::unique_ptr<SrhinoPluginFramework::v1_3_x::BodyContextImpl> body_context_;
  std::unique_ptr<SrhinoPluginFramework::v1_3_x::TrailersContextImpl> trailer_context_;
  std::optional<SrhinoPluginFramework::v1_3_x::HeaderStatus> header_status_;
  std::optional<SrhinoPluginFramework::v1_3_x::DataStatus> data_status_;
  std::optional<SrhinoPluginFramework::v1_3_x::TrailerStatus> trailer_status_;
  bool is_cotinued_{false};
  bool is_waited_{false};
};
} // namespace v1_3_x
} // namespace SrhinoPluginLoaderFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
