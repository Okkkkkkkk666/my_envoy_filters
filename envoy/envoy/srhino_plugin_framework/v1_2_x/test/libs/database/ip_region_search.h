#pragma once

#include <gmock/gmock.h>
#include <srhino_plugin_framework/libs/database/ip_region_search.h>

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Database {

using namespace SrhinoPluginFramework::Libs::Database;

class MockIpRegionSearch : public SrhinoPluginFramework::Libs::Database::IpRegionSearch {
public:
  MockIpRegionSearch() {}

public:
  MOCK_METHOD(bool, init_file, (), ());
  MOCK_METHOD(bool, init_vector_index, (), ());
  MOCK_METHOD(bool, init_content, (), ());
  MOCK_METHOD(std::string, search, (uint32_t), ());
  MOCK_METHOD(std::string, search, (const std::string&), ());
  MOCK_METHOD(std::string, get_country, (const std::string&), ());
  MOCK_METHOD(std::string, get_province, (const std::string&), ());
  MOCK_METHOD(std::string, get_city, (const std::string&), ());
};

} // namespace Database
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework
