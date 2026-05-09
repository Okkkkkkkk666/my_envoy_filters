#include "status_converter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SrhinoPluginLoaderFilter {
namespace v1_0_x {
Http::FilterHeadersStatus
StatusConverter::convert(SrhinoPluginFramework::v1_0_x::HeaderStatus status) {
  using SrhinoStatus = SrhinoPluginFramework::v1_0_x::HeaderStatus;
  using EnvoyStatus = Envoy::Http::FilterHeadersStatus;

  static std::unordered_map<SrhinoStatus, EnvoyStatus> status_map = {
      {SrhinoStatus::Continue, EnvoyStatus::Continue},
      {SrhinoStatus::Break, EnvoyStatus::StopIteration},
      {SrhinoStatus::Pause, EnvoyStatus::StopAllIterationAndWatermark},
      {SrhinoStatus::AppendBody, EnvoyStatus::ContinueAndDontEndStream},
      {SrhinoStatus::DirectResponse, EnvoyStatus::StopIteration}};

  return status_map[status];
}

Http::FilterDataStatus StatusConverter::convert(SrhinoPluginFramework::v1_0_x::DataStatus status) {
  using SrhinoStatus = SrhinoPluginFramework::v1_0_x::DataStatus;
  using EnvoyStatus = Envoy::Http::FilterDataStatus;

  static std::unordered_map<SrhinoStatus, EnvoyStatus> status_map = {
      {SrhinoStatus::Continue, EnvoyStatus::Continue},
      {SrhinoStatus::Break, EnvoyStatus::StopIterationNoBuffer},
      {SrhinoStatus::Pause, EnvoyStatus::StopIterationAndWatermark},
      {SrhinoStatus::WaitForBody, EnvoyStatus::StopIterationAndBuffer},
      {SrhinoStatus::TryWaitForBody, EnvoyStatus::StopIterationAndBuffer},
      {SrhinoStatus::DirectResponse, EnvoyStatus::StopIterationNoBuffer}};

  return status_map[status];
}

Http::FilterTrailersStatus
StatusConverter::convert(SrhinoPluginFramework::v1_0_x::TrailerStatus status) {
  using SrhinoStatus = SrhinoPluginFramework::v1_0_x::TrailerStatus;
  using EnvoyStatus = Envoy::Http::FilterTrailersStatus;

  static std::unordered_map<SrhinoStatus, EnvoyStatus> status_map = {
      {SrhinoStatus::Continue, EnvoyStatus::Continue},
      {SrhinoStatus::Break, EnvoyStatus::StopIteration},
      {SrhinoStatus::DirectResponse, EnvoyStatus::StopIteration}};

  return status_map[status];
}

void StatusConverter::adjustDataStatus(FilterStatus& filter_status,
                                       SrhinoPluginFramework::v1_0_x::DataStatus& status,
                                       bool end_stream) {
  // 如果之前返回过Continue，那么之后将不再被允许继续返回WaitForBody，如果继续返回WaitForBody，
  // 那此时的行为将跟返回Continue一样。
  if (status == SrhinoPluginFramework::v1_0_x::DataStatus::Continue) {
    filter_status.is_cotinued_ = true;
  } else if (status == SrhinoPluginFramework::v1_0_x::DataStatus::WaitForBody ||
             status == SrhinoPluginFramework::v1_0_x::DataStatus::TryWaitForBody) {
    if (filter_status.is_waited_) {
      status = SrhinoPluginFramework::v1_0_x::DataStatus::Continue;
    } else {
      if (end_stream) {
        status = SrhinoPluginFramework::v1_0_x::DataStatus::Continue;
      } else {
        filter_status.is_waited_ = true;
        if (filter_status.is_cotinued_) {
          status = SrhinoPluginFramework::v1_0_x::DataStatus::Continue;
        }
      }
    }
  }
}
} // namespace v1_0_x
} // namespace SrhinoPluginLoaderFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy