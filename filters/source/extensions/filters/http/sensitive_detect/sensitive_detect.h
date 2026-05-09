#pragma once

#include "source/extensions/filters/http/common/pass_through_filter_ex.h"
#include "source/common/router/config_impl.h"
#include "source/common/buffer/buffer_impl.h"

#include "filters/api/envoy/extensions/filters/http/sensitive_detect/v3/sensitive_detect.pb.h"
#include "filters/api/envoy/extensions/filters/http/sensitive_detect/v3/sensitive_detect_log.pb.h"
#include "filters/source/extensions/filters/http/common/regex_matcher/regex_matcher.h"
#include "envoy/compression/decompressor/config.h"
#include "envoy/compression/decompressor/decompressor.h"

#define FILTER_NAME "envoy.filters.http.sensitive-detect.1.0"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SensitiveDetect {

namespace v3 = envoy::extensions::filters::http::sensitive_detect::v3;
namespace RegexMatcher = Envoy::Extensions::Filters::Common::RegexMatcher;

namespace Decompressor = Envoy::Compression::Decompressor;

using RegexMatcherPtr = std::shared_ptr<RegexMatcher::RegexMatcher>;

class SensitiveDetectGlobalConfig :
                        public Logger::Loggable<Logger::Id::filter> {
public:
  SensitiveDetectGlobalConfig(const v3::SensitiveDetectGlobal&,
                              Decompressor::DecompressorFactoryPtr decompressor_gzip_factory,
                              Decompressor::DecompressorFactoryPtr decompressor_brotli_factory);

  inline bool enable() const { return enable_; }
  inline RegexMatcherPtr matcher() { return matcher_; }
  Decompressor::DecompressorPtr makeDecompressorGzip() {
    return decompressor_gzip_factory_->createDecompressor("");
  }
  Decompressor::DecompressorPtr makeDecompressorBrotli() {
    return decompressor_brotli_factory_->createDecompressor("");
  }
private:
  const bool enable_;
  RegexMatcherPtr matcher_;
  const Decompressor::DecompressorFactoryPtr decompressor_gzip_factory_;
  const Decompressor::DecompressorFactoryPtr decompressor_brotli_factory_;
};

using SensitiveDetectGlobalConfigSharedPtr = std::shared_ptr<SensitiveDetectGlobalConfig>;

class SensitiveDetectRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  SensitiveDetectRouteConfig(const v3::SensitiveDetectPerRoute&);
};

class SensitiveDetect : public Http::PassThroughEncoderFilterEx,
                        public Logger::Loggable<Logger::Id::filter> {
  friend class SensitiveDetectTest_TestEncodeHeaders_Test;
  friend class SensitiveDetectTest_TestEncodeData_Test;
  friend class SensitiveDetectTest_TestNotMatch_Test;
  friend class SensitiveDetectTest_TestAddMatch_Test;
  friend class SensitiveDetectTest_TestStreamScan_Test;
  friend class SensitiveDetectTest_TestOnStreamComplete_Test;
public:
  SensitiveDetect(SensitiveDetectGlobalConfigSharedPtr config,
                  const Server::Configuration::ServerFactoryContext& context);

  Http::FilterDataStatus encodeData(Buffer::Instance&, bool) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;
  void onStreamComplete() override;
private:
  int threadIndex();
  int processHttpData(Buffer::Instance&);
  v3::SensitiveDetectLog& getLog();

private:
  static const std::string filter_name_;
  const SensitiveDetectGlobalConfigSharedPtr filter_hcm_config_;
  v3::SensitiveDetectLog log_{};
  int thread_index_{-1};
  bool deal_flag_{false}; // 是否处理响应数据
  RegexMatcher::EncodingType encode_type_;
  RegexMatcher::StreamContext stream_ctx_;
  std::vector<RegexMatcher::MatchResult> results_;
  Envoy::Compression::Decompressor::DecompressorPtr decompressor_;
};

} // namespace SensitiveDetect
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
