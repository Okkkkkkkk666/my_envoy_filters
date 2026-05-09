#include "source/common/formatter/substitution_formatter.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <regex>
#include <string>
#include <vector>

#include "envoy/config/core/v3/base.pb.h"
#include "envoy/upstream/upstream.h"

#include "source/common/api/os_sys_calls_impl.h"
#include "source/common/common/assert.h"
#include "source/common/common/empty_string.h"
#include "source/common/common/fmt.h"
#include "source/common/common/thread.h"
#include "source/common/common/utility.h"
#include "source/common/config/metadata.h"
#include "source/common/grpc/common.h"
#include "source/common/grpc/status.h"
#include "source/common/http/utility.h"
#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/protobuf/utility.h"
#include "source/common/runtime/runtime_features.h"
#include "source/common/stream_info/utility.h"
#include "source/common/common/random_generator.h"
#include "source/common/http/codes.h"

#include "absl/strings/str_split.h"
#include "fmt/format.h"

#include "envoy/config/accesslog/v3/full_access_log.pb.h"

using Envoy::Config::Metadata;

namespace Envoy {
namespace Formatter {

static const std::string DefaultUnspecifiedValueString = "-";

namespace {

const ProtobufWkt::Value& unspecifiedValue() { return ValueUtil::nullValue(); }

void truncate(std::string& str, absl::optional<uint32_t> max_length) {
  if (!max_length) {
    return;
  }

  str = str.substr(0, max_length.value());
}

// Matches newline pattern in a system time format string (e.g. start time)
const std::regex& getSystemTimeFormatNewlinePattern() {
  CONSTRUCT_ON_FIRST_USE(std::regex, "%[-_0^#]*[1-9]*(E|O)?n");
}
const std::regex& getNewlinePattern() { CONSTRUCT_ON_FIRST_USE(std::regex, "\n"); }

} // namespace

const std::string SubstitutionFormatUtils::DEFAULT_FORMAT =
    "[%START_TIME%] \"%REQ(:METHOD)% %REQ(X-ENVOY-ORIGINAL-PATH?:PATH)% %PROTOCOL%\" "
    "%RESPONSE_CODE% %RESPONSE_FLAGS% %BYTES_RECEIVED% %BYTES_SENT% %DURATION% "
    "%RESP(X-ENVOY-UPSTREAM-SERVICE-TIME)% "
    "\"%REQ(X-FORWARDED-FOR)%\" \"%REQ(USER-AGENT)%\" \"%REQ(X-REQUEST-ID)%\" "
    "\"%REQ(:AUTHORITY)%\" \"%UPSTREAM_HOST%\"\n";

FormatterPtr SubstitutionFormatUtils::defaultSubstitutionFormatter() {
  return FormatterPtr{new FormatterImpl(DEFAULT_FORMAT, false)};
}

const absl::optional<std::reference_wrapper<const std::string>>
SubstitutionFormatUtils::protocolToString(const absl::optional<Http::Protocol>& protocol) {
  if (protocol) {
    return Http::Utility::getProtocolString(protocol.value());
  }
  return absl::nullopt;
}

const std::string&
SubstitutionFormatUtils::protocolToStringOrDefault(const absl::optional<Http::Protocol>& protocol) {
  if (protocol) {
    return Http::Utility::getProtocolString(protocol.value());
  }
  return DefaultUnspecifiedValueString;
}

const absl::optional<std::string> SubstitutionFormatUtils::getHostname() {
#ifdef HOST_NAME_MAX
  const size_t len = HOST_NAME_MAX;
#else
  // This is notably the case in OSX.
  const size_t len = 255;
#endif
  char name[len];
  Api::OsSysCalls& os_sys_calls = Api::OsSysCallsSingleton::get();
  const Api::SysCallIntResult result = os_sys_calls.gethostname(name, len);

  absl::optional<std::string> hostname;
  if (result.return_value_ == 0) {
    hostname = name;
  }

  return hostname;
}

const std::string SubstitutionFormatUtils::getHostnameOrDefault() {
  absl::optional<std::string> hostname = getHostname();
  if (hostname.has_value()) {
    return hostname.value();
  }
  return DefaultUnspecifiedValueString;
}

FormatterImpl::FormatterImpl(const std::string& format, bool omit_empty_values)
    : empty_value_string_(omit_empty_values ? EMPTY_STRING : DefaultUnspecifiedValueString) {
  providers_ = SubstitutionFormatParser::parse(format);
}

FormatterImpl::FormatterImpl(const std::string& format, bool omit_empty_values,
                             const std::vector<CommandParserPtr>& command_parsers)
    : empty_value_string_(omit_empty_values ? EMPTY_STRING : DefaultUnspecifiedValueString) {
  providers_ = SubstitutionFormatParser::parse(format, command_parsers);
}

std::string FormatterImpl::format(const Http::RequestHeaderMap& request_headers,
                                  const Http::ResponseHeaderMap& response_headers,
                                  const Http::ResponseTrailerMap& response_trailers,
                                  const StreamInfo::StreamInfo& stream_info,
                                  absl::string_view local_reply_body) const {
  std::string log_line;
  log_line.reserve(256);

  for (const FormatterProviderPtr& provider : providers_) {
    if (!provider->isSupportFormatRef()) {
      const auto bit = provider->format(request_headers, response_headers, response_trailers,
                                        stream_info, local_reply_body);
      log_line += bit.value_or(empty_value_string_);
    } else {
      const std::string& value = provider->formatRef(
          request_headers, response_headers, response_trailers, stream_info, local_reply_body);
      if (value.empty()) {
        log_line += empty_value_string_;
      } else {
        log_line += value;
      }
    }
  }

  return log_line;
}

void uuidToInteger(const std::string& uuid_str, std::string& uuid_integer) {
  uuid_integer.clear();
  uuid_integer.reserve(ID_MAX_LENGTH);

  uint8_t current_byte = 0;
  bool high_nibble = true; // 标记当前处理的是高4位还是低4位
  for (const auto& it : uuid_str) {
    if (it >= '0' && it <= '9') {
      current_byte |= (it - '0');
    } else if (it >= 'a' && it <= 'f') {
      current_byte |= (it - 'a' + 10);
    } else if (it >= 'A' && it <= 'F') {
      current_byte |= (it - 'A' + 10);
    } else {
      continue;
    }

    if (high_nibble) {
      current_byte <<= 4;
    } else {
      if (uuid_integer.size() < ID_MAX_LENGTH) { // 添加边界检查
        uuid_integer.push_back(static_cast<char>(current_byte));
      }
      current_byte = 0;
    }

    high_nibble = !high_nibble;
  }

  if (uuid_integer.size() < ID_MAX_LENGTH) {
    uuid_integer.resize(ID_MAX_LENGTH, 0);
  }
}

std::string protobufFormat(const std::string& data) {
  std::string result;
  if (data.size() <= PACKET_MAX_SIZE) {
    result.resize(data.size() + 4);
    uint32_t length_net = htonl(data.size());
    memcpy(result.data(), &length_net, sizeof(uint32_t));
    memcpy(result.data() + sizeof(uint32_t), data.data(), data.size());
  } else {
    std::string uuid_integer;
    // RandomGeneratorImpl 是多线程安全的
    static Random::RandomGeneratorImpl random;
    uuidToInteger(random.uuid(), uuid_integer);

    uint16_t total_packets = (data.size() - 1) / PACKET_MAX_SIZE + 1;
    uint32_t total_length = data.size() + sizeof(struct PacketHeader) * total_packets;
    const char* start = data.data();
    const char* end = data.data() + data.size();
    uint16_t current_packet_idx = 0;
    uint32_t result_length = 0;

    result.resize(total_length);
    while (start < end) {
      uint32_t packet_length = PACKET_MAX_SIZE;
      if (current_packet_idx == (total_packets - 1)) {
        packet_length = end - start;
      }
      struct PacketHeader header;
      memcpy(header.id, uuid_integer.data(), ID_MAX_LENGTH);
      header.total_packets = htons(total_packets);
      header.current_packet_idx = htons(current_packet_idx++);
      header.length = htonl(packet_length | 0x80000000);
      memcpy(result.data() + result_length, &header, sizeof(struct PacketHeader));

      result_length += sizeof(struct PacketHeader);
      memcpy(result.data() + result_length, start, packet_length);

      start += packet_length;
      result_length += packet_length;
    }
    ASSERT(result_length == result.size());
  }

  return result;
}

bool JsonFormatterImpl::findField(const ProtobufWkt::Struct& struct_format, const std::string& field_name) const {
  for (const auto& pair : struct_format.fields()) {
    if ((pair.second.kind_case() == ProtobufWkt::Value::kStringValue) &&
        (pair.first == field_name)) {
      return true;
    }
  }
  return false;
}

std::string JsonFormatterImpl::format(const Http::RequestHeaderMap& request_headers,
                                      const Http::ResponseHeaderMap& response_headers,
                                      const Http::ResponseTrailerMap& response_trailers,
                                      const StreamInfo::StreamInfo& stream_info,
                                      absl::string_view local_reply_body) const {
  ProtobufWkt::Struct output_struct = struct_formatter_.format(
      request_headers, response_headers, response_trailers, stream_info, local_reply_body);
  if (is_proto_format_) {

    // 由于body中含有特殊字符，在json转message时会出现错误。因此，这里将body字段提取出来，手动添加到message中。
    // dynamic_metadata_filter是bytes类型，json转message时失败。需要先提取出来
    // 直接清空metadata，全量的访问日志目前暂时不需要插件日志信息
    auto& members = *(output_struct.mutable_fields());
    ata::v1::FullAccessLog log;

    log.set_request_id(members.at("request_id").string_value());
    log.set_node_id(members.at("node_id").string_value());
    log.set_engine_type(ata::v1::EngineType::TARFFIC);
    log.set_plugin_hit(StreamInfoFormatter::isPluginHit(stream_info));

    // log.mutable_user_info()->set_unique_id(members.at("user_info").mutable_struct_value()->mutable_fields()->at("unique_id").string_value());
    // log.mutable_user_info()->set_name(members.at("user_info").mutable_struct_value()->mutable_fields()->at("name").string_value());
    // log.mutable_user_info()->set_app_id(members.at("user_info").mutable_struct_value()->mutable_fields()->at("app_id").string_value());
    // log.mutable_user_info()->set_rule_id(members.at("user_info").mutable_struct_value()->mutable_fields()->at("rule_id").string_value());


    // clang-format off
    try {
      log.mutable_filter_log()->set_gateway_name(members.at("filter_log").mutable_struct_value()->mutable_fields()->at("gateway_name").string_value());
      log.mutable_filter_log()->set_virtual_host_name(members.at("filter_log").mutable_struct_value()->mutable_fields()->at("virtual_host_name").string_value());
      log.mutable_filter_log()->set_cluster_name(members.at("filter_log").mutable_struct_value()->mutable_fields()->at("cluster_name").string_value());
      auto& field = *members.at("connection_info").mutable_struct_value()->mutable_fields()->at("dynamic_metadata_filter").mutable_struct_value()->mutable_fields();
      for (auto it = field.begin(); it != field.end(); ++it) {
        auto instance_info = log.mutable_filter_log()->add_instance_info();
        instance_info->set_name(it->first);
        instance_info->set_log(it->second.string_value());
      }

      log.mutable_connection_info()->set_downstream_remote_address(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("downstream_remote_address").string_value());
      log.mutable_connection_info()->set_virtual_cluster_name(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("virtual_cluster_name").string_value());
      log.mutable_connection_info()->set_listener_stat_prefix(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("listener_stat_prefix").string_value());
      log.mutable_connection_info()->set_response_tx_duration(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("response_tx_duration").number_value());
      log.mutable_connection_info()->set_bytes_received(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("bytes_received").number_value());
      log.mutable_connection_info()->set_bytes_sent(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("bytes_sent").number_value());
      log.mutable_connection_info()->set_downstream_direct_remote_address(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("downstream_direct_remote_address").string_value());
      log.mutable_connection_info()->set_upstream_host(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("upstream_host").string_value());
      log.mutable_connection_info()->set_upstream_local_address(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("upstream_local_address").string_value());
      log.mutable_connection_info()->set_duration(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("duration").number_value());
      log.mutable_connection_info()->set_request_duration(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("request_duration").number_value());
      log.mutable_connection_info()->set_request_tx_duration(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("request_tx_duration").number_value());
      log.mutable_connection_info()->set_response_duration(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("response_duration").number_value());
      log.mutable_connection_info()->set_upstream_cluster(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("upstream_cluster").string_value());
      // log.mutable_connection_info()->set_dynamic_metadata_filter(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("dynamic_metadata_filter").string_value());
      log.mutable_connection_info()->set_start_time(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("start_time").number_value());
      log.mutable_connection_info()->set_origin_host(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("origin_host").string_value());
      log.mutable_connection_info()->set_downstream_local_address(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("downstream_local_address").string_value());
      log.mutable_connection_info()->set_route_name(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("route_name").string_value());
      log.mutable_connection_info()->set_client_device_fingerprint(StreamInfoFormatter::getUniqueId(stream_info));
      log.mutable_connection_info()->set_response_code_details(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("response_code_details").string_value());
      log.mutable_connection_info()->set_response_trailers_bytes(members.at("connection_info").mutable_struct_value()->mutable_fields()->at("response_trailers_bytes").number_value());

      log.mutable_request()->set_raw_start_line(members.at("request").mutable_struct_value()->mutable_fields()->at("raw_start_line").string_value());
      log.mutable_request()->set_raw_headers(members.at("request").mutable_struct_value()->mutable_fields()->at("raw_headers").string_value());
      for (const auto& it : members.at("request").struct_value().fields().at("raw_body").list_value().values()) {
        log.mutable_request()->add_raw_body(it.string_value());
      }

      log.mutable_response()->set_raw_start_line(members.at("response").mutable_struct_value()->mutable_fields()->at("raw_start_line").string_value());
      log.mutable_response()->set_raw_headers(members.at("response").mutable_struct_value()->mutable_fields()->at("raw_headers").string_value());
      for (const auto& it : members.at("response").struct_value().fields().at("raw_body").list_value().values()) {
        log.mutable_response()->add_raw_body(it.string_value());
      }
    } catch (std::exception &e) {
      std::cout << "exception: " << e.what() << std::endl;
      return std::string();
    }
    // clang-format on

    return protobufFormat(log.SerializeAsString());
  } else {
    const std::string log_line =
        MessageUtil::getJsonStringFromMessageOrDie(output_struct, false, true);
    return absl::StrCat(log_line, "\n");
  }
}

std::string ProtoFormatterImpl::format(const Http::RequestHeaderMap& request_headers,
                                      const Http::ResponseHeaderMap& response_headers,
                                      const Http::ResponseTrailerMap& response_trailers,
                                      const StreamInfo::StreamInfo& stream_info,
                                      absl::string_view local_reply_body) const {
  const ProtobufWkt::Struct output_struct = struct_formatter_.format(
      request_headers, response_headers, response_trailers, stream_info, local_reply_body);
  return protobufFormat(output_struct.SerializeAsString());
}

StructFormatter::StructFormatter(const ProtobufWkt::Struct& format_mapping, bool preserve_types,
                                 bool omit_empty_values,
                                 const std::vector<CommandParserPtr>& commands)
    : omit_empty_values_(omit_empty_values), preserve_types_(preserve_types),
      empty_value_(omit_empty_values_ ? EMPTY_STRING : DefaultUnspecifiedValueString),
      struct_output_format_(FormatBuilder(commands).toFormatMapValue(format_mapping)) {}

StructFormatter::StructFormatter(const ProtobufWkt::Struct& format_mapping, bool preserve_types,
                                 bool omit_empty_values)
    : omit_empty_values_(omit_empty_values), preserve_types_(preserve_types),
      empty_value_(omit_empty_values_ ? EMPTY_STRING : DefaultUnspecifiedValueString),
      struct_output_format_(FormatBuilder().toFormatMapValue(format_mapping)) {}

StructFormatter::StructFormatMapWrapper
StructFormatter::FormatBuilder::toFormatMapValue(const ProtobufWkt::Struct& struct_format) const {
  auto output = std::make_unique<StructFormatMap>();
  for (const auto& pair : struct_format.fields()) {
    switch (pair.second.kind_case()) {
    case ProtobufWkt::Value::kStringValue:
      if (pair.first != JsonFormatterFieldNameProtoFormat && pair.first != JsonFormatterFieldNameSendToNsq) {
        output->emplace(pair.first, toFormatStringValue(pair.second.string_value()));
      }
      break;

    case ProtobufWkt::Value::kStructValue:
      output->emplace(pair.first, toFormatMapValue(pair.second.struct_value()));
      break;

    case ProtobufWkt::Value::kListValue:
      output->emplace(pair.first, toFormatListValue(pair.second.list_value()));
      break;

    default:
      throw EnvoyException("Only string values, nested structs and list values are "
                           "supported in structured access log format.");
    }
  }
  return {std::move(output)};
}

StructFormatter::StructFormatListWrapper StructFormatter::FormatBuilder::toFormatListValue(
    const ProtobufWkt::ListValue& list_value_format) const {
  auto output = std::make_unique<StructFormatList>();
  for (const auto& value : list_value_format.values()) {
    switch (value.kind_case()) {
    case ProtobufWkt::Value::kStringValue:
      output->emplace_back(toFormatStringValue(value.string_value()));
      break;

    case ProtobufWkt::Value::kStructValue:
      output->emplace_back(toFormatMapValue(value.struct_value()));
      break;

    case ProtobufWkt::Value::kListValue:
      output->emplace_back(toFormatListValue(value.list_value()));
      break;
    default:
      throw EnvoyException("Only string values, nested structs and list values are "
                           "supported in structured access log format.");
    }
  }
  return {std::move(output)};
}

std::vector<FormatterProviderPtr>
StructFormatter::FormatBuilder::toFormatStringValue(const std::string& string_format) const {
  std::vector<CommandParserPtr> commands;
  return SubstitutionFormatParser::parse(string_format, commands_.value_or(commands));
}

ProtobufWkt::Value StructFormatter::providersCallback(
    const std::vector<FormatterProviderPtr>& providers,
    const Http::RequestHeaderMap& request_headers, const Http::ResponseHeaderMap& response_headers,
    const Http::ResponseTrailerMap& response_trailers, const StreamInfo::StreamInfo& stream_info,
    absl::string_view local_reply_body) const {
  ASSERT(!providers.empty());
  if (providers.size() == 1) {
    const auto& provider = providers.front();
    if (preserve_types_) {
      return provider->formatValue(request_headers, response_headers, response_trailers,
                                   stream_info, local_reply_body);
    }

    if (omit_empty_values_) {
      return ValueUtil::optionalStringValue(provider->format(
          request_headers, response_headers, response_trailers, stream_info, local_reply_body));
    }

    const auto str = provider->format(request_headers, response_headers, response_trailers,
                                      stream_info, local_reply_body);
    return ValueUtil::stringValue(str.value_or(DefaultUnspecifiedValueString));
  }
  // Multiple providers forces string output.
  std::string str;
  for (const auto& provider : providers) {
    const auto bit = provider->format(request_headers, response_headers, response_trailers,
                                      stream_info, local_reply_body);
    str += bit.value_or(empty_value_);
  }
  return ValueUtil::stringValue(str);
}

ProtobufWkt::Value StructFormatter::structFormatMapCallback(
    const StructFormatter::StructFormatMapWrapper& format_map,
    const StructFormatter::StructFormatMapVisitor& visitor) const {
  ProtobufWkt::Struct output;
  auto* fields = output.mutable_fields();
  for (const auto& pair : *format_map.value_) {
    ProtobufWkt::Value value = absl::visit(visitor, pair.second);
    if (omit_empty_values_ && value.kind_case() == ProtobufWkt::Value::kNullValue) {
      continue;
    }
    (*fields)[pair.first] = value;
  }
  return ValueUtil::structValue(output);
}

ProtobufWkt::Value StructFormatter::structFormatListCallback(
    const StructFormatter::StructFormatListWrapper& format_list,
    const StructFormatter::StructFormatMapVisitor& visitor) const {
  std::vector<ProtobufWkt::Value> output;
  for (const auto& val : *format_list.value_) {
    ProtobufWkt::Value value = absl::visit(visitor, val);
    if (omit_empty_values_ && value.kind_case() == ProtobufWkt::Value::kNullValue) {
      continue;
    }
    output.push_back(value);
  }
  return ValueUtil::listValue(output);
}

ProtobufWkt::Struct StructFormatter::format(const Http::RequestHeaderMap& request_headers,
                                            const Http::ResponseHeaderMap& response_headers,
                                            const Http::ResponseTrailerMap& response_trailers,
                                            const StreamInfo::StreamInfo& stream_info,
                                            absl::string_view local_reply_body) const {
  StructFormatMapVisitor visitor{
      [&](const std::vector<FormatterProviderPtr>& providers) {
        return providersCallback(providers, request_headers, response_headers, response_trailers,
                                 stream_info, local_reply_body);
      },
      [&, this](const StructFormatter::StructFormatMapWrapper& format_map) {
        return structFormatMapCallback(format_map, visitor);
      },
      [&, this](const StructFormatter::StructFormatListWrapper& format_list) {
        return structFormatListCallback(format_list, visitor);
      },
  };
  return structFormatMapCallback(struct_output_format_, visitor).struct_value();
}

void SubstitutionFormatParser::parseCommandHeader(const std::string& token, const size_t start,
                                                  std::string& main_header,
                                                  std::string& alternative_header,
                                                  absl::optional<size_t>& max_length) {
  // subs is used only to check if there are more than 2 tokens separated by '?'.
  std::vector<std::string> subs;
  alternative_header = "";
  parseCommand(token, start, '?', max_length, main_header, alternative_header, subs);
  if (!subs.empty()) {
    throw EnvoyException(
        // Header format rules support only one alternative header.
        // docs/root/configuration/observability/access_log/access_log.rst#format-rules
        absl::StrCat("More than 1 alternative header specified in token: ", token));
  }

  // The main and alternative header should not contain invalid characters {NUL, LR, CF}.
  if (std::regex_search(main_header, getNewlinePattern()) ||
      std::regex_search(alternative_header, getNewlinePattern())) {
    throw EnvoyException("Invalid header configuration. Format string contains newline.");
  }
}

void SubstitutionFormatParser::tokenizeCommand(const std::string& command, const size_t start,
                                               const char separator,
                                               std::vector<absl::string_view>& tokens,
                                               absl::optional<size_t>& max_length) {
  const size_t end_request = command.find(')', start);
  tokens.clear();
  if (end_request != command.length() - 1) {
    // Closing bracket is not found.
    if (end_request == std::string::npos) {
      throw EnvoyException(absl::StrCat("Closing bracket is missing in token: ", command));
    }

    // Closing bracket should be either last one or followed by ':' to denote limitation.
    if (command[end_request + 1] != ':') {
      throw EnvoyException(absl::StrCat("Incorrect position of ')' in token: ", command));
    }

    const auto length_str = absl::string_view(command).substr(end_request + 2);
    uint64_t length_value;

    if (!absl::SimpleAtoi(length_str, &length_value)) {
      throw EnvoyException(absl::StrCat("Length must be an integer, given: ", length_str));
    }

    max_length = length_value;
  }

  absl::string_view name_data(command);
  name_data.remove_prefix(start);
  name_data.remove_suffix(command.length() - end_request);
  tokens = absl::StrSplit(name_data, separator);
}

std::vector<FormatterProviderPtr> SubstitutionFormatParser::parse(const std::string& format) {
  return SubstitutionFormatParser::parse(format, {});
}

FormatterProviderPtr SubstitutionFormatParser::parseBuiltinCommand(const std::string& token) {
  static constexpr absl::string_view DYNAMIC_META_TOKEN{"DYNAMIC_METADATA("};
  static constexpr absl::string_view CLUSTER_META_TOKEN{"CLUSTER_METADATA("};
  static constexpr absl::string_view FILTER_STATE_TOKEN{"FILTER_STATE("};
  static constexpr absl::string_view PLAIN_SERIALIZATION{"PLAIN"};
  static constexpr absl::string_view TYPED_SERIALIZATION{"TYPED"};

  if (absl::StartsWith(token, "REQ(")) {
    std::string main_header, alternative_header;
    absl::optional<size_t> max_length;

    parseCommandHeader(token, ReqParamStart, main_header, alternative_header, max_length);

    return std::make_unique<RequestHeaderFormatter>(main_header, alternative_header, max_length);
  } else if (absl::StartsWith(token, "RESP(")) {
    std::string main_header, alternative_header;
    absl::optional<size_t> max_length;

    parseCommandHeader(token, RespParamStart, main_header, alternative_header, max_length);

    return std::make_unique<ResponseHeaderFormatter>(main_header, alternative_header, max_length);
  } else if (absl::StartsWith(token, "TRAILER(")) {
    std::string main_header, alternative_header;
    absl::optional<size_t> max_length;

    parseCommandHeader(token, TrailParamStart, main_header, alternative_header, max_length);

    return std::make_unique<ResponseTrailerFormatter>(main_header, alternative_header, max_length);
  } else if (absl::StartsWith(token, "REQ_LINE")) {
    return std::make_unique<RequestLineFormatter>();
  } else if (absl::StartsWith(token, "RESP_LINE")) {
    return std::make_unique<ResponseLineFormatter>();
  } else if (absl::StartsWith(token, "NODE_ID")) {
    return std::make_unique<NodeIdFormatter>();
  } else if (absl::StartsWith(token, "LISTENER_STAT_PREFIX")) {
    return std::make_unique<ListenerStatPrefixFormatter>();
  } else if (absl::EndsWith(token, "ALL_REQ_HEADER")) {
    return std::make_unique<AllRequestHeaderFormatter>(true);
  } else if (absl::EndsWith(token, "ALL_REQ_HEADER_BYTES")) {
    return std::make_unique<AllRequestHeaderFormatter>(false);
  } else if (absl::EndsWith(token, "ALL_RESP_HEADER")) {
    return std::make_unique<AllResponseHeaderFormatter>(true);
  } else if (absl::EndsWith(token, "ALL_RESP_HEADER_BYTES")) {
    return std::make_unique<AllResponseHeaderFormatter>(false);
  } else if (absl::StartsWith(token, "LOCAL_REPLY_BODY")) {
    return std::make_unique<LocalReplyBodyFormatter>();
  } else if (absl::StartsWith(token, DYNAMIC_META_TOKEN)) {
    std::string filter_namespace;
    absl::optional<size_t> max_length;
    std::vector<std::string> path;
    const size_t start = DYNAMIC_META_TOKEN.size();

    parseCommand(token, start, ':', max_length, filter_namespace, path);
    return std::make_unique<DynamicMetadataFormatter>(filter_namespace, path, max_length);
  } else if (absl::StartsWith(token, CLUSTER_META_TOKEN)) {
    std::string filter_namespace;
    absl::optional<size_t> max_length;
    std::vector<std::string> path;
    const size_t start = CLUSTER_META_TOKEN.size();

    parseCommand(token, start, ':', max_length, filter_namespace, path);
    return std::make_unique<ClusterMetadataFormatter>(filter_namespace, path, max_length);
  } else if (absl::StartsWith(token, FILTER_STATE_TOKEN)) {
    std::string key, serialize_type;
    absl::optional<size_t> max_length;
    std::string path;
    const size_t start = FILTER_STATE_TOKEN.size();

    parseCommand(token, start, ':', max_length, key, serialize_type);
    if (key.empty()) {
      throw EnvoyException("Invalid filter state configuration, key cannot be empty.");
    }

    if (serialize_type.empty()) {
      serialize_type = std::string(TYPED_SERIALIZATION);
    }
    if (serialize_type != PLAIN_SERIALIZATION && serialize_type != TYPED_SERIALIZATION) {
      throw EnvoyException("Invalid filter state serialize type, only support PLAIN/TYPED.");
    }
    const bool serialize_as_string = serialize_type == PLAIN_SERIALIZATION;

    return std::make_unique<FilterStateFormatter>(key, max_length, serialize_as_string);
  } else if (absl::StartsWith(token, "START_TIME")) {
    return std::make_unique<StartTimeFormatter>(token);
  } else if (absl::StartsWith(token, "INT64_START_TIME")) {
    return std::make_unique<StartTimeInt64Formatter>(token);
  } else if (absl::StartsWith(token, "DOWNSTREAM_PEER_CERT_V_START")) {
    return std::make_unique<DownstreamPeerCertVStartFormatter>(token);
  } else if (absl::StartsWith(token, "DOWNSTREAM_PEER_CERT_V_END")) {
    return std::make_unique<DownstreamPeerCertVEndFormatter>(token);
  } else if (absl::StartsWith(token, "GRPC_STATUS")) {
    return std::make_unique<GrpcStatusFormatter>("grpc-status", "", absl::optional<size_t>());
  } else if (absl::StartsWith(token, "REQUEST_HEADERS_BYTES")) {
    return std::make_unique<HeadersByteSizeFormatter>(
        HeadersByteSizeFormatter::HeaderType::RequestHeaders);
  } else if (absl::StartsWith(token, "RESPONSE_HEADERS_BYTES")) {
    return std::make_unique<HeadersByteSizeFormatter>(
        HeadersByteSizeFormatter::HeaderType::ResponseHeaders);
  } else if (absl::StartsWith(token, "RESPONSE_TRAILERS_BYTES")) {
    return std::make_unique<HeadersByteSizeFormatter>(
        HeadersByteSizeFormatter::HeaderType::ResponseTrailers);
  }

  return nullptr;
}

// TODO(derekargueta): #2967 - Rewrite SubstitutionFormatter with parser library & formal grammar
std::vector<FormatterProviderPtr>
SubstitutionFormatParser::parse(const std::string& format,
                                const std::vector<CommandParserPtr>& commands) {
  std::string current_token;
  std::vector<FormatterProviderPtr> formatters;
  const std::regex command_w_args_regex(R"EOF(^%([A-Z]|[0-9]|_)+(\([^\)]*\))?(:[0-9]+)?(%))EOF");

  for (size_t pos = 0; pos < format.length(); ++pos) {
    if (format[pos] != '%') {
      current_token += format[pos];
      continue;
    }

    if (!current_token.empty()) {
      formatters.emplace_back(FormatterProviderPtr{new PlainStringFormatter(current_token)});
      current_token = "";
    }

    std::smatch m;
    const std::string search_space = format.substr(pos);
    if (!std::regex_search(search_space, m, command_w_args_regex)) {
      throw EnvoyException(fmt::format(
          "Incorrect configuration: {}. Couldn't find valid command at position {}", format, pos));
    }

    const std::string match = m.str(0);
    const std::string token = match.substr(1, match.length() - 2);
    pos += 1;
    const size_t command_end_position = pos + token.length();

    auto formatter = parseBuiltinCommand(token);
    if (formatter) {
      formatters.push_back(std::move(formatter));
    } else {
      // Check formatter extensions. These are used for anything not provided by the built-in
      // operators, e.g.: specialized formatting, computing stats from request/response headers
      // or from stream info, etc.
      bool added = false;
      for (const auto& cmd : commands) {
        auto formatter = cmd->parse(token, pos, command_end_position);
        if (formatter) {
          formatters.push_back(std::move(formatter));
          added = true;
          break;
        }
      }

      if (!added) {
        formatters.emplace_back(FormatterProviderPtr{new StreamInfoFormatter(token)});
      }
    }

    pos = command_end_position;
  }

  if (!current_token.empty() || format.empty()) {
    // Create a PlainStringFormatter with the final string literal. If the format string was empty,
    // this creates a PlainStringFormatter with an empty string.
    formatters.emplace_back(FormatterProviderPtr{new PlainStringFormatter(current_token)});
  }

  return formatters;
}

// StreamInfo std::string field extractor.
class StreamInfoStringFieldExtractor : public StreamInfoFormatter::FieldExtractor {
public:
  using FieldExtractor = std::function<absl::optional<std::string>(const StreamInfo::StreamInfo&)>;

  StreamInfoStringFieldExtractor(FieldExtractor f) : field_extractor_(f) {}

  // StreamInfoFormatter::FieldExtractor
  absl::optional<std::string> extract(const StreamInfo::StreamInfo& stream_info) const override {
    return field_extractor_(stream_info);
  }
  ProtobufWkt::Value extractValue(const StreamInfo::StreamInfo& stream_info) const override {
    return ValueUtil::optionalStringValue(field_extractor_(stream_info));
  }

private:
  FieldExtractor field_extractor_;
};

// 跟StreamInfoStringFieldExtractor类似。只是增加了返回类型为const std::string&的extractRef
// 方法，主要是为了避免不必要的拷贝，提高性能。
class StreamInfoStringRefFieldExtractor : public StreamInfoFormatter::FieldExtractor {
public:
  using FieldExtractor = std::function<const std::string&(const StreamInfo::StreamInfo&)>;

  StreamInfoStringRefFieldExtractor(FieldExtractor f) : field_extractor_(f) {}

  // StreamInfoFormatter::FieldExtractor
  absl::optional<std::string>
  extract(const StreamInfo::StreamInfo& stream_info) const override {
    return field_extractor_(stream_info);
  }
  const std::string& extractRef(const StreamInfo::StreamInfo& stream_info) const override {
    return field_extractor_(stream_info);
  }
  ProtobufWkt::Value extractValue(const StreamInfo::StreamInfo& stream_info) const override {
    return ValueUtil::optionalStringValue(field_extractor_(stream_info));
  }

private:
  FieldExtractor field_extractor_;
};

// StreamInfo std::chrono_nanoseconds field extractor.
class StreamInfoDurationFieldExtractor : public StreamInfoFormatter::FieldExtractor {
public:
  using FieldExtractor =
      std::function<absl::optional<std::chrono::nanoseconds>(const StreamInfo::StreamInfo&)>;

  StreamInfoDurationFieldExtractor(FieldExtractor f) : field_extractor_(f) {}

  // StreamInfoFormatter::FieldExtractor
  absl::optional<std::string> extract(const StreamInfo::StreamInfo& stream_info) const override {
    const auto millis = extractMillis(stream_info);
    if (!millis) {
      return absl::nullopt;
    }

    return fmt::format_int(millis.value()).str();
  }
  ProtobufWkt::Value extractValue(const StreamInfo::StreamInfo& stream_info) const override {
    const auto millis = extractMillis(stream_info);
    if (!millis) {
      return unspecifiedValue();
    }

    return ValueUtil::numberValue(millis.value());
  }

private:
  absl::optional<int64_t> extractMillis(const StreamInfo::StreamInfo& stream_info) const {
    const auto time = field_extractor_(stream_info);
    if (time) {
      return std::chrono::duration_cast<std::chrono::milliseconds>(time.value()).count();
    }
    return absl::nullopt;
  }

  FieldExtractor field_extractor_;
};

// StreamInfo uint64_t field extractor.
class StreamInfoUInt64FieldExtractor : public StreamInfoFormatter::FieldExtractor {
public:
  using FieldExtractor = std::function<uint64_t(const StreamInfo::StreamInfo&)>;

  StreamInfoUInt64FieldExtractor(FieldExtractor f) : field_extractor_(f) {}

  // StreamInfoFormatter::FieldExtractor
  absl::optional<std::string> extract(const StreamInfo::StreamInfo& stream_info) const override {
    return fmt::format_int(field_extractor_(stream_info)).str();
  }
  ProtobufWkt::Value extractValue(const StreamInfo::StreamInfo& stream_info) const override {
    return ValueUtil::numberValue(field_extractor_(stream_info));
  }

private:
  FieldExtractor field_extractor_;
};

// StreamInfo Envoy::Network::Address::InstanceConstSharedPtr field extractor.
class StreamInfoAddressFieldExtractor : public StreamInfoFormatter::FieldExtractor {
public:
  using FieldExtractor =
      std::function<Network::Address::InstanceConstSharedPtr(const StreamInfo::StreamInfo&)>;

  static std::unique_ptr<StreamInfoAddressFieldExtractor> withPort(FieldExtractor f) {
    return std::make_unique<StreamInfoAddressFieldExtractor>(
        f, StreamInfoFormatter::StreamInfoAddressFieldExtractionType::WithPort);
  }

  static std::unique_ptr<StreamInfoAddressFieldExtractor> withoutPort(FieldExtractor f) {
    return std::make_unique<StreamInfoAddressFieldExtractor>(
        f, StreamInfoFormatter::StreamInfoAddressFieldExtractionType::WithoutPort);
  }

  static std::unique_ptr<StreamInfoAddressFieldExtractor> justPort(FieldExtractor f) {
    return std::make_unique<StreamInfoAddressFieldExtractor>(
        f, StreamInfoFormatter::StreamInfoAddressFieldExtractionType::JustPort);
  }

  StreamInfoAddressFieldExtractor(
      FieldExtractor f, StreamInfoFormatter::StreamInfoAddressFieldExtractionType extraction_type)
      : field_extractor_(f), extraction_type_(extraction_type) {}

  // StreamInfoFormatter::FieldExtractor
  absl::optional<std::string> extract(const StreamInfo::StreamInfo& stream_info) const override {
    Network::Address::InstanceConstSharedPtr address = field_extractor_(stream_info);
    if (!address) {
      return absl::nullopt;
    }

    return toString(*address);
  }
  ProtobufWkt::Value extractValue(const StreamInfo::StreamInfo& stream_info) const override {
    Network::Address::InstanceConstSharedPtr address = field_extractor_(stream_info);
    if (!address) {
      return unspecifiedValue();
    }

    return ValueUtil::stringValue(toString(*address));
  }

private:
  std::string toString(const Network::Address::Instance& address) const {
    switch (extraction_type_) {
    case StreamInfoFormatter::StreamInfoAddressFieldExtractionType::WithoutPort:
      return StreamInfo::Utility::formatDownstreamAddressNoPort(address);
    case StreamInfoFormatter::StreamInfoAddressFieldExtractionType::JustPort:
      return StreamInfo::Utility::formatDownstreamAddressJustPort(address);
    case StreamInfoFormatter::StreamInfoAddressFieldExtractionType::WithPort:
    default:
      return address.asString();
    }
  }

  FieldExtractor field_extractor_;
  const StreamInfoFormatter::StreamInfoAddressFieldExtractionType extraction_type_;
};

// Ssl::ConnectionInfo std::string field extractor.
class StreamInfoSslConnectionInfoFieldExtractor : public StreamInfoFormatter::FieldExtractor {
public:
  using FieldExtractor =
      std::function<absl::optional<std::string>(const Ssl::ConnectionInfo& connection_info)>;

  StreamInfoSslConnectionInfoFieldExtractor(FieldExtractor f) : field_extractor_(f) {}

  absl::optional<std::string> extract(const StreamInfo::StreamInfo& stream_info) const override {
    if (stream_info.downstreamAddressProvider().sslConnection() == nullptr) {
      return absl::nullopt;
    }

    const auto value = field_extractor_(*stream_info.downstreamAddressProvider().sslConnection());
    if (value && value->empty()) {
      return absl::nullopt;
    }

    return value;
  }

  ProtobufWkt::Value extractValue(const StreamInfo::StreamInfo& stream_info) const override {
    if (stream_info.downstreamAddressProvider().sslConnection() == nullptr) {
      return unspecifiedValue();
    }

    const auto value = field_extractor_(*stream_info.downstreamAddressProvider().sslConnection());
    if (value && value->empty()) {
      return unspecifiedValue();
    }

    return ValueUtil::optionalStringValue(value);
  }

private:
  FieldExtractor field_extractor_;
};

const StreamInfoFormatter::FieldExtractorLookupTbl& StreamInfoFormatter::getKnownFieldExtractors() {
  CONSTRUCT_ON_FIRST_USE(
      FieldExtractorLookupTbl,
      {{"REQUEST_DURATION",
        []() {
          return std::make_unique<StreamInfoDurationFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                StreamInfo::TimingUtility timing(stream_info);
                return timing.lastDownstreamRxByteReceived();
              });
        }},
       {"REQUEST_TX_DURATION",
        []() {
          return std::make_unique<StreamInfoDurationFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                StreamInfo::TimingUtility timing(stream_info);
                return timing.lastUpstreamTxByteSent();
              });
        }},
       {"RESPONSE_DURATION",
        []() {
          return std::make_unique<StreamInfoDurationFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                StreamInfo::TimingUtility timing(stream_info);
                return timing.firstUpstreamRxByteReceived();
              });
        }},
       {"RESPONSE_TX_DURATION",
        []() {
          return std::make_unique<StreamInfoDurationFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                StreamInfo::TimingUtility timing(stream_info);
                auto downstream = timing.lastDownstreamTxByteSent();
                auto upstream = timing.firstUpstreamRxByteReceived();

                absl::optional<std::chrono::nanoseconds> result;
                if (downstream && upstream) {
                  result = downstream.value() - upstream.value();
                }

                return result;
              });
        }},
       {"BYTES_RECEIVED",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.bytesReceived();
              });
        }},
       {"UPSTREAM_WIRE_BYTES_RECEIVED",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.getUpstreamBytesMeter()->wireBytesReceived();
              });
        }},
       {"UPSTREAM_HEADER_BYTES_RECEIVED",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.getUpstreamBytesMeter()->headerBytesReceived();
              });
        }},
       {"DOWNSTREAM_WIRE_BYTES_RECEIVED",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.getDownstreamBytesMeter()->wireBytesReceived();
              });
        }},
       {"DOWNSTREAM_HEADER_BYTES_RECEIVED",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.getDownstreamBytesMeter()->headerBytesReceived();
              });
        }},
       {"PROTOCOL",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return SubstitutionFormatUtils::protocolToString(stream_info.protocol());
              });
        }},
       {"RESPONSE_CODE",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.responseCode().value_or(0);
              });
        }},
       {"RESPONSE_CODE_DETAILS",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.responseCodeDetails();
              });
        }},
       {"CONNECTION_TERMINATION_DETAILS",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.connectionTerminationDetails();
              });
        }},
       {"BYTES_SENT",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) { return stream_info.bytesSent(); });
        }},
       {"UPSTREAM_WIRE_BYTES_SENT",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.getUpstreamBytesMeter()->wireBytesSent();
              });
        }},
       {"UPSTREAM_HEADER_BYTES_SENT",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.getUpstreamBytesMeter()->headerBytesSent();
              });
        }},
       {"DOWNSTREAM_WIRE_BYTES_SENT",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.getDownstreamBytesMeter()->wireBytesSent();
              });
        }},
       {"DOWNSTREAM_HEADER_BYTES_SENT",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.getDownstreamBytesMeter()->headerBytesSent();
              });
        }},
       {"DURATION",
        []() {
          return std::make_unique<StreamInfoDurationFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.requestComplete();
              });
        }},
       {"RESPONSE_FLAGS",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return StreamInfo::ResponseFlagUtils::toShortString(stream_info);
              });
        }},
       {"UPSTREAM_HOST",
        []() {
          return StreamInfoAddressFieldExtractor::withPort(
              [](const StreamInfo::StreamInfo& stream_info)
                  -> std::shared_ptr<const Envoy::Network::Address::Instance> {
                if (stream_info.upstreamInfo() && stream_info.upstreamInfo()->upstreamHost()) {
                  return stream_info.upstreamInfo()->upstreamHost()->address();
                } else if (stream_info.predictUpstreamRemoteAddress()) {
                  return stream_info.predictUpstreamRemoteAddress();
                }
                return nullptr;
              });
        }},
       {"ORIGIN_HOST",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.originHost();
              });
        }},
       {"VHOST_MATCHED",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return std::to_string(stream_info.isVHostMatched());
              });
        }},
       {"UPSTREAM_CLUSTER",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                std::string upstream_cluster_name;
                if (stream_info.upstreamClusterInfo().has_value() &&
                    stream_info.upstreamClusterInfo().value() != nullptr) {
                  if (Runtime::runtimeFeatureEnabled(
                          "envoy.reloadable_features.use_observable_cluster_name")) {
                    upstream_cluster_name =
                        stream_info.upstreamClusterInfo().value()->observabilityName();
                  } else {
                    upstream_cluster_name = stream_info.upstreamClusterInfo().value()->name();
                  }
                }

                return upstream_cluster_name.empty()
                           ? absl::nullopt
                           : absl::make_optional<std::string>(upstream_cluster_name);
              });
        }},
       {"UPSTREAM_LOCAL_ADDRESS",
        []() {
          return StreamInfoAddressFieldExtractor::withPort(
              [](const StreamInfo::StreamInfo& stream_info)
                  -> std::shared_ptr<const Envoy::Network::Address::Instance> {
                if (stream_info.upstreamInfo().has_value()) {
                  return stream_info.upstreamInfo().value().get().upstreamLocalAddress();
                }
                return nullptr;
              });
        }},
       {"UPSTREAM_REQUEST_ATTEMPT_COUNT",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.attemptCount().value_or(0);
              });
        }},
       {"DOWNSTREAM_LOCAL_ADDRESS",
        []() {
          return StreamInfoAddressFieldExtractor::withPort(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.downstreamAddressProvider().localAddress();
              });
        }},
       {"DOWNSTREAM_LOCAL_ADDRESS_WITHOUT_PORT",
        []() {
          return StreamInfoAddressFieldExtractor::withoutPort(
              [](const Envoy::StreamInfo::StreamInfo& stream_info) {
                return stream_info.downstreamAddressProvider().localAddress();
              });
        }},
       {"DOWNSTREAM_LOCAL_PORT",
        []() {
          return StreamInfoAddressFieldExtractor::justPort(
              [](const Envoy::StreamInfo::StreamInfo& stream_info) {
                return stream_info.downstreamAddressProvider().localAddress();
              });
        }},
       {"DOWNSTREAM_REMOTE_ADDRESS",
        []() {
          return StreamInfoAddressFieldExtractor::withPort(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.downstreamAddressProvider().remoteAddress();
              });
        }},
       {"DOWNSTREAM_REMOTE_ADDRESS_WITHOUT_PORT",
        []() {
          return StreamInfoAddressFieldExtractor::withoutPort(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.downstreamAddressProvider().remoteAddress();
              });
        }},
       {"DOWNSTREAM_DIRECT_REMOTE_ADDRESS",
        []() {
          return StreamInfoAddressFieldExtractor::withPort(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.downstreamAddressProvider().directRemoteAddress();
              });
        }},
       {"DOWNSTREAM_DIRECT_REMOTE_ADDRESS_WITHOUT_PORT",
        []() {
          return StreamInfoAddressFieldExtractor::withoutPort(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.downstreamAddressProvider().directRemoteAddress();
              });
        }},
       {"CONNECTION_ID",
        []() {
          return std::make_unique<StreamInfoUInt64FieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return stream_info.downstreamAddressProvider().connectionID().value_or(0);
              });
        }},
       {"REQUESTED_SERVER_NAME",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                absl::optional<std::string> result;
                if (!stream_info.downstreamAddressProvider().requestedServerName().empty()) {
                  result =
                      std::string(stream_info.downstreamAddressProvider().requestedServerName());
                }
                return result;
              });
        }},
       {"ROUTE_NAME",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                absl::optional<std::string> result;
                std::string route_name = stream_info.getRouteName();
                if (!route_name.empty()) {
                  result = route_name;
                }
                return result;
              });
        }},
       {"CLIENT_DEVICE_FINGERPRINT",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                return StreamInfoFormatter::getUniqueId(stream_info);
              });
        }},
       {"DOWNSTREAM_PEER_URI_SAN",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return absl::StrJoin(connection_info.uriSanPeerCertificate(), ",");
              });
        }},
       {"DOWNSTREAM_LOCAL_URI_SAN",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return absl::StrJoin(connection_info.uriSanLocalCertificate(), ",");
              });
        }},
       {"DOWNSTREAM_PEER_SUBJECT",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.subjectPeerCertificate();
              });
        }},
       {"DOWNSTREAM_LOCAL_SUBJECT",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.subjectLocalCertificate();
              });
        }},
       {"DOWNSTREAM_TLS_SESSION_ID",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.sessionId();
              });
        }},
       {"DOWNSTREAM_TLS_CIPHER",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.ciphersuiteString();
              });
        }},
       {"DOWNSTREAM_TLS_VERSION",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.tlsVersion();
              });
        }},
       {"DOWNSTREAM_PEER_FINGERPRINT_256",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.sha256PeerCertificateDigest();
              });
        }},
       {"DOWNSTREAM_PEER_FINGERPRINT_1",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.sha1PeerCertificateDigest();
              });
        }},
       {"DOWNSTREAM_PEER_SERIAL",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.serialNumberPeerCertificate();
              });
        }},
       {"DOWNSTREAM_PEER_ISSUER",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.issuerPeerCertificate();
              });
        }},
       {"DOWNSTREAM_PEER_CERT",
        []() {
          return std::make_unique<StreamInfoSslConnectionInfoFieldExtractor>(
              [](const Ssl::ConnectionInfo& connection_info) {
                return connection_info.urlEncodedPemEncodedPeerCertificate();
              });
        }},
       {"UPSTREAM_TRANSPORT_FAILURE_REASON",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                absl::optional<std::string> result;
                if (stream_info.upstreamInfo().has_value() && !stream_info.upstreamInfo()
                                                                   .value()
                                                                   .get()
                                                                   .upstreamTransportFailureReason()
                                                                   .empty()) {
                  result =
                      stream_info.upstreamInfo().value().get().upstreamTransportFailureReason();
                }
                return result;
              });
        }},
       {"HOSTNAME",
        []() {
          absl::optional<std::string> hostname = SubstitutionFormatUtils::getHostname();
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [hostname](const StreamInfo::StreamInfo&) { return hostname; });
        }},
       {"FILTER_CHAIN_NAME",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> absl::optional<std::string> {
                if (!stream_info.filterChainName().empty()) {
                  return stream_info.filterChainName();
                }
                return absl::nullopt;
              });
        }},
       {"VIRTUAL_CLUSTER_NAME",
        []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> absl::optional<std::string> {
                return stream_info.virtualClusterName();
              });
        }},
       {"TLS_JA3_FINGERPRINT", []() {
          return std::make_unique<StreamInfoStringFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) {
                absl::optional<std::string> result;
                if (!stream_info.downstreamAddressProvider().ja3Hash().empty()) {
                  result = std::string(stream_info.downstreamAddressProvider().ja3Hash());
                }
                return result;
              });
        }},
        {"REQ_HEADERS_PRE_PLUGIN_BYTES",
          []() {
            return std::make_unique<StreamInfoStringRefFieldExtractor>(
                [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                  const static std::string key = "req_headers_pre_plugin_bytes";
                  return getMetadataField(stream_info, key, false);
                });
        }},
        {"REQ_HEADERS_PRE_PLUGIN",
          []() {
            return std::make_unique<StreamInfoStringRefFieldExtractor>(
                [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                  const static std::string key = "req_headers_pre_plugin";
                  return getMetadataField(stream_info, key, true);
                });
        }},
       {"REQUEST_BODY_BYTES",
        []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                const static std::string key = "request_body_bytes";
                return getMetadataField(stream_info, key, false);
              });
        }},
       {"RESPONSE_BODY_BYTES",
        []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                const static std::string key = "response_body_bytes";
                return getMetadataField(stream_info, key, false);
              });
        }},
       {"REQUEST_BODY",
        []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                const static std::string key = "request_body";
                return getMetadataField(stream_info, key, true);
              });
        }},
       {"RESPONSE_BODY", []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                const static std::string key = "response_body";
                return getMetadataField(stream_info, key, true);
              });
        }},
       {"UNIQUE_ID", []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                return getSharedData(stream_info, UserIdentifySharedDataUserUniqueId);
              });
        }},
       {"USER_NAME", []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                return getSharedData(stream_info, UserIdentifySharedDataUserName);
              });
        }},
       {"APP_ID", []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                return getSharedData(stream_info, UserIdentifySharedDataAppId);
              });
        }},
       {"RULE_ID", []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                return getSharedData(stream_info, UserIdentifySharedDataRuleId);
              });
        }},
       {"FILTER_GATEWAY_NAME", []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                return getSharedData(stream_info, LogSharedDataGatewayName);
              });
        }},
       {"FILTER_VIRTUAL_HOST_NAME", []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                return getSharedData(stream_info, LogSharedDataVirtualHostName);
              });
        }},
       {"FILTER_CLUSTER_NAME", []() {
          return std::make_unique<StreamInfoStringRefFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> const std::string& {
                return getSharedData(stream_info, LogSharedDataClusterName);
              });
        }}
      });
}

const std::string& StreamInfoFormatter::getSharedData(const StreamInfo::StreamInfo& stream_info,
                                                         const std::string& key) {
  static const absl::string_view data_name("shared_data");
  if (stream_info.filterState().hasData<Envoy::StreamInfo::UserData>(data_name)) {
    const auto& shared_data = stream_info.filterState().getDataReadOnly<Envoy::StreamInfo::UserData>(data_name);
    const auto it = shared_data.find(key);
    if (it != shared_data.end()) {
      return it->second;
    }
  }
  return EMPTY_STRING;
}

bool StreamInfoFormatter::isPluginHit(const StreamInfo::StreamInfo& stream_info) {
    return (getSharedData(stream_info, CommonSharedDataPluginHit) != EMPTY_STRING);
}

const std::string& StreamInfoFormatter::getUniqueId(const StreamInfo::StreamInfo& stream_info){
  return getSharedData(stream_info, UserIdentifySharedDataUserUniqueId);
}

const std::string& StreamInfoFormatter::getMetadataField(const StreamInfo::StreamInfo& stream_info,
                                                         const std::string& key, bool number) {
  // 该命名空间下的数据由body-to-metadata插件写入
  const static std::string namespace_string = "envoy.filters.http.body-to-metadata.1.0";

  const envoy::config::core::v3::Metadata& metadata = stream_info.dynamicMetadata();
  const auto namespace_iter = metadata.filter_metadata().find(namespace_string);
  if (namespace_iter != metadata.filter_metadata().end()) {
    const auto field_iter = namespace_iter->second.fields().find(key);
    if (field_iter != namespace_iter->second.fields().end()) {
      if (number) {
        // 为避免大量数据以值拷贝方式拷贝进metadata，这里存放的数字实际是std::string的指针
        uint64_t body_pointer = field_iter->second.number_value();
        if (body_pointer) {
          return *(reinterpret_cast<std::string*>(body_pointer));
        }
      } else {
        return field_iter->second.string_value();
      }
    }
  }
  return EMPTY_STRING;
}

StreamInfoFormatter::StreamInfoFormatter(const std::string& field_name) {
  const FieldExtractorLookupTbl& extractors = getKnownFieldExtractors();

  auto it = extractors.find(field_name);

  if (it == extractors.end()) {
    throw EnvoyException(fmt::format("Not supported field in StreamInfo: {}", field_name));
  }

  // Create a pointer to the formatter by calling a function
  // associated with formatter's name.
  field_extractor_ = (*it).second();

  if (field_name == "REQUEST_BODY_BYTES" || field_name == "RESPONSE_BODY_BYTES" ||
      field_name == "REQUEST_BODY" || field_name == "RESPONSE_BODY") {
    support_format_ref_ = true;
  }
}

absl::optional<std::string> StreamInfoFormatter::format(const Http::RequestHeaderMap&,
                                                        const Http::ResponseHeaderMap&,
                                                        const Http::ResponseTrailerMap&,
                                                        const StreamInfo::StreamInfo& stream_info,
                                                        absl::string_view) const {
  return field_extractor_->extract(stream_info);
}

const std::string& StreamInfoFormatter::formatRef(const Http::RequestHeaderMap&,
                                                  const Http::ResponseHeaderMap&,
                                                  const Http::ResponseTrailerMap&,
                                                  const StreamInfo::StreamInfo& stream_info,
                                                  absl::string_view) const {
  return field_extractor_->extractRef(stream_info);
}

ProtobufWkt::Value StreamInfoFormatter::formatValue(const Http::RequestHeaderMap&,
                                                    const Http::ResponseHeaderMap&,
                                                    const Http::ResponseTrailerMap&,
                                                    const StreamInfo::StreamInfo& stream_info,
                                                    absl::string_view) const {
  return field_extractor_->extractValue(stream_info);
}

PlainStringFormatter::PlainStringFormatter(const std::string& str) { str_.set_string_value(str); }

absl::optional<std::string> PlainStringFormatter::format(const Http::RequestHeaderMap&,
                                                         const Http::ResponseHeaderMap&,
                                                         const Http::ResponseTrailerMap&,
                                                         const StreamInfo::StreamInfo&,
                                                         absl::string_view) const {
  return str_.string_value();
}

ProtobufWkt::Value PlainStringFormatter::formatValue(const Http::RequestHeaderMap&,
                                                     const Http::ResponseHeaderMap&,
                                                     const Http::ResponseTrailerMap&,
                                                     const StreamInfo::StreamInfo&,
                                                     absl::string_view) const {
  return str_;
}

absl::optional<std::string>
LocalReplyBodyFormatter::format(const Http::RequestHeaderMap&, const Http::ResponseHeaderMap&,
                                const Http::ResponseTrailerMap&, const StreamInfo::StreamInfo&,
                                absl::string_view local_reply_body) const {
  return std::string(local_reply_body);
}

ProtobufWkt::Value LocalReplyBodyFormatter::formatValue(const Http::RequestHeaderMap&,
                                                        const Http::ResponseHeaderMap&,
                                                        const Http::ResponseTrailerMap&,
                                                        const StreamInfo::StreamInfo&,
                                                        absl::string_view local_reply_body) const {
  return ValueUtil::stringValue(std::string(local_reply_body));
}

HeaderFormatter::HeaderFormatter(const std::string& main_header,
                                 const std::string& alternative_header,
                                 absl::optional<size_t> max_length)
    : main_header_(main_header), alternative_header_(alternative_header), max_length_(max_length) {}

const Http::HeaderEntry* HeaderFormatter::findHeader(const Http::HeaderMap& headers) const {
  const auto header = headers.get(main_header_);

  if (header.empty() && !alternative_header_.get().empty()) {
    const auto alternate_header = headers.get(alternative_header_);
    // TODO(https://github.com/envoyproxy/envoy/issues/13454): Potentially log all header values.
    return alternate_header.empty() ? nullptr : alternate_header[0];
  }

  return header.empty() ? nullptr : header[0];
}

absl::optional<std::string> HeaderFormatter::format(const Http::HeaderMap& headers) const {
  const Http::HeaderEntry* header = findHeader(headers);
  if (!header) {
    return absl::nullopt;
  }

  std::string val = std::string(header->value().getStringView());
  truncate(val, max_length_);
  return val;
}

ProtobufWkt::Value HeaderFormatter::formatValue(const Http::HeaderMap& headers) const {
  const Http::HeaderEntry* header = findHeader(headers);
  if (!header) {
    return unspecifiedValue();
  }

  std::string val = std::string(header->value().getStringView());
  truncate(val, max_length_);
  return ValueUtil::stringValue(val);
}

ResponseHeaderFormatter::ResponseHeaderFormatter(const std::string& main_header,
                                                 const std::string& alternative_header,
                                                 absl::optional<size_t> max_length)
    : HeaderFormatter(main_header, alternative_header, max_length) {}

absl::optional<std::string> ResponseHeaderFormatter::format(
    const Http::RequestHeaderMap&, const Http::ResponseHeaderMap& response_headers,
    const Http::ResponseTrailerMap&, const StreamInfo::StreamInfo&, absl::string_view) const {
  return HeaderFormatter::format(response_headers);
}

ProtobufWkt::Value ResponseHeaderFormatter::formatValue(
    const Http::RequestHeaderMap&, const Http::ResponseHeaderMap& response_headers,
    const Http::ResponseTrailerMap&, const StreamInfo::StreamInfo&, absl::string_view) const {
  return HeaderFormatter::formatValue(response_headers);
}

RequestHeaderFormatter::RequestHeaderFormatter(const std::string& main_header,
                                               const std::string& alternative_header,
                                               absl::optional<size_t> max_length)
    : HeaderFormatter(main_header, alternative_header, max_length) {}

absl::optional<std::string>
RequestHeaderFormatter::format(const Http::RequestHeaderMap& request_headers,
                               const Http::ResponseHeaderMap&, const Http::ResponseTrailerMap&,
                               const StreamInfo::StreamInfo&, absl::string_view) const {
  return HeaderFormatter::format(request_headers);
}

ProtobufWkt::Value
RequestHeaderFormatter::formatValue(const Http::RequestHeaderMap& request_headers,
                                    const Http::ResponseHeaderMap&, const Http::ResponseTrailerMap&,
                                    const StreamInfo::StreamInfo&, absl::string_view) const {
  return HeaderFormatter::formatValue(request_headers);
}

absl::optional<std::string> formatRequestLine(const Http::RequestHeaderMap& request_headers,
                               const StreamInfo::StreamInfo& stream_info, bool is_proto = false) {
  auto method = request_headers.getMethodValue();
  auto path = request_headers.getEnvoyOriginalPathValue();
  if (path.empty()) {
    path = request_headers.getPathValue();
  }
  auto protocol = SubstitutionFormatUtils::protocolToString(stream_info.protocol());

  std::string request_line(method);
  request_line.append(" ");
  request_line.append(path.data(), path.size());
  request_line.append(" ");
  request_line.append(protocol.has_value() ? protocol.value() : std::string("HTTP1.1"));
  if (is_proto) {
    return request_line;
  } else {
    return std::to_string(request_line.size()) + ":" + request_line;
  }
}

// request line format
RequestLineFormatter::RequestLineFormatter() {}

absl::optional<std::string>
RequestLineFormatter::format(const Http::RequestHeaderMap& request_headers,
                               const Http::ResponseHeaderMap&, const Http::ResponseTrailerMap&,
                               const StreamInfo::StreamInfo& stream_info, absl::string_view) const {
  return formatRequestLine(request_headers, stream_info);
}

ProtobufWkt::Value RequestLineFormatter::formatValue(const Http::RequestHeaderMap& request_headers,
                                                     const Http::ResponseHeaderMap&,
                                                     const Http::ResponseTrailerMap&,
                                                     const StreamInfo::StreamInfo& stream_info,
                                                     absl::string_view) const {
  absl::optional<std::string> str = formatRequestLine(request_headers, stream_info, true);
  if (str.has_value()) {
    return ValueUtil::stringValue(str.value());
  } else {
    return unspecifiedValue();
  }
}

absl::optional<std::string> formatResponseLine(const StreamInfo::StreamInfo& stream_info) {
  auto protocol = SubstitutionFormatUtils::protocolToString(stream_info.protocol());
  std::string response_line(protocol.has_value() ? protocol.value() : std::string("HTTP1.1"));

  absl::optional<uint32_t> code = stream_info.responseCode();
  uint32_t code_value = 0;
  if (code) {
    code_value = code.value();
  }
  response_line.append(" ");
  response_line.append(std::to_string(code_value));
  response_line.append(" ");
  response_line.append(Envoy::Http::CodeUtility::toString(static_cast<Envoy::Http::Code>(code_value)));
  return response_line;
}

// response line format
ResponseLineFormatter::ResponseLineFormatter() {}

absl::optional<std::string>
ResponseLineFormatter::format(const Http::RequestHeaderMap&,
                               const Http::ResponseHeaderMap&, const Http::ResponseTrailerMap&,
                               const StreamInfo::StreamInfo& stream_info, absl::string_view) const {
  return formatResponseLine(stream_info);
}

ProtobufWkt::Value ResponseLineFormatter::formatValue(const Http::RequestHeaderMap&,
                                                     const Http::ResponseHeaderMap&,
                                                     const Http::ResponseTrailerMap&,
                                                     const StreamInfo::StreamInfo& stream_info,
                                                     absl::string_view) const {
  absl::optional<std::string> str = formatResponseLine(stream_info);
  if (str.has_value()) {
    return ValueUtil::stringValue(str.value());
  } else {
    return unspecifiedValue();
  }
}

// 引擎SN号.SE-W SN号
absl::optional<std::string> formatNodeId() {
  static absl::optional<std::string> node_id = []() -> absl::optional<std::string> {
    std::string id;
    auto gs = getenv("GS_EE");
    if (gs) {
      id.append(gs);
    }
    id.append(".");

    gs = getenv("GS_WW");
    if (gs) {
      id.append(gs);
    }
    return absl::optional<std::string>(id);
  }();
  return node_id;
}

// node id format
NodeIdFormatter::NodeIdFormatter() {}

absl::optional<std::string>
NodeIdFormatter::format(const Http::RequestHeaderMap&,
                               const Http::ResponseHeaderMap&, const Http::ResponseTrailerMap&,
                               const StreamInfo::StreamInfo&, absl::string_view) const {
  return formatNodeId();
}

ProtobufWkt::Value NodeIdFormatter::formatValue(const Http::RequestHeaderMap&,
                                                     const Http::ResponseHeaderMap&,
                                                     const Http::ResponseTrailerMap&,
                                                     const StreamInfo::StreamInfo&,
                                                     absl::string_view) const {
  absl::optional<std::string> str = formatNodeId();
  if (str.has_value()) {
    return ValueUtil::stringValue(str.value());
  } else {
    return unspecifiedValue();
  }
}

absl::optional<std::string> formatListenerStatPrefix(const Http::ResponseHeaderMap&,
                               const StreamInfo::StreamInfo& stream_info) {
  return stream_info.listenerStatPrefix();
}

// listener stat_prefix format
ListenerStatPrefixFormatter::ListenerStatPrefixFormatter() {}

absl::optional<std::string>
ListenerStatPrefixFormatter::format(const Http::RequestHeaderMap&,
                               const Http::ResponseHeaderMap&, const Http::ResponseTrailerMap&,
                               const StreamInfo::StreamInfo& stream_info, absl::string_view) const {
  return stream_info.listenerStatPrefix();
}

ProtobufWkt::Value ListenerStatPrefixFormatter::formatValue(const Http::RequestHeaderMap&,
                                                     const Http::ResponseHeaderMap&,
                                                     const Http::ResponseTrailerMap&,
                                                     const StreamInfo::StreamInfo& stream_info,
                                                     absl::string_view) const {
  const std::string& stat_prefix = stream_info.listenerStatPrefix();
  if (!stat_prefix.empty()) {
    return ValueUtil::stringValue(stat_prefix);
  } else {
    return unspecifiedValue();
  }
}

/**
 * format_data = true: 返回拼接生成的headers
 * format_data = false: 返回拼接生成的headers长度大小
 */
absl::optional<std::string> formatHeaders(const Http::HeaderMap& headers_map, bool format_data) {
  static std::set<std::string_view> IgnoreHeaderSet = {":path", ":method"};
  if (format_data) {
    std::string headers;
    headers_map.iterate([&](const Http::HeaderEntry& entry) {
      auto key = entry.key().getStringView();
      if (IgnoreHeaderSet.find(std::string_view(key.data(), key.size())) != IgnoreHeaderSet.end()) {
        return Http::HeaderMap::Iterate::Continue;
      }
      auto value = entry.value().getStringView();
      headers.append(key.data(), key.size());
      headers.append(": ");
      headers.append(value.data(), value.size());
      headers.append("\r\n");
      return Http::HeaderMap::Iterate::Continue;
    });
    if (headers.empty()) {
      return absl::nullopt;
    }
    return headers;
  } else {
    // uint32_t headers_size = headers_map.byteSize() + headers_map.size() * 4;
    uint32_t headers_size = 0;
    headers_map.iterate([&](const Http::HeaderEntry& entry) {
      auto key = entry.key().getStringView();
      ASSERT(key.size() == entry.key().size());
      if (IgnoreHeaderSet.find(std::string_view(key.data(), key.size())) == IgnoreHeaderSet.end()) {
        headers_size += entry.key().size() + entry.value().size() + 4;
      }
      return Http::HeaderMap::Iterate::Continue;
    });
    if (headers_size == 0) {
      return absl::nullopt;
    }
    return std::to_string(headers_size);
  }
}

AllResponseHeaderFormatter::AllResponseHeaderFormatter(bool format_data) : format_data_(format_data) {}

absl::optional<std::string> AllResponseHeaderFormatter::format(
    const Http::RequestHeaderMap&, const Http::ResponseHeaderMap& response_headers,
    const Http::ResponseTrailerMap&, const StreamInfo::StreamInfo&, absl::string_view) const {
  return formatHeaders(response_headers, format_data_);
}

ProtobufWkt::Value AllResponseHeaderFormatter::formatValue(
    const Http::RequestHeaderMap&, const Http::ResponseHeaderMap& response_headers,
    const Http::ResponseTrailerMap&, const StreamInfo::StreamInfo&, absl::string_view) const {
  absl::optional<std::string> str = formatHeaders(response_headers, format_data_);
  if (str.has_value()) {
    return ValueUtil::stringValue(str.value());
  } else {
    return unspecifiedValue();
  }
}

AllRequestHeaderFormatter::AllRequestHeaderFormatter(bool format_data) : format_data_(format_data) {}

absl::optional<std::string>
AllRequestHeaderFormatter::format(const Http::RequestHeaderMap& request_headers,
                                  const Http::ResponseHeaderMap&, const Http::ResponseTrailerMap&,
                                  const StreamInfo::StreamInfo&, absl::string_view) const {
  return formatHeaders(request_headers, format_data_);
}

ProtobufWkt::Value AllRequestHeaderFormatter::formatValue(const Http::RequestHeaderMap& request_headers,
                                                          const Http::ResponseHeaderMap&,
                                                          const Http::ResponseTrailerMap&,
                                                          const StreamInfo::StreamInfo&,
                                                          absl::string_view) const {
  absl::optional<std::string> str = formatHeaders(request_headers, format_data_);
  if (str.has_value()) {
    return ValueUtil::stringValue(str.value());
  } else {
    return unspecifiedValue();
  }
}

ResponseTrailerFormatter::ResponseTrailerFormatter(const std::string& main_header,
                                                   const std::string& alternative_header,
                                                   absl::optional<size_t> max_length)
    : HeaderFormatter(main_header, alternative_header, max_length) {}

absl::optional<std::string>
ResponseTrailerFormatter::format(const Http::RequestHeaderMap&, const Http::ResponseHeaderMap&,
                                 const Http::ResponseTrailerMap& response_trailers,
                                 const StreamInfo::StreamInfo&, absl::string_view) const {
  return HeaderFormatter::format(response_trailers);
}

ProtobufWkt::Value
ResponseTrailerFormatter::formatValue(const Http::RequestHeaderMap&, const Http::ResponseHeaderMap&,
                                      const Http::ResponseTrailerMap& response_trailers,
                                      const StreamInfo::StreamInfo&, absl::string_view) const {
  return HeaderFormatter::formatValue(response_trailers);
}

HeadersByteSizeFormatter::HeadersByteSizeFormatter(const HeaderType header_type)
    : header_type_(header_type) {}

uint64_t HeadersByteSizeFormatter::extractHeadersByteSize(
    const Http::RequestHeaderMap& request_headers, const Http::ResponseHeaderMap& response_headers,
    const Http::ResponseTrailerMap& response_trailers) const {
  switch (header_type_) {
  case HeaderType::RequestHeaders:
    return request_headers.byteSize();
  case HeaderType::ResponseHeaders:
    return response_headers.byteSize();
  case HeaderType::ResponseTrailers:
    return response_trailers.byteSize();
  }
  PANIC_DUE_TO_CORRUPT_ENUM;
}

absl::optional<std::string>
HeadersByteSizeFormatter::format(const Http::RequestHeaderMap& request_headers,
                                 const Http::ResponseHeaderMap& response_headers,
                                 const Http::ResponseTrailerMap& response_trailers,
                                 const StreamInfo::StreamInfo&, absl::string_view) const {
  return absl::StrCat(extractHeadersByteSize(request_headers, response_headers, response_trailers));
}

ProtobufWkt::Value
HeadersByteSizeFormatter::formatValue(const Http::RequestHeaderMap& request_headers,
                                      const Http::ResponseHeaderMap& response_headers,
                                      const Http::ResponseTrailerMap& response_trailers,
                                      const StreamInfo::StreamInfo&, absl::string_view) const {
  return ValueUtil::numberValue(
      extractHeadersByteSize(request_headers, response_headers, response_trailers));
}

GrpcStatusFormatter::GrpcStatusFormatter(const std::string& main_header,
                                         const std::string& alternative_header,
                                         absl::optional<size_t> max_length)
    : HeaderFormatter(main_header, alternative_header, max_length) {}

absl::optional<std::string>
GrpcStatusFormatter::format(const Http::RequestHeaderMap&,
                            const Http::ResponseHeaderMap& response_headers,
                            const Http::ResponseTrailerMap& response_trailers,
                            const StreamInfo::StreamInfo& info, absl::string_view) const {
  const auto grpc_status =
      Grpc::Common::getGrpcStatus(response_trailers, response_headers, info, true);
  if (!grpc_status.has_value()) {
    return absl::nullopt;
  }
  const auto grpc_status_message = Grpc::Utility::grpcStatusToString(grpc_status.value());
  if (grpc_status_message == EMPTY_STRING || grpc_status_message == "InvalidCode") {
    return std::to_string(grpc_status.value());
  }
  return grpc_status_message;
}

ProtobufWkt::Value
GrpcStatusFormatter::formatValue(const Http::RequestHeaderMap&,
                                 const Http::ResponseHeaderMap& response_headers,
                                 const Http::ResponseTrailerMap& response_trailers,
                                 const StreamInfo::StreamInfo& info, absl::string_view) const {
  const auto grpc_status =
      Grpc::Common::getGrpcStatus(response_trailers, response_headers, info, true);
  if (!grpc_status.has_value()) {
    return unspecifiedValue();
  }
  const auto grpc_status_message = Grpc::Utility::grpcStatusToString(grpc_status.value());
  if (grpc_status_message == EMPTY_STRING || grpc_status_message == "InvalidCode") {
    return ValueUtil::stringValue(std::to_string(grpc_status.value()));
  }
  return ValueUtil::stringValue(grpc_status_message);
}

MetadataFormatter::MetadataFormatter(const std::string& filter_namespace,
                                     const std::vector<std::string>& path,
                                     absl::optional<size_t> max_length,
                                     MetadataFormatter::GetMetadataFunction get_func)
    : filter_namespace_(filter_namespace), path_(path), max_length_(max_length),
      get_func_(get_func) {}

absl::optional<std::string>
MetadataFormatter::formatMetadata(const envoy::config::core::v3::Metadata& metadata) const {
  ProtobufWkt::Value value = formatMetadataValue(metadata);
  if (value.kind_case() == ProtobufWkt::Value::kNullValue) {
    return absl::nullopt;
  }

  std::string str;
  if (Runtime::runtimeFeatureEnabled("envoy.reloadable_features.unquote_log_string_values") &&
      value.kind_case() == ProtobufWkt::Value::kStringValue) {
    str = value.string_value();
  } else {
    str = MessageUtil::getJsonStringFromMessageOrDie(value, false, true);
  }
  truncate(str, max_length_);
  return str;
}

ProtobufWkt::Value
MetadataFormatter::formatMetadataValue(const envoy::config::core::v3::Metadata& metadata) const {
  if (path_.empty()) {
    const auto filter_it = metadata.filter_metadata().find(filter_namespace_);
    if (filter_it == metadata.filter_metadata().end()) {
      return unspecifiedValue();
    }
    ProtobufWkt::Value output;
    output.mutable_struct_value()->CopyFrom(filter_it->second);
    return output;
  }

  const ProtobufWkt::Value& val = Metadata::metadataValue(&metadata, filter_namespace_, path_);
  if (val.kind_case() == ProtobufWkt::Value::KindCase::KIND_NOT_SET) {
    return unspecifiedValue();
  }

  return val;
}

absl::optional<std::string> MetadataFormatter::format(const Http::RequestHeaderMap&,
                                                      const Http::ResponseHeaderMap&,
                                                      const Http::ResponseTrailerMap&,
                                                      const StreamInfo::StreamInfo& stream_info,
                                                      absl::string_view) const {
  auto metadata = get_func_(stream_info);
  return (metadata != nullptr) ? formatMetadata(*metadata) : absl::nullopt;
}

ProtobufWkt::Value MetadataFormatter::formatValue(const Http::RequestHeaderMap&,
                                                  const Http::ResponseHeaderMap&,
                                                  const Http::ResponseTrailerMap&,
                                                  const StreamInfo::StreamInfo& stream_info,
                                                  absl::string_view) const {
  auto metadata = get_func_(stream_info);
  return formatMetadataValue((metadata != nullptr) ? *metadata
                                                   : envoy::config::core::v3::Metadata());
}
// TODO(glicht): Consider adding support for route/listener/cluster metadata as suggested by
// @htuch. See: https://github.com/envoyproxy/envoy/issues/3006
DynamicMetadataFormatter::DynamicMetadataFormatter(const std::string& filter_namespace,
                                                   const std::vector<std::string>& path,
                                                   absl::optional<size_t> max_length)
    : MetadataFormatter(filter_namespace, path, max_length,
                        [](const StreamInfo::StreamInfo& stream_info) {
                          return &stream_info.dynamicMetadata();
                        }) {}

ClusterMetadataFormatter::ClusterMetadataFormatter(const std::string& filter_namespace,
                                                   const std::vector<std::string>& path,
                                                   absl::optional<size_t> max_length)
    : MetadataFormatter(filter_namespace, path, max_length,
                        [](const StreamInfo::StreamInfo& stream_info)
                            -> const envoy::config::core::v3::Metadata* {
                          auto cluster_info = stream_info.upstreamClusterInfo();
                          if (!cluster_info.has_value() || cluster_info.value() == nullptr) {
                            return nullptr;
                          }
                          return &cluster_info.value()->metadata();
                        }) {}

FilterStateFormatter::FilterStateFormatter(const std::string& key,
                                           absl::optional<size_t> max_length,
                                           bool serialize_as_string)
    : key_(key), max_length_(max_length), serialize_as_string_(serialize_as_string) {}

const Envoy::StreamInfo::FilterState::Object*
FilterStateFormatter::filterState(const StreamInfo::StreamInfo& stream_info) const {
  const StreamInfo::FilterState& filter_state = stream_info.filterState();
  if (!filter_state.hasDataWithName(key_)) {
    return nullptr;
  }
  return &filter_state.getDataReadOnly<StreamInfo::FilterState::Object>(key_);
}

absl::optional<std::string> FilterStateFormatter::format(const Http::RequestHeaderMap&,
                                                         const Http::ResponseHeaderMap&,
                                                         const Http::ResponseTrailerMap&,
                                                         const StreamInfo::StreamInfo& stream_info,
                                                         absl::string_view) const {
  const Envoy::StreamInfo::FilterState::Object* state = filterState(stream_info);
  if (!state) {
    return absl::nullopt;
  }

  if (serialize_as_string_) {
    absl::optional<std::string> plain_value = state->serializeAsString();
    if (plain_value.has_value()) {
      truncate(plain_value.value(), max_length_);
      return plain_value.value();
    }
    return absl::nullopt;
  }

  ProtobufTypes::MessagePtr proto = state->serializeAsProto();
  if (proto == nullptr) {
    return absl::nullopt;
  }

  std::string value;
  const auto status = Protobuf::util::MessageToJsonString(*proto, &value);
  if (!status.ok()) {
    // If the message contains an unknown Any (from WASM or Lua), MessageToJsonString will fail.
    // TODO(lizan): add support of unknown Any.
    return absl::nullopt;
  }

  truncate(value, max_length_);
  return value;
}

ProtobufWkt::Value FilterStateFormatter::formatValue(const Http::RequestHeaderMap&,
                                                     const Http::ResponseHeaderMap&,
                                                     const Http::ResponseTrailerMap&,
                                                     const StreamInfo::StreamInfo& stream_info,
                                                     absl::string_view) const {
  const Envoy::StreamInfo::FilterState::Object* state = filterState(stream_info);
  if (!state) {
    return unspecifiedValue();
  }

  if (serialize_as_string_) {
    absl::optional<std::string> plain_value = state->serializeAsString();
    if (plain_value.has_value()) {
      truncate(plain_value.value(), max_length_);
      return ValueUtil::stringValue(plain_value.value());
    }
    return unspecifiedValue();
  }

  ProtobufTypes::MessagePtr proto = state->serializeAsProto();
  if (!proto) {
    return unspecifiedValue();
  }

  ProtobufWkt::Value val;
  if (MessageUtil::jsonConvertValue(*proto, val)) {
    return val;
  }
  return unspecifiedValue();
}

// Given a token, extract the command string between parenthesis if it exists.
std::string SystemTimeFormatter::parseFormat(const std::string& token, size_t parameters_start) {
  const size_t parameters_length = token.length() - (parameters_start + 1);
  return token[parameters_start - 1] == '(' ? token.substr(parameters_start, parameters_length)
                                            : "";
}

// A SystemTime formatter that extracts the startTime from StreamInfo. Must be provided
// an access log token that starts with `START_TIME`.
StartTimeFormatter::StartTimeFormatter(const std::string& token)
    : SystemTimeFormatter(
          parseFormat(token, sizeof("START_TIME(") - 1),
          std::make_unique<SystemTimeFormatter::TimeFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> absl::optional<SystemTime> {
                return stream_info.startTime();
              })) {}

// A SystemTime formatter that extracts the startTime from StreamInfo. Must be provided
// an access log token that starts with `INT64_START_TIME`.
StartTimeInt64Formatter::StartTimeInt64Formatter(const std::string& token)
    : SystemTimeInt64Formatter(
          parseFormat(token, sizeof("INT64_START_TIME(") - 1),
          std::make_unique<SystemTimeFormatter::TimeFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> absl::optional<SystemTime> {
                return stream_info.startTime();
              })) {}

// A SystemTime formatter that optionally extracts the start date from the downstream peer's
// certificate. Must be provided an access log token that starts with `DOWNSTREAM_PEER_CERT_V_START`
DownstreamPeerCertVStartFormatter::DownstreamPeerCertVStartFormatter(const std::string& token)
    : SystemTimeFormatter(
          parseFormat(token, sizeof("DOWNSTREAM_PEER_CERT_V_START(") - 1),
          std::make_unique<SystemTimeFormatter::TimeFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> absl::optional<SystemTime> {
                const auto connection_info =
                    stream_info.downstreamAddressProvider().sslConnection();
                return connection_info != nullptr ? connection_info->validFromPeerCertificate()
                                                  : absl::optional<SystemTime>();
              })) {}

// A SystemTime formatter that optionally extracts the end date from the downstream peer's
// certificate. Must be provided an access log token that starts with `DOWNSTREAM_PEER_CERT_V_END`
DownstreamPeerCertVEndFormatter::DownstreamPeerCertVEndFormatter(const std::string& token)
    : SystemTimeFormatter(
          parseFormat(token, sizeof("DOWNSTREAM_PEER_CERT_V_END(") - 1),
          std::make_unique<SystemTimeFormatter::TimeFieldExtractor>(
              [](const StreamInfo::StreamInfo& stream_info) -> absl::optional<SystemTime> {
                const auto connection_info =
                    stream_info.downstreamAddressProvider().sslConnection();
                return connection_info != nullptr ? connection_info->expirationPeerCertificate()
                                                  : absl::optional<SystemTime>();
              })) {}

SystemTimeFormatter::SystemTimeFormatter(const std::string& format, TimeFieldExtractorPtr f)
    : date_formatter_(format), time_field_extractor_(std::move(f)) {
  // Validate the input specifier here. The formatted string may be destined for a header, and
  // should not contain invalid characters {NUL, LR, CF}.
  if (std::regex_search(format, getSystemTimeFormatNewlinePattern())) {
    throw EnvoyException("Invalid header configuration. Format string contains newline.");
  }
}

absl::optional<std::string> SystemTimeFormatter::format(const Http::RequestHeaderMap&,
                                                        const Http::ResponseHeaderMap&,
                                                        const Http::ResponseTrailerMap&,
                                                        const StreamInfo::StreamInfo& stream_info,
                                                        absl::string_view) const {
  const auto time_field = (*time_field_extractor_)(stream_info);
  if (!time_field.has_value()) {
    return absl::nullopt;
  }
  if (date_formatter_.formatString().empty()) {
    return AccessLogDateTimeFormatter::fromTime(time_field.value());
  }
  return date_formatter_.fromTime(time_field.value());
}

ProtobufWkt::Value SystemTimeFormatter::formatValue(
    const Http::RequestHeaderMap& request_headers, const Http::ResponseHeaderMap& response_headers,
    const Http::ResponseTrailerMap& response_trailers, const StreamInfo::StreamInfo& stream_info,
    absl::string_view local_reply_body) const {
  return ValueUtil::optionalStringValue(
      format(request_headers, response_headers, response_trailers, stream_info, local_reply_body));
}

SystemTimeInt64Formatter::SystemTimeInt64Formatter(const std::string& format, TimeFieldExtractorPtr f)
    : SystemTimeFormatter(format, std::move(f)) { }

ProtobufWkt::Value SystemTimeInt64Formatter::formatValue(
    const Http::RequestHeaderMap&, const Http::ResponseHeaderMap&,
    const Http::ResponseTrailerMap&, const StreamInfo::StreamInfo& stream_info,
    absl::string_view) const {
  const auto time_field = (*time_field_extractor_)(stream_info);
  if (!time_field.has_value()) {
    return unspecifiedValue();
  }
  return ValueUtil::numberValue(std::chrono::duration_cast<std::chrono::milliseconds>(time_field.value().time_since_epoch()).count());
}

} // namespace Formatter
} // namespace Envoy
