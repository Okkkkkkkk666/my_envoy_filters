#pragma once

#include <chrono>

#include "envoy/http/filter.h"
#include "envoy/upstream/host_description.h"

#include "filters/source/extensions/filters/http/common/lru_cache.hpp"
#include "filters/source/extensions/filters/http/common/central_database/database.h"

#include "filters/api/envoy/extensions/filters/http/strong_stateful_session/v3/strong_stateful_session.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {

namespace v3 = envoy::extensions::filters::http::strong_stateful_session::v3;
namespace CentralDatabase = Filters::Common::CentralDatabase;

// 会话保持所需的上游信息
struct UpstreamInfo {
  uint32_t ip_{0};        // 网络字节序ip
  uint16_t port_{0};      // 端口
  int64_t time_stamp_{0}; // 时间戳，用于本地LRU缓存决定是否跟远程缓存进行同步

  bool operator==(const UpstreamInfo& upstream) const {
    return memcmp(&upstream, this, sizeof(UpstreamInfo)) == 0;
  }

  bool operator!=(const UpstreamInfo& upstream) const { return !(*this == upstream); }
};

// 会话保持基类，各个实现类必须继承此类
class StatefulSession : public Logger::Loggable<Logger::Id::filter> {
public:
  StatefulSession(const v3::StrongStatefulSessionGlobal& proto_config)
      : ttl_(proto_config.ttl().seconds()) {}
  StatefulSession(const v3::StrongStatefulSessionPerRoute& proto_config)
      : ttl_(proto_config.ttl().seconds()) {}
  virtual ~StatefulSession() {}

public:
  /**
   * 获取当前会话保持的上游地址
   * @param headers
   * @param stream_info
   * @param upstream_address 接收上游地址
   * @param cb KV存储客户端
   * @param kv_db KV存储客户端
   * @param cb 同步完缓存后的回调
   * @return 返回真需要同步缓存，返回假则不需要同步缓存
   */
  bool upstreamAddress(const Http::RequestHeaderMap& headers,
                       const StreamInfo::StreamInfo& stream_info, std::string& upstream_address,
                       CentralDatabase::DatabasePtr& kv_db, std::function<void()> cb);

  /**
   * 更新会话保持的上游地址
   * @param host
   * @param request_headers
   * @param response_headers
   * @param stream_info
   * @param kv_db KV存储客户端
   * @param cb 同步完缓存后的回调
   * @return 返回真需要同步缓存，返回假则不需要同步缓存
   */
  bool update(const Upstream::HostDescription& host, const Http::RequestHeaderMap& request_headers,
              Http::ResponseHeaderMap& response_headers, const StreamInfo::StreamInfo& stream_info,
              CentralDatabase::DatabasePtr& kv_db, std::function<void()> cb);
  // 指示负载均衡器选择指定的节点
  static void setUpstreamOverrideHost(Http::StreamDecoderFilterCallbacks* decoder_cb,
                                      const std::string& addr);

protected:
  virtual std::string upstreamAddressInternal(const Http::RequestHeaderMap& headers,
                                              const StreamInfo::StreamInfo& stream_info) = 0;
  virtual std::string updateInternal(const Upstream::HostDescription&,
                                     const Http::RequestHeaderMap& request_headers,
                                     Http::ResponseHeaderMap& response_headers,
                                     const StreamInfo::StreamInfo& stream_info) = 0;

protected:
  // 将32位网络字节序的IP地址转换为点分十进制形式的字符串
  static std::string sockaddrToString(uint32_t addr);
  static int64_t now();

private:
  // 会话保持的上游地址是否失效(超时)
  bool expired(const UpstreamInfo& upstream) const;
  // 从远程控制面数据库中读取数据并同步到本地LRU
  void pull(const std::string& key, CentralDatabase::DatabasePtr& kv_db,
            CentralDatabase::Database::GetCallback cb);
  // 将本地LRU中的数据同步到远程控制面数据库中
  void push(const std::string& key, const UpstreamInfo& upstream,
            CentralDatabase::DatabasePtr& kv_db, CentralDatabase::Database::GetCallback cb);

protected:
  // 会话保持超时时间
  const std::chrono::seconds ttl_;

  // 会话保持的本地缓存（客户端标识、上游地址等信息）。
  // 在多活模式下（多个envoy工作节点），该缓存用于本地加速，即路由请求时，
  // 不需要每次都跟远程控制面的集中缓存进行同步，只需要在会话保持有效期期间同步一次即可。
  // FIXME(zy): 保证引擎节点跟控制面节点时间一致？
  Filters::Common::LruCache<std::string /*key*/, UpstreamInfo /*upstream Info*/,
                            20011 /*must be prime number*/>
      upstream_info_cache_;
};
} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy