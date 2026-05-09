#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace SrhinoPluginFramework {
namespace v1_0_x {
class HeaderMap {
public:
  virtual ~HeaderMap() = default;

public:
  virtual std::string_view get(const std::string_view& key) const = 0;
  virtual void get(const std::string_view& key, std::vector<std::string_view>& values) const = 0;
  virtual void add(const std::string_view& key, const std::string_view& value) = 0;
  virtual void set(const std::string_view& key, const std::string_view& value) = 0;
  /**
   * 修改http header value
   * @param key header key
   * @param cb 修改回调函数，value是修改前的值，new_value是修改后的值。
   *        当cb返回true，modify将当前header value设置为new_value；返回false，则什么也不做。
   */
  virtual void modify(const std::string_view& key,
         std::function<bool(const std::string_view& value, std::string& new_value)> cb) = 0;
  virtual std::size_t remove(const std::string_view& key) = 0;
  virtual void traverse(
      std::function<bool(const std::string_view& key, const std::string_view& value)> cb) const = 0;
  virtual std::unique_ptr<HeaderMap> create() const = 0;
  virtual std::unique_ptr<HeaderMap>
  create(const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers)
      const = 0;
};
} // namespace v1_0_x
} // namespace SrhinoPluginFramework