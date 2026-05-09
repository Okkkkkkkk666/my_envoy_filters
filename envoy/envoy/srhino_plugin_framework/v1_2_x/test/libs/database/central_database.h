#pragma once

#include <gmock/gmock.h>
#include <srhino_plugin_framework/libs/database/central_database.h>

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Database {

using namespace SrhinoPluginFramework::Libs::Database;
using testing::_;
using testing::Return;

struct DataItem {
  std::string data;
  std::chrono::system_clock::time_point timeout;
};

class MockCentralDatabase : public SrhinoPluginFramework::Libs::Database::CentralDatabase {
public:
  MockCentralDatabase(const std::chrono::milliseconds& /*timeout*/,
                      const std::string& /*name_space*/) {
    EXPECT_CALL(*this, insert(_, _, _, _, _))
        .WillRepeatedly([](const std::string&, const void*, size_t, uint32_t, bool) {
          throw "not implemented!";
        });
    EXPECT_CALL(*this, del(_)).WillRepeatedly([](const std::string&) { throw "not implemented!"; });
    EXPECT_CALL(*this, update(_, _, _, _))
        .WillRepeatedly(
            [](const std::string&, const void*, size_t, uint32_t) { throw "not implemented!"; });
    EXPECT_CALL(*this, insertAsync(_, _, _, _, _, _))
        .WillRepeatedly([&](const std::string& key, InsertCallback cb, const void* data,
                            size_t size, uint32_t ttl, bool /*force*/) {
          EXPECT_EQ(success_callback_, nullptr);
          key_ = key;
          data_ = std::string(static_cast<const char*>(data), size);
          ttl_ = ttl;
          insert_cb_ = cb;

          success_callback_ = [this]() {
            struct DataItem item = {
                data_,
                std::chrono::system_clock::now() + std::chrono::seconds(ttl_),
            };
            central_database_map_[key_] = item;

            insert_cb_(1, key_, data_.data(), data_.size());
          };
        });
    EXPECT_CALL(*this, delAsync(_, _)).WillRepeatedly([&](const std::string& key, DelCallback cb) {
      EXPECT_EQ(success_callback_, nullptr);
      key_ = key;
      del_cb_ = cb;

      success_callback_ = [this]() {
        int result = 0;
        auto it = central_database_map_.find(key_);
        if (it != central_database_map_.end()) {
          central_database_map_.erase(it);
          result = 1;
        }
        del_cb_(result, key_);
      };
    });
    EXPECT_CALL(*this, getAsync(_, _, _, _, _))
        .WillRepeatedly([&](const std::string& key, GetCallback cb, const void* data, size_t size,
                          uint32_t ttl) {
          EXPECT_EQ(success_callback_, nullptr);
          key_ = key;
          data_ = std::string(static_cast<const char *>(data), size);
          ttl_ = ttl;
          get_cb_ = cb;

          success_callback_ = [this]() {
            auto timeout = std::chrono::system_clock::now() + std::chrono::seconds(ttl_);
            auto it = central_database_map_.find(key_);
            if (it != central_database_map_.end()) {
              it->second.timeout = timeout;
              get_cb_(1, key_, it->second.data.c_str(), it->second.data.size());
            } else {
              if (!data_.empty()) {
                // 插入
                struct DataItem item = {
                    data_,
                    timeout,
                };
                central_database_map_[key_] = item;
                get_cb_(2, key_, item.data.c_str(), item.data.size());
              } else {
                get_cb_(0, key_, nullptr, 0);
              }
            }
          };

        });
    EXPECT_CALL(*this, updateAsync(_, _, _, _, _))
        .WillRepeatedly([&](const std::string& key, UpdateCallback cb, const void* data,
                            size_t size, uint32_t ttl) {
          EXPECT_EQ(success_callback_, nullptr);
          key_ = key;
          data_ = std::string(static_cast<const char*>(data), size);
          ttl_ = ttl;
          update_cb_ = cb;

          success_callback_ = [this]() {
            int result = 0;
            auto it = central_database_map_.find(key_);
            if (it != central_database_map_.end()) {
              it->second.data = data_;
              it->second.timeout = std::chrono::system_clock::now() + std::chrono::seconds(ttl_);
              result = 1;
            }
            update_cb_(result, key_, data_.data(), data_.size());
          };
        });
    EXPECT_CALL(*this, cleanAsync(_)).WillRepeatedly([&](CleanCallback cb) {
      central_database_map_.clear();
      cb(1);
    });
    EXPECT_CALL(*this, cancel()).WillRepeatedly([]() {});
  }

public:
  MOCK_METHOD(void, insert, (const std::string&, const void*, size_t, uint32_t, bool), ());
  MOCK_METHOD(void, del, (const std::string&), ());
  MOCK_METHOD(void, update, (const std::string&, const void*, size_t, uint32_t), ());
  MOCK_METHOD(void, insertAsync,
              (const std::string&, InsertCallback, const void*, size_t, uint32_t, bool), ());
  MOCK_METHOD(void, delAsync, (const std::string&, DelCallback), ());
  MOCK_METHOD(void, getAsync, (const std::string&, GetCallback, const void*, size_t, uint32_t), ());
  MOCK_METHOD(void, updateAsync,
              (const std::string&, UpdateCallback, const void*, size_t, uint32_t), ());
  MOCK_METHOD(void, cleanAsync, (CleanCallback), ());
  MOCK_METHOD(void, cancel, (), ());

public:
  // 模拟异步调用
  void onSuccess() {
    if (success_callback_) {
      success_callback_();
      success_callback_ = nullptr;
    }
  };
  void onFail() {
    if (fail_callback_) {
      fail_callback_();
      fail_callback_ = nullptr;
    }
  };
private:
  using AsyncCallback = std::function<void()>;

private:
  AsyncCallback success_callback_{nullptr};
  AsyncCallback fail_callback_{nullptr};
  GetCallback get_cb_;
  InsertCallback insert_cb_;
  DelCallback del_cb_;
  UpdateCallback update_cb_;
  std::string key_;
  std::string data_;
  uint32_t ttl_;
  std::map<std::string, struct DataItem> central_database_map_;
};

} // namespace Database
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework