#pragma once

#include "envoy/http/filter.h"
#include "envoy/srhino_plugin_framework/v1_2_x/plugin_interface.h"
#include "filter_status.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SrhinoPluginLoaderFilter {
namespace v1_2_x {
class StatusConverter {
public:
  // 石犀插件框架返回值转换为envoy返回值
  static Http::FilterHeadersStatus convert(SrhinoPluginFramework::v1_2_x::HeaderStatus status);
  static Http::FilterDataStatus convert(SrhinoPluginFramework::v1_2_x::DataStatus status);
  static Http::FilterTrailersStatus convert(SrhinoPluginFramework::v1_2_x::TrailerStatus status);

  static void adjustDataStatus(FilterStatus& filter_status,
                               SrhinoPluginFramework::v1_2_x::DataStatus& status, bool end_stream);
};
} // namespace v1_2_x
} // namespace SrhinoPluginLoaderFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
