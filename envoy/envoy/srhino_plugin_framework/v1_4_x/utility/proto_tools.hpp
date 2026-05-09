#pragma once

#include <string>
#include <memory>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>

#include "empty_string.hpp"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Utility {
class ProtoTools {
public:
  /**
   * json字符串转protobuf::Message
   * @param json json字符串
   * @param message 接收转换后的protobuf::Message
   * @param ignore_unknown_fields 是否忽略未知字段，默认为true
   * @return std::string 如果转换失败则描述失败原因，当为空则表示转换成功
   */
  static std::string jsonToMessage(const std::string& json, google::protobuf::Message& message,
                                   bool ignore_unknown_fields = true) {
    using namespace google::protobuf::util;

    JsonParseOptions options;
    options.ignore_unknown_fields = ignore_unknown_fields;
    options.case_insensitive_enum_parsing = true;
    const auto strict_status = JsonStringToMessage(json, &message, options);
    if (!strict_status.ok()) {
      std::string error = "jsonToMessage fail: ";
      error += strict_status.ToString();
      error += " ";
      error += message.DebugString();
      return error;
    }

    return EMPTY_STRING;
  }

  /**
   * json字符串转protobuf::Message
   * @param json json字符串
   * @param size json长度，单位字节
   * @param message 接收转换后的protobuf::Message
   * @param ignore_unknown_fields 是否忽略未知字段，默认为true
   * @return std::string 如果转换失败则描述失败原因，当为空则表示转换成功
   */
  static std::string jsonToMessage(const char* json, uint32_t size,
                                   google::protobuf::Message& message,
                                   bool ignore_unknown_fields = true) {
    using namespace google::protobuf::util;

    JsonParseOptions options;
    options.ignore_unknown_fields = ignore_unknown_fields;
    options.case_insensitive_enum_parsing = true;
    const auto strict_status = JsonStringToMessage({json, size}, &message, options);
    if (!strict_status.ok()) {
      std::string error = "jsonToMessage fail: ";
      error += strict_status.ToString();
      error += " ";
      error += message.DebugString();
      return error;
    }

    return EMPTY_STRING;
  }

  /**
   * protobuf::Message转json字符串
   * @param message protobuf::Message
   * @param json 接收转换后的json字符串
   * @param always_print_primitive_fields 总是打印值为默认值的项，默认为 false
   * @param always_print_enums_as_ints 总是使用int表示枚举，默认为 false
   * @return std::string 如果转换失败则描述失败原因，当为空则表示转换成功
   */
  static std::string messageToJson(const google::protobuf::Message& message, std::string& json,
                                   bool always_print_primitive_fields = false,
                                   bool always_print_enums_as_ints = false) {
    using namespace google::protobuf::util;

    JsonPrintOptions json_options;
    json_options.preserve_proto_field_names = true;
    json_options.always_print_primitive_fields = always_print_primitive_fields;
    json_options.always_print_enums_as_ints = always_print_enums_as_ints;
    const auto status = MessageToJsonString(message, &json, json_options);
    if (!status.ok()) {
      std::string error = "messageToJson fail: ";
      error += status.ToString();
      error += " ";
      error += message.DebugString();
      return error;
    }

    return EMPTY_STRING;
  }

  /**
   * 两个protobuf::Message之间相互转换
   * @param msg1 protobuf::Message
   * @param msg2 protobuf::Message
   * @return bool 转换成功返回true,否则返回false
   */
  template <class MessageT1, class MessageT2>
  static bool convertMessage(const MessageT1& msg1, MessageT2& msg2) {
    std::string json;
    if (!messageToJson(msg1, json).empty()) {
      return false;
    }

    if (!jsonToMessage(json, msg2).empty()) {
      return false;
    }

    return true;
  }
};

} // namespace Utility
} // namespace v1_4_x
} // namespace SrhinoPluginFramework