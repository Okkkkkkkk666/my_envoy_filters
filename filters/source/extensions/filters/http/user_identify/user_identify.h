#pragma once

#include "absl/strings/match.h"
#include "envoy/http/header_map.h"
#include "source/common/http/headers.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"
#include "filters/source/extensions/filters/http/common/central_database/database.h"
#include "filters/api/envoy/extensions/filters/http/user_identify/v3/user_identify.pb.h"
#include "filters/api/envoy/extensions/filters/http/user_identify/v3/user_identify_log.pb.h"

#include "impl/rule.h"
#include "impl/data_interactive.h"

#define FILTER_NAME "envoy.filters.http.user-identify.1.0"

const std::string SrhinoDomainMetaData("srhino_metadata");
const std::string SrhinoKeyUserName("user_name");
const std::string SrhinoKeyToken("token");
const std::string TokenKeySep("#");
const std::string GuestName("guest");

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {

namespace v3 = envoy::extensions::filters::http::user_identify::v3;
namespace CentralDatabase = Filters::Common::CentralDatabase;

using CustomizePtr = std::shared_ptr<Impl::Customize>;
using IdentifyBasePtr = std::shared_ptr<Impl::IdentifyBase>;
// 全局配置命名
class UserIdentifyFilterGlobalConfig : public Router::RouteSpecificFilterConfig {
public:
  UserIdentifyFilterGlobalConfig(const v3::UserIdentifyGlobal& proto_config,
                                 Server::Configuration::FactoryContext& context,
                                 std::string url_list, std::string user_name_list,
                                 std::string token_list, std::string guest_list);

public:
  IdentifyBasePtr identify() { return identify_; }
  const std::shared_ptr<Impl::DataInteractive>& getDataInterative() const {
    return data_interactive_;
  }

private:
  const std::shared_ptr<Impl::DataInteractive> data_interactive_;
  IdentifyBasePtr identify_;
};

using FilterGlobalConfigSharedPtr = std::shared_ptr<UserIdentifyFilterGlobalConfig>;

// 路由配置
class UserIdentifyFilterRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  UserIdentifyFilterRouteConfig(const v3::UserIdentifyRoute&,
                                Server::Configuration::ServerFactoryContext&, Event::Dispatcher&) {}
};

using FilterRouteConfigSharedPtr = std::shared_ptr<UserIdentifyFilterRouteConfig>;

const std::string SharedDataUserName("user_identify.user_name");

class UserIdentifyFilter : public Http::PassThroughFilterEx,
                           public Logger::Loggable<Logger::Id::filter> {
  friend class UserIdentifyTest_CustomizeMatchUserInHeader_Test;
  friend class UserIdentifyTest_CustomizeMatchUserInBody_Test;
  friend class UserIdentifyTest_CustomizeAndUrlsIsEmpty_Test;
  friend class UserIdentifyTest_CustomizeMatchUserFromBody_Test;
  friend class UserIdentifyTest_AutoModeMatchUserFromBody_Test;
  friend class UserIdentifyTest_CustomizeModeMatchUserFromBodyAndXml_Test;
  friend class UserIdentifyTest_CustomizeMatchUserNoCluster_Test;
  friend class UserIdentifyTest_AutoModeMatchUserFromBodyAndXml_Test;
  friend class UserIdentifyTest_AutoModeAndIsLogin_Test;
  friend class UserIdentifyTest_AutoModeAndIsNotLogin_Test;
  friend class UserIdentifyTest_AutoModeAndIsNotLoginAndNoResponseToken_Test;
  friend class UserIdentifyTest_AutoModeAndIsNotLoginGetGuestToken_Test;
public:
  UserIdentifyFilter(FilterGlobalConfigSharedPtr config,
                     Filters::Common::CentralDatabase::DatabasePtr&& kv_db,
                     const Server::Configuration::ServerFactoryContext& context)
      : Http::PassThroughFilterEx(context), filter_hcm_config_(config), kv_db_(std::move(kv_db)) {}

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance& data, bool end_stream) override;
  void onDestroy() override;
  void onStreamComplete() override;

private:
  inline const std::string& getUpstreamName() const;
  inline const Network::Address::InstanceConstSharedPtr getDownstreamAddress() const;
  void storeLog();
  void storeUserInfoToFilter(const std::string& user_name);
  bool getUserInfo(std::string& user_name);
  int threadIndex();
  inline void token_key_init();
  std::unordered_map<std::string, std::string>& sharedData();

private:
  FilterGlobalConfigSharedPtr filter_hcm_config_;
  Filters::Common::CentralDatabase::DatabasePtr kv_db_;
  static const std::string filter_name_;
  std::string upstream_;
  v3::UserIdentifyLog log_;

  bool deal_flag_{true}; // 是否处理
  enum Impl::ContentType content_type_{Impl::ContentType::CONTENT_TYPE_UNKNOWN};
  enum Impl::TokenRegexType token_type_{Impl::TokenRegexType::TOKEN_TYPE_UNKNOWN};
  bool is_compressed_{false};
  bool is_login_{false};
  std::string user_name_;
  std::string token_;
  std::string token_key_;
  bool is_decode_buffer_full_{false};
  bool is_encode_buffer_full_{false};
  int thread_index_{-1};
  Impl::IdentifyCtxPtr ctx_{nullptr};
};

} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy