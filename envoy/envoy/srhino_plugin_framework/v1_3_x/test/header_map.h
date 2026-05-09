#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_3_x/header_map.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Test {
class MockHeaderMap : public HeaderMap {
public:
  MockHeaderMap() {
  }

public:
  MOCK_METHOD(std::string_view, get, (const std::string_view&), (const));
  MOCK_METHOD(void, get, (const std::string_view&, std::vector<std::string_view>&), (const));
  MOCK_METHOD(void, add, (const std::string_view&, const std::string_view&), ());
  MOCK_METHOD(void, add, (const std::string_view&, uint64_t), ());
  MOCK_METHOD(void, set, (const std::string_view&, const std::string_view&), ());
  MOCK_METHOD(void, setReference, (const std::string_view&, const std::string_view&), ());
  MOCK_METHOD(void, addReference, (const std::string_view&, const std::string_view&), ());
  MOCK_METHOD(void, clear, (), (const));
  MOCK_METHOD(bool, empty, (), (const));
  MOCK_METHOD(std::string_view, path, (), (const));
  MOCK_METHOD(std::string_view, authority, (), (const));
  MOCK_METHOD(std::string_view, protocol, (), (const));
  MOCK_METHOD(std::string_view, method, (), (const));
  MOCK_METHOD(std::string_view, host, (), (const));
  MOCK_METHOD(uint32_t, statusCode, (), (const));
  MOCK_METHOD(std::string_view, userAgent, (), (const));
  MOCK_METHOD(std::string_view, forwardedFor, (), (const));
  MOCK_METHOD(std::string_view, contentEncoding, (), (const));
  MOCK_METHOD(std::string_view, contentType, (), (const));
  MOCK_METHOD(uint64_t, byteSize, (), (const));
  MOCK_METHOD(size_t, size, (), (const));
  MOCK_METHOD(size_t, removePrefix, (const std::string_view&), (const));

  MOCK_METHOD(void, parseCookieValue,
              (const std::string_view&, const std::function<bool(const std::string_view)>&), (const));
  MOCK_METHOD(void, parseSetCookieValue,
              (const std::string_view&, const std::function<bool(const std::string_view)>&), (const));

  MOCK_METHOD(void, modify,
              (const std::string_view&,
               std::function<bool(const std::string_view& value, std::string& new_value)>),
              ());
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
} // namespace v1_3_x
} // namespace SrhinoPluginFramework