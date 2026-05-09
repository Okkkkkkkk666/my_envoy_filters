#pragma once

#include <cstdint>
#include <memory>

#include "envoy/srhino_plugin_framework/v1_4_x/context.h"

#include "envoy/srhino_plugin_framework/v1_4_x/proto/config/type/matcher/matcher.pb.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
class Matcher {
public:
  Matcher(
      const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher& /*matcher*/) {}
  virtual ~Matcher() = default;

public:
  /**
   * 路径匹配
   * @param context
   * @return true
   * @return false
   */
  virtual bool matchPath(const HeaderContext& context) const = 0;

  /**
   * HTTP头匹配
   * @param context
   * @return true
   * @return false
   */
  virtual bool matchHeader(const HeaderContext& context) const = 0;

  /**
   * 查询参数匹配
   * @param context
   * @return true
   * @return false
   */
  virtual bool matchQueryParameter(const HeaderContext& context) const = 0;

  /**
   * HTTP方法名匹配
   * @param context
   * @return true
   * @return false
   */
  virtual bool matchMethods(const HeaderContext& context) const = 0;

  /**
   * 用户名匹配
   * @param username
   * @return true
   * @return false
   */
  virtual bool matchUser(const std::string& username) const = 0;

  /**
   * Ip匹配
   * @param address
   * @return true
   * @return false
   */
  virtual bool matchIp(uint32_t address) const = 0;

  /**
   * IP归属地匹配
   * @param region_name
   * @return true
   * @return false
   */
  virtual bool matchIpRegion(const std::string& region_name) const = 0;

  /**
   * referer匹配
   * @param context
   * @return true
   * @return false
   */
  virtual bool matchReferer(const HeaderContext& context) const = 0;

  /**
   * 时间匹配
   * @param time
   * @return true
   * @return false
   */
  virtual bool matchTime() const = 0;

  /**
   * 路径、HTTP头、查询参数、HTTP方法名等同时匹配
   * @param context
   * @return true
   * @return false
   */
  virtual bool matchAll(const HeaderContext& context) const = 0;

  /**
   * 路径、HTTP头、查询参数、HTTP方法名、用户名、IP等同时匹配
   * @param context
   * @param username
   * @param address
   * @return true
   * @return false
   */
  virtual bool matchAll(const HeaderContext& context, uint32_t address,
                        const std::string& username) const = 0;

  /**
   * 路径、HTTP头、查询参数、HTTP方法名、用户名、IP、IP归属地、时间等同时匹配
   * @param context
   * @param username
   * @param address
   * @param region_name
   * @return true
   * @return false
   */
  virtual bool matchAll(const HeaderContext& context, uint32_t address, const std::string& username,
                        const std::string& region_name) const = 0;

  /**
   * 判断配置是否存在用户名
   * @return true
   * @return false
   */
  virtual bool getUserMatchers() const = 0;

  /**
   * 获取hash值
   * @return uint64_t
   */
  virtual uint64_t hash() const = 0;
};

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework