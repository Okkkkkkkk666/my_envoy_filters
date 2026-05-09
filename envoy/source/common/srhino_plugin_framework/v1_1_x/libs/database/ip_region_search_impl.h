#pragma once
#include <string>
#include <arpa/inet.h>
#include <sys/time.h>
#include <iostream>
#include "source/common/common/logger.h"
#include "envoy/srhino_plugin_framework/v1_1_x/libs/database/ip_region_search.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Database {

class IpRegionSearchImpl : public IpRegionSearch,
                           public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  IpRegionSearchImpl(const std::string& file_name);
  virtual ~IpRegionSearchImpl();

public:
  bool init_file() override;
  bool init_vector_index() override;
  bool init_content() override;

public:
  std::string search(const std::string& ip) override;
  std::string search(uint32_t ip_uint) override;
  
  std::string get_country(const std::string& ip) override;
  std::string get_province(const std::string& ip) override;
  std::string get_city(const std::string& ip) override;
private:
  void getContentIndex(uint32_t ip, uint32_t& left, uint32_t& right);
  void getContent(uint32_t index, uint32_t& ip_left, uint32_t& ip_right,
                  unsigned short& region_len, uint32_t& region_index);
  std::string getRegion(uint32_t index, unsigned short len);
  std::string doSearch(uint32_t ip_uint);

private:
  const std::string xdb_name_;
  FILE* db_;
  char* vector_index_;
  char* content_;
  static constexpr int header_length_ = 256;
  static constexpr int vector_index_rows_ = 256;
  static constexpr int vector_index_cols_ = 256;
  static constexpr int vector_index_size_ = 8;
  static constexpr int segment_index_size_ = 14;
  static constexpr int vector_index_length_ =
      vector_index_rows_ * vector_index_cols_ * vector_index_size_;
};

} // namespace Database
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework