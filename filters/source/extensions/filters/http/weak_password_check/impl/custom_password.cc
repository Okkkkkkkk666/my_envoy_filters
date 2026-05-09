#include "custom_password.h"
#include "rule.h"
#include "openssl/md5.h"
#include "source/common/common/hex.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {
namespace Impl {

CustomPassword::CustomPassword(const v3::CustomPassword& custom_password)
    : rule_config_([&custom_password]() {
        std::vector<std::set<std::string>> rules;
        for (const auto& config : custom_password.rule_config()) {
          std::set<std::string> part_rules;
          for (const auto& rule : config.part_rule()) {
            part_rules.insert(rule);
          }
          rules.emplace_back(part_rules);
        }
        return rules;
      }()),
      date_range_([&custom_password]() {
        std::vector<std::string> range;
        for (const auto& date : custom_password.date_range()) {
          range.emplace_back(date);
        }
        return range;
      }()),
      date_format_(custom_password.date_format()),
      filter_instance_id_(custom_password.filter_instance_id()), rule_hash_(generateRuleHash()),
      custom_rainbow_(std::make_shared<CustomRainbowDb>()) {

  //init custom rule_info table
  custom_rainbow_->createRuleInfoTable();

  std::string password_table_name = custom_rainbow_->getWeakPasswordTableNameByHash(rule_hash_);
  if (password_table_name.empty()) {
    password_table_name = generatePasswordTableName();
    custom_rainbow_->setWeakPasswordTableName(password_table_name);
    // create new password table
    if (custom_rainbow_->createWeakPasswordTable()) {
      fillWeakPasswordTable();
      if (custom_rainbow_->insertRule(filter_instance_id_, rule_hash_)) {
        ENVOY_LOG(debug,
                  "insert rule instance_id: {} rule_hash: {}, password_table_name: {} success",
                  filter_instance_id_, rule_hash_, password_table_name);
      } else {
        ENVOY_LOG(debug, "insert rule instance_id: {} rule_hash: {}, password_table_name: {} fail",
                  filter_instance_id_, rule_hash_, password_table_name);
      }
    } else {
      ENVOY_LOG(error, "create weak_md5 table failed");
    }

    // delete old rule
    custom_rainbow_->deleteOldRules();
  } else {
    if (custom_rainbow_->isRuleExist(filter_instance_id_, rule_hash_)) {
      if (custom_rainbow_->updateRuleInuse(filter_instance_id_, rule_hash_)) {
        ENVOY_LOG(debug, "update instance_id: {}, rule_hash: {} success", filter_instance_id_, rule_hash_);
      } else {
        ENVOY_LOG(debug, "update instance_id: {}, rule_hash: {} fail", filter_instance_id_, rule_hash_);
      }
    } else {
      if (custom_rainbow_->insertRule(filter_instance_id_, rule_hash_)) {
        ENVOY_LOG(debug,
                  "insert rule instance_id: {} rule_hash: {}, password_table_name: {} success",
                  filter_instance_id_, rule_hash_, password_table_name);
      } else {
        ENVOY_LOG(debug, "insert rule instance_id: {} rule_hash: {}, password_table_name: {} fail",
                  filter_instance_id_, rule_hash_, password_table_name);
      }
    }
  }
}

CustomPassword::~CustomPassword() {
  // 执行设置unuse
  ENVOY_LOG(debug, __FUNCTION__);
  if (custom_rainbow_) {
    if (custom_rainbow_->updateRuleUnuse(filter_instance_id_, rule_hash_)) {
      ENVOY_LOG(debug, "update instance_id: {}, rule_hash: {} success", filter_instance_id_,
                rule_hash_);
    } else {
      ENVOY_LOG(debug, "update instance_id: {}, rule_hash: {} fail", filter_instance_id_,
                rule_hash_);
    }
    custom_rainbow_ = nullptr;
  }
}

std::string CustomPassword::generatePasswordTableName() {
  return WeakPasswordTable::TableNameWeakPassword + "_" + std::to_string(time(NULL));
}

bool CustomPassword::get_timestamps(const std::string& date_str, time_t& time) {
 std::istringstream ss(date_str);
  std::tm t = {};
  ss >> std::get_time(&t, "%Y-%m-%d");
  if (ss.fail()) {
    ENVOY_LOG(error, "parse date string: {} failed", date_str);
    return false;
  } else {
    time = std::mktime(&t);
    return true;
  }
}

void CustomPassword::generateDates(std::set<std::string>& dates) {
  if (date_range_.empty() || (date_format_ == 0)) {
    return;
  }
  time_t start_time;
  time_t end_time;
  if (get_timestamps(date_range_[0], start_time) && get_timestamps(date_range_[1], end_time)) {
    for (; start_time <= end_time; start_time += 86400) {
      std::tm t = {};
      localtime_r(&start_time, &t);

      if (date_format_ & DataFormatYearMonthDay) {
        std::ostringstream ss;
        ss << std::put_time(&t, "%Y%m%d");
        dates.insert(ss.str());
      }
      if (date_format_ & DataFormatYearMonth) {
        std::ostringstream ss;
        ss << std::put_time(&t, "%Y%m");
        dates.insert(ss.str());
      }
      if (date_format_ & DataFormatMonthDay) {
        std::ostringstream ss;
        ss << std::put_time(&t, "%m%d");
        dates.insert(ss.str());
      }
      if (date_format_ & DataFormatYear) {
        std::ostringstream ss;
        ss << std::put_time(&t, "%Y");
        dates.insert(ss.str());
      }
      if (date_format_ & DataFormatMonth) {
        std::ostringstream ss;
        ss << std::put_time(&t, "%m");
        dates.insert(ss.str());
      }
      if (date_format_ & DataFormatDay) {
        std::ostringstream ss;
        ss << std::put_time(&t, "%d");
        dates.insert(ss.str());
      }
    }
  }
}

/**
 * 生成前两部分的组合
*/
void CustomPassword::generateTwoParts(std::set<std::string>& two_parts) {
  // 现有需求文档不支持
  // for (const auto& parts : rule_config_) {
  //   for (const auto& part : parts) {
  //     two_parts.insert(part);
  //   }
  // }
  for (const auto& first_part : rule_config_[0]) {
    for (const auto& second_part : rule_config_[1]) {
      two_parts.insert(first_part + second_part);
    }
  }
}

// std::string CustomPassword::md5(const std::string& data) {
//     uint8_t buf[MD5_DIGEST_LENGTH];
//     MD5(reinterpret_cast<const uint8_t*>(data.data()), data.size(), buf);
//     return Envoy::Hex::encode(buf, MD5_DIGEST_LENGTH);
// }
/**
 * 考虑到生成数据可能很大，因此一边生成，一边写入数据库
*/
void CustomPassword::fillWeakPasswordTable() {
  std::set<std::string> two_parts;
  generateTwoParts(two_parts);
  std::set<std::string> dates;
  generateDates(dates);

  ENVOY_LOG(debug, "two_parts size: {}, dates size: {}", two_parts.size(), dates.size());

  uint32_t total_cnt = 0;
  uint32_t insert_cnt = 0;
  std::vector<struct WeakPasswordItem> rainbow_datas;
  // TODO, password length < WeakPasswordLength(6)
  for (const auto& parts : rule_config_) {
    for (const auto& part : parts) {
      struct WeakPasswordItem item;
      GENERATE_MD5_WEAK_PASSWORD_ITEM(item, part);
      rainbow_datas.emplace_back(item);
      if (rainbow_datas.size() >= SqlMaxLinesPerInsert) {
        total_cnt += rainbow_datas.size();
        insert_cnt += custom_rainbow_->insertWeakPasswordTable(rainbow_datas);
        rainbow_datas.clear();
      }
    }
  }
  for (const auto& two_part : two_parts) {
    struct WeakPasswordItem item;
    GENERATE_MD5_WEAK_PASSWORD_ITEM(item, two_part);
    rainbow_datas.emplace_back(item);
    if (rainbow_datas.size() >= SqlMaxLinesPerInsert) {
      total_cnt += rainbow_datas.size();
      insert_cnt += custom_rainbow_->insertWeakPasswordTable(rainbow_datas);
      rainbow_datas.clear();
    }
  }
  for (const auto& date : dates) {
    struct WeakPasswordItem item;
    GENERATE_MD5_WEAK_PASSWORD_ITEM(item, date);
    rainbow_datas.emplace_back(item);
    if (rainbow_datas.size() >= SqlMaxLinesPerInsert) {
      total_cnt += rainbow_datas.size();
      insert_cnt += custom_rainbow_->insertWeakPasswordTable(rainbow_datas);
      rainbow_datas.clear();
    }
  }

  for (const auto& two_part : two_parts) {
    for (const auto& date : dates) {
      {
        std::string password = two_part + date;
        struct WeakPasswordItem item;
        GENERATE_MD5_WEAK_PASSWORD_ITEM(item, password);
        rainbow_datas.emplace_back(item);
      }
      {
        std::string password = date + two_part;
        struct WeakPasswordItem item;
        GENERATE_MD5_WEAK_PASSWORD_ITEM(item, password);
        rainbow_datas.emplace_back(item);
      }

      if (rainbow_datas.size() >= SqlMaxLinesPerInsert) {
        total_cnt += rainbow_datas.size();
        insert_cnt += custom_rainbow_->insertWeakPasswordTable(rainbow_datas);
        rainbow_datas.clear();
      }
    }
  }

  if (rainbow_datas.size()) {
    total_cnt += rainbow_datas.size();
    insert_cnt += custom_rainbow_->insertWeakPasswordTable(rainbow_datas);
    rainbow_datas.clear();
  }
  ENVOY_LOG(debug, "generate count: {}, insert table: {} {} lines", total_cnt, custom_rainbow_->getWeakPasswordTableName(), insert_cnt);
}

std::string CustomPassword::generateRuleHash() {
  std::string rule_str;
  for (const auto& config : rule_config_) {
    // std::sort(config.begin(), config.end());
    for (const auto& rule : config) {
      rule_str.append(rule);
      rule_str.append("\n");
    }
  }
  for (const auto& date : date_range_) {
    rule_str.append(date);
    rule_str.append("\n");
  }
  rule_str.append(std::to_string(date_format_));
  return std::to_string(std::hash<std::string>{}(rule_str));
}

} // namespace Impl
} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy