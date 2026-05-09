#pragma once

#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "source/extensions/access_loggers/grpc/filter_access_log.h"
#include "source/extensions/filters/http/well_known_names.h"

namespace Envoy {
namespace Http {

class SupportAccessLogWrapper {
protected:
  SupportAccessLogWrapper() {}
  virtual ~SupportAccessLogWrapper() {}

protected:
  void initLog(const Server::Configuration::ServerFactoryContext& context,
               StreamInfo::StreamInfo& stream_info) {
    logger_ = std::make_unique<Extensions::AccessLoggers::HttpGrpc::FilterAccessLog>(
        getFilterName(stream_info), context, stream_info);
  }
public:
  virtual void log(const std::string& message) {
    if (logger_)
      logger_->log(message);
  }
  const std::string& getFilterAccessLogKey()const{return logger_->getKey();}
  std::string getFilterName(const StreamInfo::StreamInfo& stream_info) {
    using namespace Envoy::Extensions::HttpFilters;

    std::string name;
    const auto& metadata = stream_info.dynamicMetadata().filter_metadata();
    auto iter = metadata.find(HttpFilterNames::get().Composite);
    if (iter != metadata.end()) {
      name = iter->second.fields().at("filter_name").string_value();
    }

    return name;
  }

private:
  Extensions::AccessLoggers::HttpGrpc::FilterAccessLogPtr logger_;
};

// A decoder filter which extended the PassThroughDecoderFilter and support for filter's access log.
class PassThroughDecoderFilterEx : public virtual PassThroughDecoderFilter,
                                   public virtual SupportAccessLogWrapper {
public:
  PassThroughDecoderFilterEx(const Server::Configuration::ServerFactoryContext& context)
      : context_(context) {}

public:
  void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override {
    PassThroughDecoderFilter::setDecoderFilterCallbacks(callbacks);
    initLog(context_, callbacks.streamInfo());
  }

private:
  const Server::Configuration::ServerFactoryContext& context_;
};

// An encoder filter which extended the PassThroughEncoderFilter and support for filter's access
// log.
class PassThroughEncoderFilterEx : public virtual PassThroughEncoderFilter,
                                   public virtual SupportAccessLogWrapper {
public:
  PassThroughEncoderFilterEx(const Server::Configuration::ServerFactoryContext& context)
      : context_(context) {}

public:
  void setEncoderFilterCallbacks(Http::StreamEncoderFilterCallbacks& callbacks) override {
    PassThroughEncoderFilter::setEncoderFilterCallbacks(callbacks);
    initLog(context_, callbacks.streamInfo());
  }

private:
  const Server::Configuration::ServerFactoryContext& context_;
};

// A filter which passes all data through with Continue status and support for filter's access log.
class PassThroughFilterEx : public StreamFilter,
                            public PassThroughDecoderFilterEx,
                            public PassThroughEncoderFilterEx {
public:
  PassThroughFilterEx(const Server::Configuration::ServerFactoryContext& context)
      : PassThroughDecoderFilterEx(context), PassThroughEncoderFilterEx(context) {}
};
} // namespace Http
} // namespace Envoy
