#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_0_x/data_slices.h"
using testing::_;

namespace SrhinoPluginFramework {

namespace Test {
class MockDataSlices : public virtual DataSlices {
public:
  MockDataSlices() {

    EXPECT_CALL(*this, size()).WillRepeatedly(testing::Invoke([&]() -> std::size_t {
      std::size_t length = 0;
      for (const std::string& slice : raw_slices_) {
        length += slice.length();
      }
      return length;
    }));

    EXPECT_CALL(*this, at(_)).WillRepeatedly(testing::Invoke([&](std::size_t pos) -> std::uint8_t& {
      std::size_t bytes_to_skip = pos;
      for (const std::string& slice : raw_slices_) {
        if (slice.length() <= bytes_to_skip) {
          bytes_to_skip -= slice.length();
        } else {
          return reinterpret_cast<std::uint8_t*>(const_cast<char*>(slice.data()))[bytes_to_skip];
        }
      }
      return out_of_range_value_;
    }));

    EXPECT_CALL(*this, slices())
        .WillRepeatedly(testing::Invoke([&]() -> std::vector<std::string_view> {
          std::vector<std::string_view> result;
          std::transform(raw_slices_.cbegin(), raw_slices_.cend(),
                         std::inserter(result, result.begin()),
                         [&](const std::string& in) { return std::string_view(in); });
          return result;
        }));

    EXPECT_CALL(*this, toString()).WillRepeatedly(testing::Invoke([&]() -> std::string {
      std::vector<std::string_view> result;
      std::string output;
      for (const std::string& slice : raw_slices_) {
        output.append(slice);
      }
      return output;
    }));

    EXPECT_CALL(*this, create()).WillRepeatedly(testing::Invoke([&]() -> DataSlicesPtr {
      return std::make_unique<MockDataSlices>();
    }));

    EXPECT_CALL(*this, set(_)).WillRepeatedly(testing::Invoke([&](const std::string_view& data) {
      raw_slices_.clear();
      raw_slices_.push_back(std::string(data.data(), data.size()));
    }));

    EXPECT_CALL(*this, move(_)).WillRepeatedly(testing::Invoke([&](DataSlices& rhs) {
      raw_slices_ = std::move(dynamic_cast<MockDataSlices&>(rhs).raw_slices_);
    }));

  }

public:
  MOCK_METHOD(std::size_t, size, (), (const));
  MOCK_METHOD(std::uint8_t&, at, (std::size_t pos), ());
  MOCK_METHOD(std::vector<std::string_view>, slices, (), ());
  MOCK_METHOD(std::string, toString, (), (const));
  MOCK_METHOD(DataSlicesPtr, create, (), (const));
  MOCK_METHOD(void, set, (const std::string_view& data), ());
  MOCK_METHOD(void, move, (DataSlices& rhs), ());

public:
  std::vector<std::string> raw_slices_;
  static inline std::uint8_t out_of_range_value_{0};
};

} // namespace Test

} // namespace SrhinoPluginFramework