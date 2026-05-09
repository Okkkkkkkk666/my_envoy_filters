#pragma once

#include "envoy/extensions/filters/http/composite/v3/composite.pb.validate.h"

#include "source/common/http/matching/data_impl.h"
#include "source/common/matcher/matcher.h"
#include "source/extensions/filters/http/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Composite {

class ExecuteFilterAction
    : public Matcher::ActionBase<
          envoy::extensions::filters::http::composite::v3::ExecuteFilterAction> {
public:
  explicit ExecuteFilterAction(Http::FilterFactoryCb cb, const std::string& name)
      : cb_(std::move(cb)), name_(name) {}

  void createFilters(Http::FilterChainFactoryCallbacks& callbacks) const;
  const std::string& name() const { return name_; }

private:
  Http::FilterFactoryCb cb_;
  std::string name_;
};

class ExecuteFilterActionFactory
    : public Matcher::ActionFactory<Http::Matching::HttpFilterActionContext> {
public:
  std::string name() const override { return "composite-action"; }
  Matcher::ActionFactoryCb
  createActionFactoryCb(const Protobuf::Message& config,
                        Http::Matching::HttpFilterActionContext& context,
                        ProtobufMessage::ValidationVisitor& validation_visitor) override {
    const auto& composite_action = MessageUtil::downcastAndValidate<
        const envoy::extensions::filters::http::composite::v3::ExecuteFilterAction&>(
        config, validation_visitor);

    auto& factory =
        Config::Utility::getAndCheckFactory<Server::Configuration::NamedHttpFilterConfigFactory>(
            composite_action.typed_config());
    ProtobufTypes::MessagePtr message = Config::Utility::translateAnyToFactoryConfig(
        composite_action.typed_config().typed_config(), validation_visitor, factory);

    // 将name(插件实例ID)设置进metadata
    ProtobufWkt::Struct metadata;
    auto& fields = *metadata.mutable_fields();
    fields["filter_name"].set_string_value(composite_action.typed_config().name());
    auto& listener_metadata =
        const_cast<envoy::config::core::v3::Metadata&>(context.factory_context_.listenerMetadata());
    listener_metadata.mutable_filter_metadata()->insert(
        Protobuf::MapPair<std::string, ProtobufWkt::Struct>(
            Envoy::Extensions::HttpFilters::HttpFilterNames::get().Composite, metadata));

    auto callback = factory.createFilterFactoryFromProto(*message, context.stat_prefix_,
                                                         context.factory_context_);

    // 删除name(插件实例ID)
    listener_metadata.mutable_filter_metadata()->erase(
        Envoy::Extensions::HttpFilters::HttpFilterNames::get().Composite);

    return [cb = std::move(callback),
            name = composite_action.typed_config().name()]() -> Matcher::ActionPtr {
      return std::make_unique<ExecuteFilterAction>(cb, name);
    };
  }
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<envoy::extensions::filters::http::composite::v3::ExecuteFilterAction>();
  }
};
} // namespace Composite
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
