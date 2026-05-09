#include "source/common/http/message_impl.h"
#include "filters/source/extensions/filters/http/common/utility/utility.h"

#include "super_glue_client.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

using namespace Envoy::Extensions::Filters::Common::Utility;
const std::string ClientFilter::filter_name_(FILTER_NAME);
const std::string ClientFilterRouteConfig::percent_key_ = "super_glue.http.fixed_percent";
const std::string ClientFilter::unhealthy_response_body_ =
    "No healthy security resource, request rejection";
const std::string ClientFilter::timeout_response_body_ = "Security resource pool request timeout";
const std::string ClientFilter::srhino_domain_metadata_("srhino_metadata");
const std::string ClientFilter::srhino_key_user_name_("user_name");
Cluster::Cluster(const v3::Cluster& cluster)
    : cluster_(cluster.cluster()), unhealthy_bypass_(cluster.unhealthy_bypass()),
      timeout_bypass_(cluster.timeout_bypass()),
      request_timeout_(PROTOBUF_GET_MS_OR_DEFAULT(cluster, request_timeout, 1000)) {}

ClientFilterGlobalConfig::ClientFilterGlobalConfig(const v3::SuperGlueClientGlobal&,
                                                   Server::Configuration::FactoryContext&) {}

ClientFilterRouteConfig::ClientFilterRouteConfig(
    const v3::SuperGlueClientRoute& proto_config,
    Server::Configuration::ServerFactoryContext& context)
    : runtime_(context.runtime()), cluster_manager_(context.clusterManager()),
      enable_(proto_config.enable()), cluster_list_([&proto_config]() {
        std::vector<ClusterPtr> cluster_list;
        for (auto& cluster : proto_config.cluster_list()) {
          cluster_list.emplace_back(std::make_unique<Cluster>(cluster));
        }
        return cluster_list;
      }()),
      rules_([&proto_config]() {
        std::vector<RulePtr> rules;
        for (auto& rule : proto_config.rules()) {
          if (rule.enable()) {
            rules.emplace_back(std::make_unique<Rule>(rule));
          }
        }
        return rules;
      }()),
      percent_age_(std::make_shared<envoy::type::v3::FractionalPercent>(proto_config.percent())) {}

Http::FilterHeadersStatus ClientFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                      bool end_stream) {
  request_headers_ = &headers;

  if (!end_stream || handleRequest()) {
    return Http::FilterHeadersStatus::StopIteration;
  }
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus ClientFilter::decodeData(Buffer::Instance& data, bool end_stream) {
  PROCESS_DECODER_BUFFER_LIMIT(is_buffer_full_);

  if (!decoder_callbacks_->decodingBuffer()) {
    last_body_block_ = data.toString();
  }
  if (!handleRequest()) {
    return Http::FilterDataStatus::Continue;
  }
  return Http::FilterDataStatus::StopIterationAndWatermark;
}

void ClientFilter::onStreamComplete() {
  if (is_match_) {
    log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true));
  }
}

void ClientFilter::onDestroy() {
  if (request_) {
    request_->cancel();
    request_ = nullptr;
  }
}

void ClientFilter::onSuccess(const Http::AsyncClient::Request&,
                             Http::ResponseMessagePtr&& response) {
  request_ = nullptr;
  auto response_status =
      static_cast<Http::Code>(Http::Utility::getResponseStatus(response->headers()));
  // 判断安全资源的响应是否超时
  if (response_status == Http::Code::GatewayTimeout) {
    // 判断是否启用容灾逃生机制
    if (current_cluster_->get()->timeoutBypass()) {
      accessLogAddCluster(v3::ClusterLog_Act_ALLOW);
      request_ = sendNextAsyncHttpRequest();
      if (request_ == nullptr) {
        log_.set_act(true);
        decoder_callbacks_->continueDecoding();
      }
    } else {
      log_.set_act(false);
      accessLogAddCluster(v3::ClusterLog_Act_DENY);
      decoder_callbacks_->sendLocalReply(Http::Code::GatewayTimeout, timeout_response_body_,
                                         nullptr, absl::nullopt, "");
    }
  } else {
    // 判断安全资源是否放行，若响应中存在server插件的标记，说明安全资源放行请求，反之则拒绝请求
    bool from_superglue_server =
        !(response->headers().get(Http::LowerCaseString(SUPER_GLUE_HEADER)).empty());
    if (from_superglue_server) {
      // 从响应中剥离出请求头，替换掉原请求头
      response->headers().iterate([this](const Envoy::Http::HeaderEntry& header) {
        std::string header_key = std::string(header.key().getStringView());
        std::size_t pos = header_key.find(request_header_tag);
        if (pos != std::string::npos) {
          header_key.erase(pos, request_header_tag.length());
          request_headers_->setCopy(Http::LowerCaseString(header_key),
                                    header.value().getStringView());
        }
        return Envoy::Http::HeaderMap::Iterate::Continue;
      });
      accessLogAddCluster(v3::ClusterLog_Act_ALLOW);
      request_ = sendNextAsyncHttpRequest();
      if (request_ == nullptr) {
        // 可视化数据上报
        log_.set_act(true);
        decoder_callbacks_->continueDecoding();
      }
    } else {
      // 可视化数据上报
      accessLogAddCluster(v3::ClusterLog_Act_DENY);
      log_.set_act(false);
      decoder_callbacks_->sendLocalReply(Http::Code::Forbidden, "", nullptr, absl::nullopt, "");
    }
  }
}

void ClientFilter::onFailure(const Http::AsyncClient::Request&, Http::AsyncClient::FailureReason) {
  request_ = nullptr;
  if (current_cluster_->get()->timeoutBypass()) {
    accessLogAddCluster(v3::ClusterLog_Act_ALLOW);
    request_ = sendNextAsyncHttpRequest();
    if (request_ == nullptr) {
      log_.set_act(true);
      decoder_callbacks_->continueDecoding();
    }
  } else {
    log_.set_act(false);
    accessLogAddCluster(v3::ClusterLog_Act_DENY);
    decoder_callbacks_->sendLocalReply(Http::Code::RequestTimeout, timeout_response_body_, nullptr,
                                       absl::nullopt, "");
  }
}

bool ClientFilter::handleRequest() {
  // 判断VH中是否存在配置
  const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
  filter_vh_config_ = getVirtualHostConfig(vh);
  if (filter_vh_config_ == nullptr) {
    return false;
  }

  // 获取路由配置
  const ClientFilterRouteConfig* filter_route_config = getRouteConfig();
  if (filter_route_config) {
    log_.set_level(v3::SuperGlueLog_Level_ROUTE);
    filter_vh_config_ = filter_route_config;
  } else {
    log_.set_level(v3::SuperGlueLog_Level_VH);
  }

  // 判断是否存在资源池
  if (filter_vh_config_->clusterList().empty()) {
    return false;
  }

  // 获取用户名、IP、上游集群名
  const std::string username = getUsername();
  const Network::Address::InstanceConstSharedPtr downstreamAddress = getDownstreamAddress();
  // 规则匹配
  if (!filter_vh_config_->rules().empty()) {
    for (const auto& rule : filter_vh_config_->rules()) {
      if (rule->match(username, downstreamAddress, *request_headers_)) {
        is_match_ = true;
        break;
      }
    }
  } else {
    is_match_ = true;
  }

  if (!is_match_) {
    return false;
  }
  // 按比例进行编排
  if (!filter_vh_config_->getFeatureEnabled()) {
    return false;
  }
  // 异步发送http请求
  request_ = sendFirstAsyncHttpRequest();
  if (request_ == nullptr) {
    log_.set_act(true);
    return false;
  }
  return true;
}

inline const Envoy::Router::VirtualHostImpl* ClientFilter::getVirtualHost() const {
  auto route = decoder_callbacks_->route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      auto& vh = entry->virtualHost();
      return dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl && route_impl->isDirectResponse()) {
        const Envoy::Router::VirtualHost& vh = route_impl->virtualHost();
        return dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
      }
    }
  }

  return nullptr;
}

inline const ClientFilterRouteConfig*
ClientFilter::getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const {
  const ClientFilterRouteConfig* filter_vh_config = nullptr;
  if (vh != nullptr) {
    auto config = vh->perFilterConfig(filter_name_);
    if (config != nullptr) {
      filter_vh_config = dynamic_cast<const ClientFilterRouteConfig*>(config);
    }
  }

  return filter_vh_config;
}

inline const ClientFilterRouteConfig* ClientFilter::getRouteConfig() const {
  const ClientFilterRouteConfig* filter_route_config = nullptr;
  auto route = decoder_callbacks_->route();
  if (route) {
    filter_route_config = dynamic_cast<const ClientFilterRouteConfig*>(
        route->mostSpecificPerFilterConfig(filter_name_));
  }

  return filter_route_config == filter_vh_config_ ? nullptr : filter_route_config;
}

inline const Network::Address::InstanceConstSharedPtr ClientFilter::getDownstreamAddress() const {
  return decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
}

inline std::string ClientFilter::getUsername() const {
  const auto& metadata = decoder_callbacks_->streamInfo().dynamicMetadata().filter_metadata();
  const auto iter = metadata.find(srhino_domain_metadata_);
  if (iter != metadata.end()) {
    const auto iter2 = iter->second.fields().find(srhino_key_user_name_);
    if (iter2 != iter->second.fields().end()) {
      return iter2->second.string_value();
    }
  }

  return EMPTY_STRING;
}

inline const std::string& ClientFilter::getUpstreamName() const {
  static const std::string empty;
  Upstream::ClusterInfoConstSharedPtr cluster =
      decoder_callbacks_->streamInfo().upstreamClusterInfo().has_value()
          ? decoder_callbacks_->streamInfo().upstreamClusterInfo().value()
          : nullptr;

  return cluster ? cluster->name() : empty;
}

Http::AsyncClient::Request* ClientFilter::sendFirstAsyncHttpRequest() {
  // 获取首个安全资源池
  current_cluster_ = filter_vh_config_->clusterList().begin();
  // 判断安全资源池容器是非为空
  if (filter_vh_config_->clusterList().empty()) {
    return nullptr;
  }
  return sendAsyncHttpRequest();
}

Http::AsyncClient::Request* ClientFilter::sendNextAsyncHttpRequest() {
  current_cluster_++;
  // 判断集群容器是否遍历完成
  if (current_cluster_ == filter_vh_config_->clusterList().end()) {
    return nullptr;
  }

  return sendAsyncHttpRequest();
}

Http::AsyncClient::Request* ClientFilter::sendAsyncHttpRequest() {
  Envoy::Upstream::ThreadLocalCluster* cluster = nullptr;
  // 循环获取存在的集群
  for (; current_cluster_ != filter_vh_config_->clusterList().end(); ++current_cluster_) {
    cluster = filter_vh_config_->getThreadLocalCluster(current_cluster_->get()->clusterName());
    if (cluster != nullptr) {
      break;
    }
  }
  if (cluster == nullptr) {
    return nullptr;
  }
  // 获取负载均衡结果
  const auto host = cluster->loadBalancer().peekAnotherHost(nullptr);
  // 判断集群是否健康
  if (host != nullptr) {
    upstream_address_ = host->address()->asString();
  } else {
    upstream_address_ = Envoy::EMPTY_STRING;
    // 判断是否跳过
    if (current_cluster_->get()->unhealthyBypass()) {
      accessLogAddCluster(v3::ClusterLog_Act_ALLOW);
      return sendNextAsyncHttpRequest();
    } else {
      request_ = nullptr;
      accessLogAddCluster(v3::ClusterLog_Act_DENY);
      decoder_callbacks_->sendLocalReply(Http::Code::ServiceUnavailable, unhealthy_response_body_,
                                         nullptr, absl::nullopt, "");
      return nullptr;
    }
  }

  // 创建请求
  Http::RequestMessagePtr request(new Http::RequestMessageImpl(
      Http::createHeaderMap<Http::RequestHeaderMapImpl>(*request_headers_)));

  if (decoder_callbacks_->decodingBuffer()) {
    request->body().add(*decoder_callbacks_->decodingBuffer());
  } else {
    if (!last_body_block_.empty()) {
      request->body().add(last_body_block_);
    }
  }
  // 发送异步http请求
  return cluster->httpAsyncClient().send(
      std::move(request), *this,
      Http::AsyncClient::RequestOptions().setTimeout(current_cluster_->get()->requestTimeout()));
}

/**
 * 可视化日志新增安全资源池数据
 * @param act 当前安全资源池处理结果
 */
inline void ClientFilter::accessLogAddCluster(v3::ClusterLog_Act act) {
  std::time_t current_time = std::time(nullptr);
  auto clusterlog = log_.add_cluster();
  clusterlog->set_cluster(current_cluster_->get()->clusterName());
  clusterlog->set_address(upstream_address_);
  clusterlog->set_act(act);
  clusterlog->set_timestamp(current_time);
  accessLogSetEndpointStatus(clusterlog);
}

// 可视化上报安全资源池所有节点状态
inline void ClientFilter::accessLogSetEndpointStatus(v3::ClusterLog* cluster_log) {
  Envoy::Upstream::ThreadLocalCluster* cluster =
      filter_vh_config_->getThreadLocalCluster(current_cluster_->get()->clusterName());

  const auto& hosts = cluster->prioritySet().hostSetsPerPriority()[0]->hosts();
  for (const auto& host : hosts) {
    // 上报host的地址和健康状态
    auto endpoint = cluster_log->add_endpoints();
    endpoint->set_local_address(host->address()->asString());
    endpoint->set_healthy(host->health() == Upstream::Host::Health::Unhealthy ? false : true);
  }
}

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy