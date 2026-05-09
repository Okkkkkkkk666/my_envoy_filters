#pragma once
#include "filters/api/envoy/extensions/filters/http/user_identify/v3/user_identify.pb.h"
#include "filters/api/envoy/extensions/filters/http/user_identify/v3/user_identify.pb.validate.h"
#include "source/extensions/filters/http/common/factory_base.h"
#include "user_identify.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {

class UserIdentifyFilterFactory
    : public Common::FactoryBase<v3::UserIdentifyGlobal, v3::UserIdentifyRoute>{
public:
    UserIdentifyFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
    Http::FilterFactoryCb
    createFilterFactoryFromProtoTyped(const v3::UserIdentifyGlobal& proto_config,
                                      const std::string& stats_prefix,
                                      Server::Configuration::FactoryContext& context) override;
                
    Router::RouteSpecificFilterConfigConstSharedPtr
    createRouteSpecificFilterConfigTyped(const v3::UserIdentifyRoute& proto_config,
                                         Server::Configuration::ServerFactoryContext& context,
                                         ProtobufMessage::ValidationVisitor& validator) override;
};

DECLARE_FACTORY(UserIdentifyFilterFactory);

}
}
}
}