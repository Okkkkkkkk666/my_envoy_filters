#include <chrono>

#include "source/common/http/header_utility.h"

#include "action.h"

#define MAX_HEADER_LEN 256

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {
namespace Impl {

const Http::LowerCaseString Action::header_ratelimit_drop_("X-RateLimit-Limit");

Action::Action(const v3::Action& action)
    : target_(action.target()), target_header_mathcher_(action.header()), dryrun_(action.dryrun()),
      proc_next_rule_(action.after_pass() == v3::Action::NEXT_RULE),
      regex_(action.header().safe_regex_match().regex(), re2::RE2::Quiet),
      quota_config_([&action]() {
        std::array<QuotaConfig, 2> quota_config;
        for (int i = 0; i < action.quotas().size() && i < static_cast<int>(quota_config.size());
             ++i) {
          const auto& quota = action.quotas().at(i);
          quota_config[i].duration = quota.duration();
          quota_config[i].max_count = quota.max_count();
        }
        adjustQuotaConfig(quota_config);
        return quota_config;
      }()) {}

void Action::proc(const Network::Address::InstanceConstSharedPtr downstreamAddress,
                  Http::RequestHeaderMap& headers, const std::string_view& vh_name,
                  uint32_t filter_instance_id, uint64_t rule_hash, bool dryrun, ProcResult& result,
                  const std::string& user_name) const {
  // 没有设置流控项则表示不限速，直接返回
  if (quota_config_[0].duration == 0 && quota_config_[0].max_count == 0) {
    result.pass = true;
    return;
  }

  // 构造key
  Key key = makeKey(downstreamAddress, headers, vh_name, filter_instance_id, rule_hash, user_name);

  // 获取流控项并处理
  Filters::Common::LruCache<Key, Quotas>& quotas = SingletonQuotas::get();
  quotas.access(key, std::bind(&Action::procQuotas, std::placeholders::_1, std::ref(result)),
                std::bind(&Action::makeQuotas, this));

  if (!result.pass) {
    if (dryrun) {
      result.pass = true;

      // 请求头添加空转标识头
      headers.setReferenceKey(header_ratelimit_drop_, "drop");
    }
  }
}

void Action::cleanQuotas(size_t count) {
  using namespace std::chrono;
  uint64_t now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

  SingletonQuotas::get().clean(count, [&](const Action::Quotas& quotas) -> bool {
    bool remove = true;
    for (const Quota& quota : quotas) {
      if (now - quota.lastTick() < quota.duration() * 1000) {
        remove = false;
        break;
      }
    }

    return remove;
  });
}

size_t Action::quotasSize() { return SingletonQuotas::get().size(); }
size_t Action::quotasMaxSize() { return SingletonQuotas::get().maxSize(); }

Action::Quotas Action::makeQuotas() const {
  return Quotas({Quota(quota_config_[0].duration, quota_config_[0].max_count),
                 Quota(quota_config_[1].duration, quota_config_[1].max_count)});
}

/**
 * key内存布局：
 * +---------------+-----------------------+----------------+------------------+------------------+
 * |target(1 bytes)|target context(n bytes)|vh name(n bytes)|rule hash(8 bytes)|filter id(4 bytes)|
 * +---------------+-----------------------+----------------+------------------+------------------+
 */
Key Action::makeKey(const Network::Address::InstanceConstSharedPtr downstreamAddress,
                    const Http::RequestHeaderMap& headers, const std::string_view& vh_name,
                    uint32_t filter_instance_id, uint64_t rule_hash,
                    const std::string& user_name) const {
  Key key;

  // 根据target填充key
  unsigned char target = static_cast<unsigned char>(target_);
  switch (target) {
  case v3::Action::Target::Action_Target_ALL:
    key.append(&target, sizeof(target));
    break;
  case v3::Action::Target::Action_Target_IP:
    key.append(&target, sizeof(target));
    if (downstreamAddress->ip()->version() == Network::Address::IpVersion::v4) {
      uint32_t ip = downstreamAddress->ip()->ipv4()->address();
      key.append(&ip, sizeof(ip));
    } else {
      absl::uint128 ip = downstreamAddress->ip()->ipv6()->address();
      key.append(&ip, sizeof(ip));
    }
    break;
  case v3::Action::Target::Action_Target_HEADER: {
    bool is_match = false;
    Http::HeaderUtility::GetAllOfHeaderAsStringResult result =
        Http::HeaderUtility::getAllOfHeaderAsString(headers, target_header_mathcher_.name_);
    if (result.result().has_value()) {
      const absl::string_view value_view = result.result().value();
      if (target_header_mathcher_.header_match_type_ ==
          Http::HeaderUtility::HeaderMatchType::Regex) {
        // 正则提取一段值
        std::string sub_value;
        is_match = re2::RE2::Extract(re2::StringPiece(value_view.data(), value_view.size()), regex_,
                                     re2::StringPiece("\\0"), &sub_value);
        if (is_match) {
          key.append(&target, sizeof(target));
          key.append(sub_value.c_str(),
                     sub_value.length() > MAX_HEADER_LEN ? MAX_HEADER_LEN : sub_value.length());
        }
      } else {
        if (target_header_mathcher_.matchesHeaders(headers)) {
          is_match = true;
          const absl::string_view value_view = result.result().value();
          key.append(&target, sizeof(target));
          key.append(value_view.data(),
                     value_view.length() > MAX_HEADER_LEN ? MAX_HEADER_LEN : value_view.length());
        }
      }
    }

    // 不匹配则使用IP
    if (!is_match) {
      target = v3::Action::Target::Action_Target_IP;
      key.append(&target, sizeof(target));
      if (downstreamAddress->ip()->version() == Network::Address::IpVersion::v4) {
        uint32_t ip = downstreamAddress->ip()->ipv4()->address();
        key.append(&ip, sizeof(ip));
      } else {
        absl::uint128 ip = downstreamAddress->ip()->ipv6()->address();
        key.append(&ip, sizeof(ip));
      }
    }
  } break;
  case v3::Action::Target::Action_Target_USER: {
    key.append(&target, sizeof(target));
    key.append(user_name.c_str(), user_name.length());
  }
  default:
    break;
  }

  key.append(vh_name.data(), vh_name.size());
  key.append(&rule_hash, sizeof(rule_hash));
  key.append(&filter_instance_id, sizeof(filter_instance_id));

  return key;
}

// 非法值纠错：
// 第二个的取值必须在区间(max_count[0], duration[1] / duration[0] * max_count[0])内
// 比如第一个为每秒5个，第二个为每分钟200个，那么第二个的合法区间为(5,300)
// 再比如第一个为每7秒2个，第二个设定每两个小时X个，那么X的合法区间是多少？
// 根据上述公式得出X的合法区间为(2, (7200 / 7 = 1028) * 2) 即 (2, 2056)
// 如果X给定值不在此区间内，该条配额将被废弃
void Action::adjustQuotaConfig(std::array<QuotaConfig, 2>& quota_config) {
  // duration从小到大排序，同时无效的排最后
  std::sort(quota_config.begin(), quota_config.end(), [](QuotaConfig& a, QuotaConfig& b) -> bool {
    if (a.duration == 0 && b.duration == 0) {
      return true;
    } else if (a.duration == 0 && b.duration != 0) {
      return false;
    } else if (a.duration != 0 && b.duration == 0) {
      return true;
    } else {
      return a.duration < b.duration;
    }
  });

  if (quota_config[1].duration != 0 && quota_config[0].duration != 0) {
    uint32_t min = quota_config[0].max_count;
    uint32_t max = quota_config[1].duration / quota_config[0].duration * quota_config[0].max_count;
    if (quota_config[1].max_count <= min || quota_config[1].max_count >= max) {
      // 无效的取值直接废弃
      quota_config[1].duration = 0;
      quota_config[1].max_count = 0;
    }
  }
}

void Action::procQuotas(Quotas& quotas, ProcResult& result) {
  result.pass = false;
  result.duration = 0;
  result.remaining = 0;

  // 当前时间戳
  using namespace std::chrono;
  uint64_t now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

  // 处理每个配额
  for (Quota& quota : quotas) {
    if (!quota.valid()) {
      continue;
    }

    if (quota.consume(now)) {
      result.pass = true;
    } else {
      result.pass = false;
      result.duration = quota.duration();
      result.remaining = quota.maxCount() - quota.passCount();
      break;
    }
  }
}

} // namespace Impl
} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy