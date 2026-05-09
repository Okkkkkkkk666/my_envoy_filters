#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_4_x/libs/factory.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/config/config.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/database/database.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/encode/encode.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/net/net.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/regex/regex.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/thread_local/thread_local.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/public_lib/public_lib.h"

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {

using namespace SrhinoPluginFramework::Libs;
using testing::_;
class MockFactory : public SrhinoPluginFramework::Libs::Factory {
public:
  MockFactory() {
    mock_config_ = std::make_shared<Config::MockConfig>();
    ON_CALL(*this, config()).WillByDefault(testing::Return(mock_config_));
    mock_encode_ = std::make_shared<Encode::MockEncode>();
    ON_CALL(*this, encode()).WillByDefault(testing::Return(mock_encode_));
    mock_regex_ = std::make_shared<Regex::MockRegex>();
    ON_CALL(*this, regex()).WillByDefault(testing::Return(mock_regex_));
    mock_database_ = std::make_shared<Database::MockDatabase>();
    ON_CALL(*this, database(_)).WillByDefault(testing::Return(mock_database_));
    ON_CALL(*this, log(_, _, _, _, _))
        .WillByDefault(  
            [](LogLevel, const char* file, const char* func, int line, std::function<std::string()> format_cb) {
              std::cout << std::format("[{}][{}:{}][{}] {}\n", std::chrono::system_clock::now(),
                                       file, line, func, format_cb());
            });
    mock_net_ = std::make_shared<Net::MockNet>();
    ON_CALL(*this, net(_)).WillByDefault(testing::Return(mock_net_));
    mock_thread_local_ = std::make_shared<ThreadLocal::MockThreadLocal>();
    ON_CALL(*this, threadLocal(_)).WillByDefault(testing::Return(mock_thread_local_));
    ON_CALL(*this, getThreadPoolSingleTon).WillByDefault(testing::ReturnRef(thread_pool_));
    mock_public_lib_ = std::make_shared<PublicLib::MockPublicLib>();
    ON_CALL(*this, publicLib()).WillByDefault(testing::Return(mock_public_lib_));
  }

public:
  MOCK_METHOD(Config::ConfigSharedPtr, config, (), (const));
  MOCK_METHOD(FileSystem::FileSystemSharedPtr, filesystem, (), (const));
  MOCK_METHOD(Regex::RegexSharedPtr, regex, (), (const));
  MOCK_METHOD(Encode::EncodeSharedPtr, encode, (), (const));
  MOCK_METHOD(ThreadPool&, getThreadPoolSingleTon, (), (const));
  MOCK_METHOD(PublicLib::PublicLibSharedPtr, publicLib, (), (const));

public:
  MOCK_METHOD(Database::DatabaseSharedPtr, database, (LibsState libs_state), (const));
  MOCK_METHOD(Net::NetSharedPtr, net, (LibsState libs_state), (const));
  MOCK_METHOD(ThreadLocal::ThreadLocalSharedPtr, threadLocal, (LibsState libs_state), (const));

public:
  MOCK_METHOD(void, log, (LogLevel, const char*, const char*, int, std::function<std::string()> format_cb), (const));

public:
  std::shared_ptr<Config::MockConfig> mock_config_;
  std::shared_ptr<Encode::MockEncode> mock_encode_;
  std::shared_ptr<Regex::MockRegex> mock_regex_;
  std::shared_ptr<Database::MockDatabase> mock_database_;
  std::shared_ptr<Net::MockNet> mock_net_;
  std::shared_ptr<ThreadLocal::MockThreadLocal> mock_thread_local_;
  std::shared_ptr<PublicLib::MockPublicLib> mock_public_lib_;
  ThreadPool thread_pool_;
};

} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework