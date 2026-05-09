#include "sensitive_detect.h"
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SensitiveDetect {

const std::string SensitiveDetect::filter_name_(FILTER_NAME);

static const std::vector<std::string> static_rules = {
    // 0 保留
    "",
    // 1.身份证
    "(\\b)[1-9]\\d{5}((18|19|[23]\\d)\\d{2})(0[1-9]|10|11|12)([0-2][1-9]|10|20|30|31)(\\d{3}[0-9Xx]"
    "\\b)",
    // 2.中国护照
    "(\\b[E|G|P|S|D][A-HJ-NP-Z0-9]\\d{3})\\d{4}(\\b)",
    // 3.港澳通行证
    "\\b((C[0-9A-HJ-NP-Z])[0-9]{7})\\b",
    // 4.银行卡
    "(\\b(62|60|4\\d|5\\d|34|35|37)\\d{2})\\d{7,11}(\\d{4}\\b)",
    // 5.车牌号
    "((京|津|沪|渝|冀|豫|云|辽|黑|湘|皖|鲁|新|苏|浙|赣|鄂|桂|甘|晋|蒙|陕|吉|闽|贵|粤|青|藏|川|宁|"
    "琼|使|领)[A-Z]{1}[A-Z0-9])[A-Z0-9]{2,3}([A-Z0-9][A-Z0-9挂学警港澳])",
    // 6.车辆识别代码
    "\\b([A-HJ-NPR-Z0-9]{8}[X0-9][A-HJ-NPR-Z0-9]{4}([A-HJ-NPR-Z0-9]{4})\\b)",
    // 7.民族
    "((汉族)|(满族)|(蒙古族)|(回族)|(藏族)|(维吾尔族)|(苗族)|(彝族)|(壮族)|(布依族)|(侗族)|(瑶族)|"
    "(白族)|(土家族)|(哈尼族)|(哈萨克族)|(傣族)|(黎族)|(傈僳族)|(佤族)|(畲族)|(高山族)|(拉祜族)|("
    "水族)|(东乡族)|(纳西族)|(景颇族)|(柯尔克孜族)|(土族)|(达斡尔族)|(仫佬族)|(羌族)|(布朗族)|("
    "撒拉族)|(毛南族)|(仡佬族)|(锡伯族)|(阿昌族)|(普米族)|(朝鲜族)|(塔吉克族)|(怒族)|(乌孜别克族)|"
    "(俄罗斯族)|(鄂温克族)|(德昂族)|(保安族)|(裕固族)|(京族)|(塔塔尔族)|(独龙族)|(鄂伦春族)|("
    "赫哲族)|(门巴族)|(珞巴族)|(基诺族))",
    // 8.邮箱
    "(\\b(\\w{3}|\\w{2}|\\w))\\w+?(@\\w+?\\.\\w+?\\b)",
    // 9.移动手机号
    "(\\b1[3456789]\\d)\\d{5}(\\d{3}\\b)",
    // 10.固定手机号
    "([^\\d-](\\d*)-\\d*(\\d{2})[^\\d-])",
    // 11.组织机构代码
    "(\\b[A-Z]{1}[A-Z0-9]{1}[0-9]{6}[A-Z0-9]{9}[A-Z0-9]{1}\\b)",
    // 12.域名
    "([^\\w@](?:[a-zA-Z0-9](?:[a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,}(?:\\.[a-zA-"
    "Z]{2,})?)",
    // 13.ip地址
    "(\\D(2(5[0-5]|[0-4]\\d)|[0-1]?\\d{1,2}))(\\.(2(5[0-5]|[0-4]\\d)|[0-1]?\\d{1,2})){3}\\D",
    // 14.MAC地址
    "\\b([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})\\b"};


SensitiveDetectGlobalConfig::SensitiveDetectGlobalConfig(
    const v3::SensitiveDetectGlobal& global_config,
    Decompressor::DecompressorFactoryPtr decompressor_gzip_factory,
    Decompressor::DecompressorFactoryPtr decompressor_brotli_factory)
    : enable_(global_config.enable()),
      decompressor_gzip_factory_(std::move(decompressor_gzip_factory)),
      decompressor_brotli_factory_(std::move(decompressor_brotli_factory)) {

  matcher_ = std::make_shared<RegexMatcher::RegexMatcher>(RegexMatcher::RegexType::RegexHyperscan,
                                                          false, true);
  std::vector<RegexMatcher::ExpressionView> exprs;
  for (int i = 0; i < global_config.rules_size(); i++) {
    auto& rule = global_config.rules(i);
    auto id = rule.id();
    std::string pattern = rule.pattern();
    if (id < static_rules.size() && id >= 1) {
      pattern = static_rules[id];
    } else if (!rule.pattern().empty()) {
    } else {
      ENVOY_LOG(error, "rule error, id: {}, pattern: {}", id, rule.pattern());
    }
    exprs.emplace_back(RegexMatcher::ExpressionView(pattern, id, true));
    ENVOY_LOG(trace, "add pattern: {}, id: {}", pattern, id);
  }

  std::vector<RegexMatcher::ExpressionView> failed_exprs;
  int ret = matcher_->init(exprs, failed_exprs);
  if (ret < 0) {
    ENVOY_LOG(trace, "regex matcher init failed: {}", ret);
    for (auto& it : failed_exprs) {
      ENVOY_LOG(error, "expression id: {}, {} failed", it.id(), it.expr());
    }
    matcher_ = nullptr;
  }

  ENVOY_LOG(trace, "regex matcher init success");
}

SensitiveDetectRouteConfig::SensitiveDetectRouteConfig(const v3::SensitiveDetectPerRoute&) {}

SensitiveDetect::SensitiveDetect(SensitiveDetectGlobalConfigSharedPtr config,
                                 const Server::Configuration::ServerFactoryContext& context)
    : Http::PassThroughEncoderFilterEx(context), filter_hcm_config_(config) {}

int SensitiveDetect::threadIndex() {
  if (thread_index_ != -1)
    return thread_index_;

  // worker_10
  std::string thread_name;
  if (encoder_callbacks_) {
    thread_name = encoder_callbacks_->dispatcher().name();
  } else {
    return -1;
  }

  auto pos = thread_name.find_first_of('_');
  if (pos == std::string::npos) {
    ENVOY_LOG(error, "Parse thread index failed: {}", thread_name);
    return -1;
  } else {
    thread_index_ = std::atol(thread_name.data() + pos + 1);
  }
  // ENVOY_LOG(trace, "thread index: {}", thread_index_);
  return thread_index_;
}

Http::FilterHeadersStatus SensitiveDetect::encodeHeaders(Http::ResponseHeaderMap& headers,
                                                         [[maybe_unused]] bool end_stream) {
  // 是否启用
  if (!filter_hcm_config_->enable()) {
    return Http::FilterHeadersStatus::Continue;
  }

  // 编码类型
  auto content_type = headers.getContentTypeValue();
  encode_type_ = RegexMatcher::RegexUtilities::getEncodeType(std::string_view(content_type.data(), content_type.length()));
  if (encode_type_ >= RegexMatcher::EncodingType::MAX) {
    ENVOY_LOG(trace, "encoding type {} unsupport", encode_type_);
    return Http::FilterHeadersStatus::Continue;
  }

  // 是否为压缩编码
  auto result = headers.get(Http::CustomHeaders::get().ContentEncoding);
  if (!result.empty()) {
    auto content_encoding = result[0]->value().getStringView();
    if (content_encoding.find("gzip") != std::string::npos) {
      decompressor_ = filter_hcm_config_->makeDecompressorGzip();
    } else if (content_encoding.find("br") != std::string::npos) {
      decompressor_ = filter_hcm_config_->makeDecompressorBrotli();
    } else {
      ENVOY_LOG(error, "Unsupport content encoding type: {}", content_encoding);
      return Http::FilterHeadersStatus::Continue;
    }
  }

  deal_flag_ = true;
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus SensitiveDetect::encodeData(Buffer::Instance& data,
                                                   [[maybe_unused]] bool end_stream) {
  if (!deal_flag_ || data.length() == 0) {
    return Http::FilterDataStatus::Continue;
  }

  // 判断是否为压缩编码
  if (!decompressor_) {
    processHttpData(data);
    return Http::FilterDataStatus::Continue;
  }

  Envoy::Buffer::OwnedImpl buf;
  decompressor_->decompress(data, buf);
  processHttpData(buf);
  return Http::FilterDataStatus::Continue;
}

int SensitiveDetect::processHttpData(Buffer::Instance& data) {
  RegexMatcherPtr regex_matcher = filter_hcm_config_->matcher();
  if (!regex_matcher) {
    return -1;
  }
  if (!stream_ctx_.id) {
    if (regex_matcher->streamOpen(stream_ctx_, 0, encode_type_)) {
      ENVOY_LOG(error, "stream open failed");
      return -1;
    }
  }

  for (const Buffer::RawSlice& slice : data.getRawSlices()) {
    std::string_view slice_data(static_cast<char*>(slice.mem_), slice.len_);
    if (regex_matcher->streamScan(stream_ctx_, slice_data, results_, threadIndex())) {
      ENVOY_LOG(error, "stream scan failed");
      return -1;
    }
  }
  return 0;
}

void SensitiveDetect::onStreamComplete() {
  if (!deal_flag_ || !stream_ctx_.id) {
    return;
  }
  RegexMatcherPtr regex_matcher = filter_hcm_config_->matcher();
  if (!regex_matcher) {
    return;
  }
  if (-1 == regex_matcher->streamClose(stream_ctx_, results_, threadIndex())) {
    ENVOY_LOG(error, "stream close failed");
    return;
  }
  if (stream_ctx_.id) {
    ENVOY_LOG(error, "stream id not null");
  }

  std::set<uint32_t> match_sets;
  std::transform(results_.begin(), results_.end(), std::inserter(match_sets, match_sets.begin()), [](const RegexMatcher::MatchResult& s) {
        return s.id();
    });
  for (auto val : match_sets) {
    // ENVOY_LOG(trace, "match id: {}", val);
    log_.add_sensitives(val);
  }

  if (log_.sensitives_size() > 0) {
    log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true));
  }
}

v3::SensitiveDetectLog& SensitiveDetect::getLog() { return log_; }

} // namespace SensitiveDetect
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
