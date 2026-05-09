#include <string>
#include <vector>
#include <string.h>
#include <fstream>
#include <dirent.h>

#include "api_check.h"

#include "filters/api/envoy/extensions/filters/http/api_check/v3/api_check.pb.h"
#include "envoy/stats/scope.h"

#include "envoy/server/filter_config.h"

#include "source/common/common/macros.h"
#include "source/common/http/path_utility.h"
#include "source/common/config/metadata.h"
#include "source/common/http/utility.h"
#include "source/common/http/headers.h"

#include "absl/container/fixed_array.h"

#include "modsecurity/rules_set_properties.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ApiCheckFilter {

FilterRouteConfig::FilterRouteConfig(const v3::ApiCheckRoute& proto_config)
    : disable_request_(false), disable_response_(false) {
  // 限速route构造
  setWafRouteProtoConfig(waf_route_proto_config_);
  waf_route_config_ = std::make_shared<WafFilter::FilterRouteConfig>(waf_route_proto_config_);
}

void FilterRouteConfig::setWafRouteProtoConfig(Impl::waf_v3::WafRoute& waf_route_proto_config) {
  waf_route_proto_config.set_disable_request(false);
  waf_route_proto_config.set_disable_response(false);
}

FilterGlobalConfig::FilterGlobalConfig(const v3::ApiCheckGlobal& proto_config,
                                       const std::string& stats_prefix,
                                       Server::Configuration::FactoryContext& context) {
  setWafGlobalProtoConfig(waf_global_proto_config_);
  waf_global_config_ = std::make_shared<WafFilter::FilterGlobalConfig>(waf_global_proto_config_,
                                                                       stats_prefix, context);
};

FilterGlobalConfig::~FilterGlobalConfig() {}

void FilterGlobalConfig::setWafGlobalProtoConfig(Impl::waf_v3::WafGlobal& waf_global_proto_config) {
  // 设置waf开启检测模式所需要的全局参数
  waf_global_proto_config.set_paranoia_level(waf_check_mode_config_.paranoiaLevel());
  waf_global_proto_config.set_rule_types(waf_check_mode_config_.ruleTypes());
  waf_global_proto_config.set_detection_only(waf_check_mode_config_.detectOnly());
  waf_global_proto_config.set_mode(waf_check_mode_config_.moderation());
}

const WafFilter::FilterRouteConfig*
ApiCheckWaFilter::getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const {
  const FilterRouteConfig* filter_vh_config = nullptr;
  if (vh != nullptr) {
    auto config = vh->perFilterConfig(filter_name);
    if (config != nullptr) {
      filter_vh_config = dynamic_cast<const FilterRouteConfig*>(config);
      if (filter_vh_config != nullptr) {
        return (filter_vh_config->waf_route_config()).get();
      }
    }
  }
  return nullptr;
}

Filter::Filter(FilterGlobalConfigSharedPtr config,
               Server::Configuration::ServerFactoryContext& context)
    : PassThroughFilterEx(context), config_(config) {
  waf_filter_.reset(new ApiCheckWaFilter(config_->waf_global_config(), context));
}

void Filter::onStreamComplete() {
  log_.set_path(waf_filter_->getLog().path());
  log_.set_param(waf_filter_->getLog().param());
  log_.set_downstream(waf_filter_->getLog().downstream());
  log_.set_method(waf_filter_->getLog().method());
  if (waf_filter_->getLog().rule_id_size() > 0) {
    const google::protobuf::RepeatedField<google::protobuf::uint64>& rule_ids =
        waf_filter_->getLog().rule_id();
    for (auto rule_id : rule_ids) {
      log_.add_rule_id(rule_id);
    }
  }
  log(MessageUtil::getJsonStringFromMessageOrDie(log_));
}

void Filter::onDestroy() { waf_filter_->onDestroy(); }

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  waf_filter_->decodeHeaders(headers, end_stream);
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  waf_filter_->decodeData(data, end_stream);
  return Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
  waf_filter_->encodeHeaders(headers, end_stream);
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool end_stream) {
  waf_filter_->encodeData(data, end_stream);
  return Http::FilterDataStatus::Continue;
}

} // namespace ApiCheckFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy