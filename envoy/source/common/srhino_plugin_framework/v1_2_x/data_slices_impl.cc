#include "source/common/srhino_plugin_framework/v1_2_x/data_slices_impl.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
std::uint8_t DataSlicesImpl::out_of_range_value_ = 0;

std::size_t DataSlicesImpl::size() const {
  if (!data_) {
    return 0;
  }

  return data_->length();
}

std::uint8_t& DataSlicesImpl::at(std::size_t pos) {
  if (!data_) {
    return out_of_range_value_;
  }

  std::size_t bytes_to_skip = pos;

  auto raw_slices = data_->getRawSlices();
  for (const Envoy::Buffer::RawSlice& slice : raw_slices) {
    if (slice.len_ <= bytes_to_skip) {
      bytes_to_skip -= slice.len_;
      continue;
    } else {
      return static_cast<std::uint8_t*>(slice.mem_)[bytes_to_skip];
    }
  }

  return out_of_range_value_;
}

std::vector<std::string_view> DataSlicesImpl::slices() {
  std::vector<std::string_view> result;
  if (!data_) {
    return result;
  }

  auto raw_slices = data_->getRawSlices();
  for (const Envoy::Buffer::RawSlice& slice : raw_slices) {
    result.emplace_back(static_cast<const char*>(slice.mem_), slice.len_);
  }

  return result;
}

std::string DataSlicesImpl::toString() const {
  if (!data_) {
    return Envoy::EMPTY_STRING;
  }

  return data_->toString();
}

void DataSlicesImpl::set(const std::string_view &data) {
  data_->drain(data_->length());
  data_->add(absl::string_view(data.data(), data.size()));
}

void DataSlicesImpl::set(DataSlices &data) {
  data_->drain(data_->length());
  data_->move(*dynamic_cast<DataSlicesImpl&>(data).raw());
}


void DataSlicesImpl::prepend(const std::string_view &data) {
  data_->prepend(absl::string_view(data.data(), data.size()));
}

void DataSlicesImpl::prepend(DataSlices &data) {
  data_->prepend(*dynamic_cast<DataSlicesImpl&>(data).raw());
}

void DataSlicesImpl::append(const std::string_view &data) {
  data_->add(absl::string_view(data.data(), data.size()));
}

void DataSlicesImpl::append(DataSlices &rhs) {
  data_->move(*dynamic_cast<DataSlicesImpl&>(rhs).raw());
}

void DataSlicesImpl::move(DataSlices &rhs) {
  data_->move(*dynamic_cast<DataSlicesImpl&>(rhs).raw());
}

} // namespace v1_2_x
} // namespace SrhinoPluginFramework