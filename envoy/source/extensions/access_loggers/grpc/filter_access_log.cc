#include "source/extensions/access_loggers/grpc/filter_access_log.h"
#include "envoy/router/router.h"
#include "source/common/router/config_impl.h"

namespace Envoy {
namespace Extensions {
namespace AccessLoggers {
namespace HttpGrpc {

const std::string FilterAccessLog::metadata_filter_name_ =
    "envoy.access_loggers.grpc.filter_access_log";
const std::string FilterAccessLog::key_gateway_name_ = "log.gateway_name";
const std::string FilterAccessLog::key_virtual_host_name_ = "log.virtual_host_name";
const std::string FilterAccessLog::key_cluster_name_ = "log.cluster_name";

const std::string FilterAccessLog::empty_string_;
const std::string FilterAccessLog::cluster_name_direct_ = "_direct_response";
const std::string FilterAccessLog::cluster_name_redirect_ = "_redirect";

FilterAccessLog::FilterAccessLog(const std::string& filter_name,
                                 const Server::Configuration::ServerFactoryContext& context,
                                 StreamInfo::StreamInfo& stream_info)
    : stream_info_(stream_info), context_(context), filter_name_(filter_name) {
    }

std::unordered_map<std::string, std::string>& FilterAccessLog::sharedData() {
  static const absl::string_view data_name("shared_data");
  if (!stream_info_.filterState()->hasData<Envoy::StreamInfo::UserData>(data_name)) {
    stream_info_.filterState()->setData(data_name,
                                        std::make_shared<Envoy::StreamInfo::UserData>(),
                                        Envoy::StreamInfo::FilterState::StateType::Mutable);
  }
  return stream_info_.filterState()->getDataMutable<Envoy::StreamInfo::UserData>(data_name);
}

const std::string& FilterAccessLog::loadSharedData(const std::string& key) {
  auto& map = sharedData();
  const auto it = map.find(key);
  if (it == map.end()) {
    return empty_string_;
  }
  return it->second;
}

void FilterAccessLog::storeSharedData(const std::string& key, const std::string& value) {
  auto& map = sharedData();
  map[key] = value;
}

void FilterAccessLog::log(const std::string& message) {
  if (filter_name_.empty()) {
    return;
  }

  // 每个插件都会写相同的值，这里判断是否已经设置过route_name，如果已经设置过，则忽略后面的设置
  if (loadSharedData(key_gateway_name_).empty()) {
    storeSharedData(key_gateway_name_, getGatewayName(stream_info_));
    storeSharedData(key_virtual_host_name_, getVirtualHostName(context_, stream_info_));
    storeSharedData(key_cluster_name_, getClusterName(stream_info_));
  }

  ProtobufWkt::Struct metadata;
  // std::cout << "filter_name: " << filter_name_ << " msg: " << message << std::endl;
  auto& fields = *metadata.mutable_fields();
  fields[filter_name_].set_string_value(message);
  // fields[key_].set_string_value(message);
  stream_info_.setDynamicMetadata(FilterAccessLog::metadata_filter_name_, metadata);
}

inline const std::string&
FilterAccessLog::getServerNodeId(const Server::Configuration::ServerFactoryContext& context) {
  return const_cast<Server::Configuration::ServerFactoryContext&>(context).bootstrap().node().id();
}

inline const std::string&
FilterAccessLog::getGatewayName(const StreamInfo::StreamInfo& stream_info) {
  auto route = stream_info.route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      auto& vh = entry->virtualHost();
      return vh.routeConfig().name();
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl && route_impl->isDirectResponse()) {
        const Envoy::Router::VirtualHost& vh = route_impl->virtualHost();
        return vh.routeConfig().name();
      }
    }
  }

  return empty_string_;
}

inline std::string
FilterAccessLog::getVirtualHostName(const Server::Configuration::ServerFactoryContext& context,
                                    const StreamInfo::StreamInfo& stream_info) {
  auto route = stream_info.route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      auto& vh = entry->virtualHost();
      return const_cast<Server::Configuration::ServerFactoryContext&>(context)
          .scope()
          .symbolTable()
          .toString(vh.statName());
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl && route_impl->isDirectResponse()) {
        const Envoy::Router::VirtualHost& vh = route_impl->virtualHost();
        return const_cast<Server::Configuration::ServerFactoryContext&>(context)
            .scope()
            .symbolTable()
            .toString(vh.statName());
      }
    }
  }

  return empty_string_;
}

inline const std::string&
FilterAccessLog::getClusterName(const StreamInfo::StreamInfo& stream_info) {
  auto route = stream_info.route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      return entry->clusterName();
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl) {
        if (route_impl->isRedirect()) {
          return cluster_name_redirect_;
        } else if (route_impl->isDirectResponse()) {
          return cluster_name_direct_;
        }
      }
    }
  }

  return empty_string_;
}


// #define KEY_FORMAT(data) ('['+ data + ']')

// inline std::string
// FilterAccessLog::getKey(const Server::Configuration::ServerFactoryContext& context,
//                         const StreamInfo::StreamInfo& stream_info, const std::string& filter_name) {
//   std::string key("[");
//   key.append(getServerNodeId(context));
//   key.append("].[");
//   key.append(getGatewayName(stream_info));
//   key.append("].[");
//   key.append(getVirtualHostName(context, stream_info));
//   key.append("].[");
//   key.append(getClusterName(stream_info));
//   key.append("]:[");
//   key.append(filter_name);
//   key.append("]");

//   return key;
// }

} // namespace HttpGrpc
} // namespace AccessLoggers
} // namespace Extensions
} // namespace Envoy
