#pragma once

#include <cstdint>
#include <string>

#include <arpa/inet.h>

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Utility {
class NetAddress {
public:
  /**
   * 32位本机地址转网络地址
   * @param host_val 本机地址
   * @return std::uint32_t 网络地址
   */
  static std::uint32_t hton32(std::uint32_t host_val) { return htonl(host_val); }

  /**
   * 32位网络地址转本机地址
   * @param net_val 网络地址
   * @return std::uint32_t 本机地址
   */
  static std::uint32_t ntoh32(std::uint32_t net_val) { return ntohl(net_val); }

  /**
   * 16位本机地址转网络地址
   * @param host_val 本机地址
   * @return std::uint16_t 网络地址
   */
  static std::uint16_t hton16(std::uint16_t host_val) { return htons(host_val); }

  /**
   * 16位网络地址转本机地址
   * @param net_val 网络地址
   * @return std::uint16_t 本机地址
   */
  static std::uint16_t ntoh16(std::uint16_t net_val) { return ntohs(net_val); }

  /**
   * IPV4地址转点分十进制字符串
   * @param network_address 网络字节序IPV4地址
   * @return std::string 点分十进制字符串
   */
  static std::string toString(std::uint32_t network_address) {
    return inet_ntoa(*(reinterpret_cast<in_addr*>(&network_address)));
  }

  static std::uint32_t fromString(const std::string& address) {
    sockaddr_in addr;
    if (inet_aton(address.c_str(), &addr.sin_addr) > 0) {
      return addr.sin_addr.s_addr;
    }

    return 0;
  }
};
} // namespace Utility
} // namespace v1_4_x
} // namespace SrhinoPluginFramework