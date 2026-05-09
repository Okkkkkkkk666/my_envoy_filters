// #include <algorithm>

#include "action.h"

#define MAX_HEADER_LEN 256

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {
namespace Impl {

Action::Action(const v3::Action& action)
    : target_(action.target()), target_header_mathcher_(action.header()),
      proc_next_rule_(action.after_pass() == v3::Action::NEXT_RULE),
      regex_(action.header().safe_regex_match().regex(), re2::RE2::Quiet), quotas_([&action]() {
        Filters::Common::RatelimitClient::Impl::Quotas quotas;
        for (int i = 0; i < action.quotas().size() && i < static_cast<int>(quotas.size()); ++i) {
          const auto& quota = action.quotas(i);
          quotas[i].duration_ = quota.duration();
          quotas[i].max_count_ = quota.max_count();
        }
        adjustQuotaConfig(quotas);
        return quotas;
      }()),
      dryrun_(action.dryrun()) {}

std::string Action::makeKey(const Network::Address::InstanceConstSharedPtr downstreamAddress,
                            const Http::RequestHeaderMap& headers, const std::string& vh_name,
                            uint64_t rule_hash, const std::string& invoke_namespace,
                            const std::string& user_name) const {
  std::string key;

  // 根据target填充key
  std::string target = std::to_string(target_);
  switch (target_) {
  case v3::Action::Target::Action_Target_ALL: {
    key.append(target);
    break;
  }
  case v3::Action::Target::Action_Target_IP: {
    key.append(target);
    key.append(downstreamAddress->ip()->addressAsString());
    break;
  }
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
          key.append(target);
          key.append(sub_value.c_str(),
                     sub_value.length() > MAX_HEADER_LEN ? MAX_HEADER_LEN : sub_value.length());
        }
      } else {
        if (target_header_mathcher_.matchesHeaders(headers)) {
          is_match = true;
          const absl::string_view value_view = result.result().value();
          key.append(target);
          key.append(value_view.data(),
                     value_view.length() > MAX_HEADER_LEN ? MAX_HEADER_LEN : value_view.length());
        }
      }
    }

    // 不匹配则使用IP
    if (!is_match) {
      target = std::to_string(v3::Action::Target::Action_Target_IP);
      key.append(target);
      key.append(downstreamAddress->ip()->addressAsString());
    }
    break;
  }
  case v3::Action::Target::Action_Target_USER: {
    key.append(target);
    key.append(user_name);
  }
  default:
    break;
  }

  // 引入命名空间（插件实例名），避免同一vh绑定相同插件的不同实例导致key相同的问题
  key.append(invoke_namespace);
  key.append(vh_name);
  key.append(std::to_string(rule_hash));

  return key;
}

/**
 * 非法值纠错：
 * 第二个的取值必须在区间(max_count[0], duration[1] / duration[0] * max_count[0])内
 * 比如第一个为每秒5个，第二个为每分钟200个，那么第二个的合法区间为(5,300)
 * 再比如第一个为每7秒2个，第二个设定每两个小时X个，那么X的合法区间是多少？
 * 如果X给定值不在此区间内，该条配额将被废弃
 */
void Action::adjustQuotaConfig(std::array<Filters::Common::RatelimitClient::Impl::Quota, 2>& quotas) {
  // duration从小到大排序，同时无效的排最后
  std::sort(quotas.begin(), quotas.end(), [](Filters::Common::RatelimitClient::Impl::Quota& a, Filters::Common::RatelimitClient::Impl::Quota& b) -> bool {
    if (a.duration_ == 0 && b.duration_ == 0) {
      return true;
    } else if (a.duration_ == 0 && b.duration_ != 0) {
      return false;
    } else if (a.duration_ != 0 && b.duration_ == 0) {
      return true;
    } else {
      return a.duration_ < b.duration_;
    }
  });

  if (quotas[1].duration_ != 0 && quotas[0].duration_ != 0) {
    uint32_t min = quotas[0].max_count_;
    uint32_t max = quotas[1].duration_ / quotas[0].duration_ * quotas[0].max_count_;
    if (quotas[1].max_count_ <= min || quotas[1].max_count_ >= max) {
      // 无效的取值直接废弃
      quotas[1].duration_ = 0;
      quotas[1].max_count_ = 0;
    }
  }
}

} // namespace Impl
} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
