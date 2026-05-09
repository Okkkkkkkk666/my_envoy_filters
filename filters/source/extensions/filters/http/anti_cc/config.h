#pragma once
#include "source/common/protobuf/utility.h"
#include "source/extensions/filters/http/common/factory_base.h"

#include "filters/api/envoy/extensions/filters/http/anti_cc/v3/anti_cc.pb.h"
#include "filters/api/envoy/extensions/filters/http/anti_cc/v3/anti_cc.pb.validate.h"

#include "anti_cc.h"
#include "filters/source/extensions/filters/http/common/ratelimit/impl/ratelimit_client_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AntiCC {

class AntiCCFilterFactory : public Common::FactoryBase<v3::AntiCCGlobal> {
public:
  AntiCCFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::AntiCCGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;
};
DECLARE_FACTORY(AntiCCFilterFactory);
} // namespace AntiCC
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy