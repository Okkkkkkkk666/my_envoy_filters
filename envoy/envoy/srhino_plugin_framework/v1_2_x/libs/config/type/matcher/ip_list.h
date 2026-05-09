#pragma once

#include <cstdint>

#include "envoy/srhino_plugin_framework/v1_2_x/proto/config/type/matcher/matcher.pb.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {

class IpRange {
public:
  virtual ~IpRange() = default;

public:
  /**
   * 有效ip范围检测
   * @param address ip地址
   * @return true
   * @return false
   */
  virtual bool isInRange(uint32_t address) const = 0;
};

class IpSet {
public:
  virtual ~IpSet() = default;

public:
  /**
   * 判断一个ip地址是否在范围内
   * @param address 网络字节序IPV4地址
   * @return
   */
  virtual bool isInRange(uint32_t address) const = 0;
};
class IPGroups {
public:
  virtual ~IPGroups() = default;

public:
  /**
   * 判断一个ip地址是否在范围内
   * @param address 网络字节序IPV4地址
   * @return
   */
  virtual bool isInRange(uint32_t address) const = 0;
};
} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework