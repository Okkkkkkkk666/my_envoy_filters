#pragma once

#include <string>

#include "envoy/srhino_plugin_framework/v1_2_x/data_slices.h"
#include "envoy/buffer/buffer.h"
#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/empty_string.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
class DataSlicesImpl;
using DataSlicesImplPtr = std::unique_ptr<DataSlicesImpl>;

class DataSlicesImpl : public DataSlices {
public:
  DataSlicesImpl() { createData(); }
  DataSlicesImpl(Envoy::Buffer::Instance& data) : data_(&data) {}

public:
  std::size_t size() const override;
  std::uint8_t& at(std::size_t pos) override;
  std::vector<std::string_view> slices() override;
  std::string toString() const override;
  void set(const std::string_view &data) override;
  void set(DataSlices &data) override;
  void prepend(const std::string_view &data) override;
  void prepend(DataSlices &data) override;
  void append(const std::string_view &data) override;
  void append(DataSlices &data) override;
  void move(DataSlices& rhs) override;

public:
  Envoy::Buffer::Instance* raw() { return data_; }
  const Envoy::Buffer::Instance* raw() const { return data_; }
  void setData(Envoy::Buffer::Instance& data) { data_ = &data; }
  DataSlicesPtr create() const { return std::make_unique<DataSlicesImpl>(); }

private:
  void createData() {
    buffer_ptr_ = std::make_unique<Envoy::Buffer::OwnedImpl>();
    data_ = buffer_ptr_.get();
  }

private:
  Envoy::Buffer::Instance* data_{nullptr};
  std::unique_ptr<Envoy::Buffer::OwnedImpl> buffer_ptr_{nullptr};
  static std::uint8_t out_of_range_value_;
};
} // namespace v1_2_x
} // namespace SrhinoPluginFramework