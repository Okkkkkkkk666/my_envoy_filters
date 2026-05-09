#pragma once
#include <iostream>
#include <iomanip>
#include <regex>
#include <memory>
#include <string>
#include <algorithm>
#include <sstream>
#include "source/common/common/logger.h"
#include "absl/strings/str_split.h"
#include "filters/api/envoy/extensions/filters/http/weak_password_check/v3/weak_password.pb.h"

#include "database.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {
namespace Impl {

enum DateFormat {
  DataFormatYearMonthDay = 1,
  DataFormatYearMonth    = 2,
  DataFormatMonthDay     = 4,
  DataFormatYear         = 8,
  DataFormatMonth        = 16,
  DataFormatDay          = 32,
};

namespace v3 = envoy::extensions::filters::http::weak_password_check::v3;

#define GENERATE_MD5_WEAK_PASSWORD_ITEM(item, password)                                            \
  {                                                                                                \
    (item).plain_password = (password);                                                            \
    (item).encrypt_type = ENCRYPT_TYPE_MD5;                                                        \
    (item).encrypt_password = WpdUtility::MD5(password);                                           \
    (item).key = WpdUtility::getKeyByEncryptType((item).encrypt_password, ENCRYPT_TYPE_MD5);       \
  }

class CustomPassword : public Logger::Loggable<Logger::Id::filter> {
public:
  CustomPassword(const v3::CustomPassword& custom_password);
  ~CustomPassword();

public:
  bool getPlainPassword(uint64_t key, std::string& plain_password) {
    return custom_rainbow_ ? custom_rainbow_->getPlainPassword(key, plain_password) : false;
  };
  // static std::string md5(const std::string& data);

private:
  bool get_timestamps(const std::string& date_str, time_t& time);
  std::string generateRuleHash();
  std::string generatePasswordTableName();
  void generateDates(std::set<std::string>& dates);
  void generateTwoParts(std::set<std::string>& two_parts);
  void fillWeakPasswordTable();

public:
  static const size_t SqlMaxLinesPerInsert{200000};

private:
  const std::vector<std::set<std::string>> rule_config_;
  const std::vector<std::string> date_range_;
  const uint16_t date_format_;
  const std::string filter_instance_id_;
  const std::string rule_hash_;

  CustomRainbowDbPtr custom_rainbow_{nullptr};
};
using CustomPasswordPtr = std::shared_ptr<CustomPassword>;

} // namespace Impl
} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy