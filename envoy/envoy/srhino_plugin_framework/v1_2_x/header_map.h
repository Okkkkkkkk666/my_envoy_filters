#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace SrhinoPluginFramework {
namespace v1_2_x {
class HeaderMap {
public:
  virtual ~HeaderMap() = default;

public:
  virtual std::string_view get(const std::string_view& key) const = 0;
  virtual void get(const std::string_view& key, std::vector<std::string_view>& values) const = 0;
  virtual void add(const std::string_view& key, const std::string_view& value) = 0;
  virtual void add(const std::string_view& key, uint64_t value) = 0;
  virtual void set(const std::string_view& key, const std::string_view& value) = 0;
  /**
   * 以引用方式添加头部，调用者要保证在headerMap被销毁前，value是有效的
   */
  virtual void addReference(const std::string_view& key, const std::string_view& value) = 0;
  /**
   * 以引用方式设置头部，调用者要保证在headerMap被销毁前，value是有效的
   */
  virtual void setReference(const std::string_view& key, const std::string_view& value) = 0;

  /**
   * 清空headerMap
   */
  virtual void clear() const = 0;
  /**
   * @return true headerMap为空
   *         false headerMap不为空
   */
  virtual bool empty() const = 0;
  virtual std::string_view path() const = 0;
  virtual std::string_view authority() const = 0;
  virtual std::string_view protocol() const = 0;
  virtual std::string_view method() const = 0;
  virtual std::string_view host() const = 0;
  virtual uint32_t statusCode() const = 0;
  virtual std::string_view userAgent() const = 0;
  virtual std::string_view forwardedFor() const = 0;
  virtual std::string_view contentEncoding() const = 0;
  virtual std::string_view contentType() const = 0;
  /**
   * @return 对象本身所占的内存大小。
   */
  virtual uint64_t byteSize() const = 0;
  /**
   * @return header中键值对的数量
   */
  virtual size_t size() const = 0;
  /**
   * 删除所有以key为前缀的所有条目
   * 可以用于一次性删除自定义的带有特定前缀的header
   * @param prefix header prefix
   * @return 删除的header数量
   */
  virtual size_t removePrefix(const std::string_view& prefix) const = 0;

  /**
   * 解析 Cookie 字段，并查找指定的键
   * @param cookie_key 查找的 cookie 键
   * @param onGetCookieValue 当找到指定 cookie 键时调用的回调函数
   *      onGetCookieValue的返回值： true表示继续查找下一个，false表示结束查找。
   */
  virtual void
  parseCookieValue(const std::string_view& cookie_key,
                   const std::function<bool(const std::string_view)>& onGetCookieValue) const = 0;

  /**
   * 解析 Set-Cookie 字段，并查找指定的键
   * @param cookie_key 查找的 set-cookie 键
   * @param onGetCookieValue 当找到指定 set-cookie 键时调用的回调函数
   *      onGetCookieValue的返回值： true表示继续查找下一个，false表示结束查找。
   */
  virtual void parseSetCookieValue(
      const std::string_view& cookie_key,
      const std::function<bool(const std::string_view)>& onGetCookieValue) const = 0;

  /**
   * 修改http header value
   * @param key header key
   * @param cb 修改回调函数，value是修改前的值，new_value是修改后的值。
   *        当cb返回true，modify将当前header value设置为new_value；返回false，则什么也不做。
   */
  virtual void
  modify(const std::string_view& key,
         std::function<bool(const std::string_view& value, std::string& new_value)> cb) = 0;
  virtual std::size_t remove(const std::string_view& key) = 0;
  virtual void traverse(
      std::function<bool(const std::string_view& key, const std::string_view& value)> cb) const = 0;
  virtual std::unique_ptr<HeaderMap> create() const = 0;
  virtual std::unique_ptr<HeaderMap>
  create(const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers)
      const = 0;
};
} // namespace v1_2_x
} // namespace SrhinoPluginFramework