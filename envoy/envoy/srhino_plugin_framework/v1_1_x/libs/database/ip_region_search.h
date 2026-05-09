#pragma once

#include <memory>
#include <string>

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Database {

class IpRegionSearch {
public:
  IpRegionSearch() {}
  IpRegionSearch(const std::string&) = delete;
  virtual ~IpRegionSearch() = default;

public:
  /**
   * 完全基于文件查询初始化，具有最多的 I/O
   * 操作，这表明它每次操作都需要频繁从文件中读取数据。
   * @return 是否初始化成功
   */
  virtual bool init_file() = 0;

  /**
   * 基于缓存 `vector_index` 索引初始化，进行了一些 I/O
     操作，但由于它依赖于磁盘上的向量索引，造成了显著的性能下降。磁盘 I/O 操作显著增加了时间开销。
   * @return 是否初始化成功 
   */
  virtual bool init_vector_index() = 0;

  /**
   * 缓存整个 `xdb` 数据，没有进行任何 I/O
   * 操作，这意味着它完全依赖于内存中的数据，没有频繁的磁盘访问，因此速度快。
   * @return 是否初始化成功
   */
  virtual bool init_content() = 0;

  /**
   * 查询ip的归属地
   * @param ip_uint ip地址
   * @return 返回查询到的ip归属地 返回格式：国家-省份-城市-ISP
   */
  virtual std::string search(uint32_t ip_uint) = 0;

  /**
   * 查询ip的归属地
   * @param ip ip地址
   * @return 返回查询到的ip归属地 返回格式：国家-省份-城市-ISP
   */
  virtual std::string search(const std::string& ip) = 0;

  /**
   * 根据ip获取国家
   * @param ip ip地址
   * @return 返回查询到的ip所属的国家名称
   */
  virtual std::string get_country(const std::string& ip) = 0;

  /**
   * 根据ip获取省份
   * @param ip ip地址
   * @return 返回查询到的ip所属的省份
   */
  virtual std::string get_province(const std::string& ip) = 0;

  /**
   * 根据ip获取城市
   * @param ip ip地址
   * @return 返回查询到的ip所属的城市
   */
  virtual std::string get_city(const std::string& ip) = 0;
};
using IpRegionSearchSharedPtr = std::shared_ptr<IpRegionSearch>;
} // namespace Database
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework