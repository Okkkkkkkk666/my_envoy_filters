#pragma once

#include <string>
#include <memory>

#include "envoy/server/factory_context.h"
#include "envoy/stream_info/stream_info.h"

namespace Envoy {
namespace Extensions {
namespace AccessLoggers {
namespace HttpGrpc {

class FilterAccessLog {
public:
  FilterAccessLog(const std::string& filter_name,
                  const Server::Configuration::ServerFactoryContext& context,
                  StreamInfo::StreamInfo& stream_info);

public:
  const static std::string metadata_filter_name_;
  const static std::string key_gateway_name_;
  const static std::string key_virtual_host_name_;
  const static std::string key_cluster_name_;

public:
  void log(const std::string& message);
  const std::string& getKey()const{return empty_string_;}
private:
  inline const std::string&
  getServerNodeId(const Server::Configuration::ServerFactoryContext& context);
  inline const std::string& getGatewayName(const StreamInfo::StreamInfo& stream_info);
  inline std::string getVirtualHostName(const Server::Configuration::ServerFactoryContext& context,
                                        const StreamInfo::StreamInfo& stream_info);
  inline const std::string& getClusterName(const StreamInfo::StreamInfo& stream_info);
  // inline std::string getKey(const Server::Configuration::ServerFactoryContext& context,
  //                           const StreamInfo::StreamInfo& stream_info, const std::string& filter_name);
  std::unordered_map<std::string, std::string>& sharedData();
  void storeSharedData(const std::string& key, const std::string& value);
  const std::string& loadSharedData(const std::string& key);

private:
  StreamInfo::StreamInfo& stream_info_;
  const Server::Configuration::ServerFactoryContext& context_;
  std::string filter_name_;
  const static std::string empty_string_;
  const static std::string cluster_name_direct_;
  const static std::string cluster_name_redirect_;
};

using FilterAccessLogPtr = std::unique_ptr<FilterAccessLog>;

} // namespace HttpGrpc
} // namespace AccessLoggers
} // namespace Extensions
} // namespace Envoy
