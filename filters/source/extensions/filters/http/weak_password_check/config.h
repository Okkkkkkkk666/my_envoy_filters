#pragma once
#include "filters/api/envoy/extensions/filters/http/weak_password_check/v3/weak_password.pb.h"
#include "filters/api/envoy/extensions/filters/http/weak_password_check/v3/weak_password.pb.validate.h"
#include "source/extensions/filters/http/common/factory_base.h"
#include "weak_password_check.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {

class WeakPasswordCheckFilterFactory
    : public Common::FactoryBase<v3::WeakPasswordCheckGlobal, v3::WeakPasswordCheckRoute> {
public:
  WeakPasswordCheckFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::WeakPasswordCheckGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const v3::WeakPasswordCheckRoute& proto_config,
                                       Server::Configuration::ServerFactoryContext& context,
                                       ProtobufMessage::ValidationVisitor& validator) override;
};

DECLARE_FACTORY(WeakPasswordCheckFilterFactory);



}
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
