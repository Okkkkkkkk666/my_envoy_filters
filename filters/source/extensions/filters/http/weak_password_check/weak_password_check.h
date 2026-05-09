#pragma once

#include "source/common/router/config_impl.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"
#include "filters/api/envoy/extensions/filters/http/weak_password_check/v3/weak_password_log.pb.h"
#include "filters/api/envoy/extensions/filters/http/weak_password_check/v3/weak_password.pb.h"
#include "impl/rule.h"

#define FILTER_NAME "envoy.filters.http.weak-password-check.1.0"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {

namespace v3 = envoy::extensions::filters::http::weak_password_check::v3;

// 全局配置
class FilterGlobalConfig : public Router::RouteSpecificFilterConfig {
public:
  FilterGlobalConfig(const v3::WeakPasswordCheckGlobal& proto_config,
                     Server::Configuration::FactoryContext&);

public:
  Impl::AutoConfigPtr config() const { return config_; }

private:
  Impl::AutoConfigPtr config_{nullptr};
};
using FilterGlobalConfigSharedPtr = std::shared_ptr<FilterGlobalConfig>;

// VH配置
class FilterRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  FilterRouteConfig(const v3::WeakPasswordCheckRoute&, Server::Configuration::ServerFactoryContext&,
                    Event::Dispatcher&){};
};
using FilterRouteConfigSharedPtr = std::shared_ptr<FilterRouteConfig>;

class Filter : public Http::PassThroughFilterEx, public Logger::Loggable<Logger::Id::filter> {
public:
  Filter(FilterGlobalConfigSharedPtr config,
         const Server::Configuration::ServerFactoryContext& context)
      : Http::PassThroughFilterEx(context), filter_hcm_config_(config){}

  // Http::PassThroughFilterEx
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers, bool) override;
  Http::FilterDataStatus encodeData(Buffer::Instance& data, bool end_stream) override;

  void onStreamComplete() override;
  void onDestroy() override;

  // for test
public:
  bool deal_flag() const { return deal_flag_; }
  void getWeakPasswords(std::vector<std::tuple<bool, bool, std::string, std::string>>& weak_passwords) const {
    for (auto& it : log_.result_info()) {
      weak_passwords.emplace_back(it.is_plain_text(), it.is_weak_password(), it.user_name(), it.password());
    }
  }

private:
  inline const std::string& getUpstreamName() const;
  inline const Network::Address::InstanceConstSharedPtr getDownstreamAddress() const;
  int threadIndex();
  void addLog(bool is_weak, bool is_plain, const std::string& user_name,
              const std::string& password, enum Impl::TrigRuleType type);
  bool weakPasswordProcess(Impl::AutoConfigPtr config);

private:
  static const std::string filter_name_;
  FilterGlobalConfigSharedPtr filter_hcm_config_;

  v3::WeakPasswordLog log_;
  enum Impl::ContentType content_type_{Impl::ContentType::CONTENT_TYPE_UNKNOWN};
  enum Impl::TokenRegexType token_type_{Impl::TokenRegexType::TOKEN_TYPE_UNKNOWN};
  bool deal_flag_{true};
  bool is_compressed_{false};
  int thread_index_{-1};
  bool is_decode_buffer_full_{false};
  bool is_encode_buffer_full_{false};
  std::vector<std::pair<std::string, std::string>> user_info_;
  std::string path_;
};

} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy