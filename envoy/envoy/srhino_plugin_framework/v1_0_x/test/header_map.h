#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_0_x/header_map.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Test {
class MockHeaderMap : public HeaderMap {
public:
  MockHeaderMap() {
  }

public:
  MOCK_METHOD(std::string_view, get, (const std::string_view&), (const));
  MOCK_METHOD(void, get, (const std::string_view&, std::vector<std::string_view>&), (const));
  MOCK_METHOD(void, add, (const std::string_view&, const std::string_view&), ());
  MOCK_METHOD(void, set, (const std::string_view&, const std::string_view&), ());
  MOCK_METHOD(void, modify, (const std::string_view&, std::function<bool(const std::string_view& value, std::string& new_value)>), ());
  MOCK_METHOD(std::size_t, remove, (const std::string_view&), ());
  MOCK_METHOD(void, traverse,
              (std::function<bool(const std::string_view& key, const std::string_view& value)>),
              (const));
  MOCK_METHOD(std::unique_ptr<HeaderMap>, create, (), (const));
  MOCK_METHOD(std::unique_ptr<HeaderMap>, create,
              ((const std::initializer_list<std::pair<std::string_view, std::string_view>>&)),
              (const));

public:
};
} // namespace Test
} // namespace v1_0_x
} // namespace SrhinoPluginFramework