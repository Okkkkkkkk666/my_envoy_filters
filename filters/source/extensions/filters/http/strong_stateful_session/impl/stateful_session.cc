#include "stateful_session.h"

#include "source/common/network/address_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {
bool StatefulSession::upstreamAddress(const Http::RequestHeaderMap& headers,
                                      const StreamInfo::StreamInfo& stream_info,
                                      std::string& upstream_address,
                                      CentralDatabase::DatabasePtr& kv_db,
                                      std::function<void()> cb) {
  upstream_address.clear();
  std::string key = upstreamAddressInternal(headers, stream_info);
  if (key.empty()) {
    return false;
  }

  // 从本地LRU获取upstream信息
  bool need_sync = true;
  upstream_info_cache_.peek(key, [&](const UpstreamInfo* upstream) {
    if (upstream) {
      ENVOY_LOG(trace, "lookup local cache(key={}),find upstream", key);
      need_sync = false;
      if (!expired(*upstream)) {
        ENVOY_LOG(trace,
                  "upstream not expired. ip_:{}, port_:{}, time_stamp_:{}(ttl:{}, remain:{})",
                  sockaddrToString(upstream->ip_), upstream->port_, upstream->time_stamp_,
                  ttl_.count(), ttl_.count() - (now() - upstream->time_stamp_));
        upstream_address = sockaddrToString(upstream->ip_);
        upstream_address += ":";
        upstream_address += std::to_string(upstream->port_);
      } else {
        ENVOY_LOG(trace, "upstream expired.ip_:{}, port_:{}, time_stamp_:{}(ttl:{})",
                  sockaddrToString(upstream->ip_), upstream->port_, upstream->time_stamp_,
                  ttl_.count());
        need_sync = true;
      }
    } else {
      ENVOY_LOG(trace, "lookup local cache(key={}),did not find upstream", key);
    }
  });

  // 同步缓存
  if (need_sync && kv_db) {
    ENVOY_LOG(trace, "pull the remote cache");
    pull(key, kv_db, [&, cb](int result, const std::string& key, const void* value, size_t size) {
      ENVOY_LOG(debug, "pull complete. result:{} key:{} size:{}", result, key, size);
      if (result > 0) {
        ASSERT(sizeof(UpstreamInfo) == size);
        if (sizeof(UpstreamInfo) == size) {
          upstream_info_cache_.access(
              key,
              [&](UpstreamInfo& upstream) {
                upstream = *(reinterpret_cast<const UpstreamInfo*>(value));
                ENVOY_LOG(
                    debug,
                    "update upstream from local cache. ip_:{}, port_:{}, time_stamp_:{}(remain:{})",
                    sockaddrToString(upstream.ip_), upstream.port_, upstream.time_stamp_,
                    ttl_.count() - (now() - upstream.time_stamp_));
              },
              [&]() {
                const UpstreamInfo* upstream = reinterpret_cast<const UpstreamInfo*>(value);
                ENVOY_LOG(
                    debug,
                    "insert upstream into local cache. ip_:{}, port_:{}, time_stamp_:{}(remain:{})",
                    sockaddrToString(upstream->ip_), upstream->port_, upstream->time_stamp_,
                    ttl_.count() - (now() - upstream->time_stamp_));
                return *(upstream);
              });
        }
      }

      cb();
    });
  }

  return need_sync && kv_db;
}

bool StatefulSession::update(const Upstream::HostDescription& host,
                             const Http::RequestHeaderMap& request_headers,
                             Http::ResponseHeaderMap& response_headers,
                             const StreamInfo::StreamInfo& stream_info,
                             CentralDatabase::DatabasePtr& kv_db, std::function<void()> cb) {
  using namespace std::chrono;

  std::string key = updateInternal(host, request_headers, response_headers, stream_info);
  if (key.empty()) {
    return false;
  }

  UpstreamInfo upstream_info;
  upstream_info.time_stamp_ =
      duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
  if (host.address()->ip()->version() == Network::Address::IpVersion::v4) {
    upstream_info.ip_ = host.address()->ip()->ipv4()->address();
  } else {
    upstream_info.ip_ = 0;
  }
  upstream_info.port_ = host.address()->ip()->port();

  // 本地更新缓存
  bool need_sync = false;
  upstream_info_cache_.access(
      key,
      [&](UpstreamInfo& upstream) {
        // 更新upstream
        if (expired(upstream)) {
          upstream = upstream_info;
          need_sync = true;
          ENVOY_LOG(
              debug,
              "update upstream from local cache(key={}). ip_:{}, port_:{}, time_stamp_:{}(ttl:{}, "
              "remain:{})",
              key, sockaddrToString(upstream.ip_), upstream.port_, upstream.time_stamp_,
              ttl_.count(), ttl_.count() - (now() - upstream.time_stamp_));
        }
      },
      [&]() {
        ENVOY_LOG(debug,
                  "insert upstream into local cache(key={}). ip_:{}, port_:{}, "
                  "time_stamp_:{}(ttl:{}, remain:{})",
                  key, sockaddrToString(upstream_info.ip_), upstream_info.port_,
                  upstream_info.time_stamp_, ttl_.count(),
                  ttl_.count() - (now() - upstream_info.time_stamp_));
        need_sync = true;
        return upstream_info;
      });

  // 同步缓存
  if (need_sync && kv_db) {
    ENVOY_LOG(trace, "push the local cache to remote");
    push(key, upstream_info, kv_db,
         [&, cb](int result, const std::string& key, const void* value, size_t size) {
           ENVOY_LOG(debug, "push complete. result:{} key:{} size:{}", result, key, size);
           if (result > 0) {
             ASSERT(sizeof(UpstreamInfo) == size);
             if (sizeof(UpstreamInfo) == size) {
               const UpstreamInfo* remote_cache_upstream =
                   reinterpret_cast<const UpstreamInfo*>(value);

               // 更新本地LRU缓存
               upstream_info_cache_.access(
                   key,
                   [&](UpstreamInfo& upstream) {
                     if (*remote_cache_upstream != upstream) {
                       upstream = *remote_cache_upstream;
                       ENVOY_LOG(debug,
                                 "update upstream from local cache(key={}). ip_:{}, port_:{}, "
                                 "time_stamp_:{}(ttl:{}, remain:{})",
                                 key, sockaddrToString(upstream.ip_), upstream.port_,
                                 upstream.time_stamp_, ttl_.count(),
                                 ttl_.count() - (now() - upstream.time_stamp_));
                     }
                   },
                   [&]() {
                     const UpstreamInfo* upstream = reinterpret_cast<const UpstreamInfo*>(value);
                     ENVOY_LOG(debug,
                               "insert upstream into local cache(key={}). ip_:{}, port_:{}, "
                               "time_stamp_:{}(ttl:{}, remain:{})",
                               key, sockaddrToString(upstream->ip_), upstream->port_,
                               upstream->time_stamp_, ttl_.count(),
                               ttl_.count() - (now() - upstream->time_stamp_));
                     return *(upstream);
                   });
             }
           }

           cb();
         });
  }

  return need_sync && kv_db;
}

void StatefulSession::setUpstreamOverrideHost(Http::StreamDecoderFilterCallbacks* decoder_cb,
                                              const std::string& addr) {
  decoder_cb->setUpstreamOverrideHost(absl::string_view(addr.c_str()));
}

std::string StatefulSession::sockaddrToString(uint32_t addr) {
  sockaddr_in addr4;
  addr4.sin_family = AF_INET;
  addr4.sin_addr.s_addr = addr;
  addr4.sin_port = 0;
  return Envoy::Network::Address::Ipv4Instance::sockaddrToString(addr4);
}

bool StatefulSession::expired(const UpstreamInfo& upstream) const {
  if (upstream.ip_ == 0 || upstream.time_stamp_ == 0) {
    return true;
  }

  return now() - upstream.time_stamp_ > ttl_.count();
}

void StatefulSession::pull(const std::string& key, CentralDatabase::DatabasePtr& kv_db,
                           CentralDatabase::Database::GetCallback cb) {
  ASSERT(kv_db);
  if (!kv_db) {
    return;
  }

  kv_db->getAsync(key, cb);
}

void StatefulSession::push(const std::string& key, const UpstreamInfo& upstream,
                           Filters::Common::CentralDatabase::DatabasePtr& kv_db,
                           CentralDatabase::Database::GetCallback cb) {
  ASSERT(kv_db);
  if (!kv_db) {
    return;
  }

  // 虽然控制面服务器可以保证insert及get的原子性，但是为防止其他引擎节点
  // 插入相同的key，导致各个节点的本地缓存不一致，这里必须使用get。
  kv_db->getAsync(key, cb, &upstream, sizeof(UpstreamInfo), ttl_.count());
}

int64_t StatefulSession::now() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy