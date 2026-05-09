#pragma once

#include <cstdint>
#include <vector>
#include <string_view>
#include <memory>

namespace SrhinoPluginFramework {
namespace v1_4_x {
class DataSlices;
using DataSlicesPtr = std::unique_ptr<DataSlices>;

class DataSlices {
public:
  virtual ~DataSlices() = default;

public:
  class Iterator {
    friend class DataSlices;

  public:
    std::uint8_t& operator*() { return data_.at(index_); }

    Iterator operator++() {
      ++index_;
      return *this;
    }

    Iterator operator--() {
      --index_;
      return *this;
    }

    bool operator==(const Iterator& rhs) const {
      return (index_ == rhs.index_) && (&data_ == &rhs.data_);
    }

    bool operator!=(const Iterator& rhs) const {
      return (index_ != rhs.index_) || (&data_ != &rhs.data_);
    }

  private:
    Iterator(std::size_t index, DataSlices& data) : index_(index), data_(data) {}
    std::size_t index_;
    DataSlices& data_;
  };

public:
  Iterator begin() { return {0, *this}; }
  Iterator end() { return {size(), *this}; }
  virtual std::size_t size() const = 0;
  virtual std::uint8_t& at(std::size_t pos) = 0;
  virtual std::vector<std::string_view> slices() = 0;
  virtual std::string toString() const = 0;
  virtual DataSlicesPtr create() const = 0;
  /**
   * 设置DataSlices的内容
   * DataSlices原有的内容将被替换成data
   */
  virtual void set(const std::string_view& data) = 0;
  /**
   * 设置DataSlices的内容
   * DataSlices原有的内容将被替换成data，同时data的内容将被清空。
   */
  virtual void set(DataSlices &data) = 0;
  /**
   * 在DataSlices的开头添加内容
   */
  virtual void prepend(const std::string_view &data) = 0;
  /**
   * 在DataSlices的开头添加内容
   * data的内容将被清空。
   */
  virtual void prepend(DataSlices &data) = 0;
  /**
   * 在DataSlices的结尾添加内容
   */
  virtual void append(const std::string_view &data) = 0;
  /**
   * 在DataSlices的结尾添加内容
   * data的内容将被清空。
   */
  virtual void append(DataSlices &data) = 0;
  /**
   * 同append
   */
  virtual void move(DataSlices& rhs) = 0;
};

} // namespace v1_4_x
} // namespace SrhinoPluginFramework