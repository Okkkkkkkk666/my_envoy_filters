
#include "ip_region_search_impl.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Database {

IpRegionSearchImpl::IpRegionSearchImpl(const std::string& file_name) : xdb_name_(file_name) {
  db_ = fopen(file_name.data(), "r");
  if (db_ == NULL) {
    ENVOY_LOG(error, "can't open {}", xdb_name_);
  } else {
    ENVOY_LOG(debug, "open db {} success", xdb_name_);
  }
  vector_index_ = nullptr;
  content_ = nullptr;
}

IpRegionSearchImpl::~IpRegionSearchImpl() {
  if (db_ != NULL) {
    fclose(db_);
    db_ = nullptr;
    ENVOY_LOG(debug, "close db {} success", xdb_name_);
  }
  if (vector_index_ != NULL) {
    free(vector_index_);
  }
  if (content_ != NULL) {
    free(content_);
  }
}

bool IpRegionSearchImpl::init_file() {
  if (!db_) {
    return false;
  }
  return true;
}

static void readBin(int index, char* buf, size_t len, FILE* db) {
  fseek(db, index, SEEK_SET);
  if (fread(buf, 1, len, db) != len) {
    throw std::runtime_error("Failed to read binary data");
  }
}

static uint32_t readUint(const char* buf) { return *reinterpret_cast<const uint32_t*>(buf); }

static unsigned short readUshort(const char* buf) {
  return *reinterpret_cast<const unsigned short*>(buf);
}

static bool ip2uint(const char* buf, uint32_t& ip) {
  struct in_addr addr;
  if (inet_pton(AF_INET, buf, &addr) == 0) {
    return false;
  }
  // 网络字节序为大端存储, 在此转换为小端存储
  ip = ntohl(addr.s_addr);
  return true;
}

bool IpRegionSearchImpl::init_vector_index() {
  if (!db_) {
    ENVOY_LOG(error, "db_ is null!");
    return false;
  }
  vector_index_ = static_cast<char*>(malloc(vector_index_length_));
  readBin(header_length_, vector_index_, vector_index_length_, db_);
  return true;
}

bool IpRegionSearchImpl::init_content() {
  if (!db_) {
    ENVOY_LOG(error, "db_ is null!");
    return false;
  }
  fseek(db_, 0, SEEK_END);
  uint32_t size = ftell(db_);
  content_ = static_cast<char*>(malloc(size));
  readBin(0, content_, size, db_);
  return true;
}

std::string IpRegionSearchImpl::search(const std::string& ip_str) {
  if (!db_) {
    ENVOY_LOG(error, "db_ is null!");
    return std::string();
  }

  uint32_t ip_uint;
  if (!ip2uint(ip_str.data(), ip_uint)) {
    return "invalid ip: " + ip_str;
  }
  return doSearch(ip_uint);
}

std::string IpRegionSearchImpl::search(uint32_t ip_uint) {
  if (!db_) {
    ENVOY_LOG(error, "db_ is null!");
    return std::string();
  }
  uint32_t ip_host_order = ntohl(ip_uint);
  return doSearch(ip_host_order);
}

std::string IpRegionSearchImpl::get_country(const std::string& ip) {
  if (!db_) {
    ENVOY_LOG(error, "db_ is null!");
    return std::string();
  }
  std::string location = search(ip);
  std::istringstream stream(location);
  std::string part;
  std::vector<std::string> parts;

  while (std::getline(stream, part, '-')) {
    parts.push_back(part);
  }

  if (parts.size() >= 1) {
    return parts[0];
  }
  return std::string();
}

std::string IpRegionSearchImpl::get_province(const std::string& ip) {
  if (!db_) {
    ENVOY_LOG(error, "db_ is null!");
    return std::string();
  }
  std::string location = search(ip);
  std::istringstream stream(location);
  std::string part;
  std::vector<std::string> parts;

  while (std::getline(stream, part, '-')) {
    parts.push_back(part);
  }

  if (parts.size() >= 2) {
    return parts[1];
  }
  return std::string();
}

std::string IpRegionSearchImpl::get_city(const std::string& ip) {
  if (!db_) {
    ENVOY_LOG(error, "db_ is null!");
    return std::string();
  }
  std::string location = search(ip);
  std::istringstream stream(location);
  std::string part;
  std::vector<std::string> parts;

  while (std::getline(stream, part, '-')) {
    parts.push_back(part);
  }

  if (parts.size() >= 3) {
    return parts[2];
  }
  return std::string();
}

void IpRegionSearchImpl::getContentIndex(uint32_t ip, uint32_t& left, uint32_t& right) {
  uint32_t ip_1 = (ip >> 24) & 0xFF;
  uint32_t ip_2 = (ip >> 16) & 0xFF;
  uint32_t index = (ip_1 * vector_index_cols_ + ip_2) * vector_index_size_;

  if (content_ != NULL) {
    left = readUint(content_ + index + header_length_);
    right = readUint(content_ + index + header_length_ + 4);
  } else if (vector_index_ != NULL) {
    left = readUint(vector_index_ + index);
    right = readUint(vector_index_ + index + 4);
  } else {
    char buf[8];
    readBin(header_length_ + index, buf, sizeof(buf), db_);
    left = readUint(buf);
    right = readUint(buf + 4);
  }
}

void IpRegionSearchImpl::getContent(uint32_t index, uint32_t& ip_left, uint32_t& ip_right,
                                    unsigned short& region_len, uint32_t& region_index) {
  char buf[segment_index_size_];
  const char* p;

  if (content_ != NULL) {
    p = content_ + index;
  } else {
    readBin(index, buf, sizeof(buf), db_);
    p = buf;
  }
  ip_left = readUint(p);
  ip_right = readUint(p + 4);
  region_len = readUshort(p + 8);
  region_index = readUint(p + 10);
}

std::string IpRegionSearchImpl::getRegion(uint32_t index, unsigned short len) {
  if (content_ != NULL) {
    return std::string(content_ + index, len);
  } else {
    char* buf = static_cast<char*>(malloc(sizeof(char) * len));
    readBin(index, buf, len, db_);
    std::string res(buf, len);
    free(buf);
    return res;
  }
}

static std::string formatSearchResult(const std::string& result) {
  std::vector<std::string> parts;
  std::istringstream stream(result);
  std::vector<std::string> valid_parts;
  std::string part;

  while (std::getline(stream, part, '|')) {
    parts.push_back(part);
  }

  for (const auto& p : parts) {
    if (p != "0") {
      valid_parts.push_back(p);
    }
  }

  return valid_parts.empty()
             ? std::string()
             : std::accumulate(valid_parts.begin(), valid_parts.end(), std::string(),
                               [](const std::string& current, const std::string& next) {
                                 return current.empty() ? next : current + "-" + next;
                               });
}

std::string IpRegionSearchImpl::doSearch(uint32_t ip_uint) {
  uint32_t content_index_left, content_index_right;
  getContentIndex(ip_uint, content_index_left, content_index_right);

  uint32_t left, right, mid;
  uint32_t ip_left, ip_right;
  unsigned short region_len;
  uint32_t region_index;
  uint32_t mid_index;

  left = 0;
  right = (content_index_right - content_index_left) / segment_index_size_;

  while (left <= right) {
    mid = left + (right - left) / 2;
    mid_index = content_index_left + mid * segment_index_size_;
    getContent(mid_index, ip_left, ip_right, region_len, region_index);

    if (ip_left > ip_uint) {
      right = mid - 1;
    } else if (ip_right < ip_uint) {
      left = mid + 1;
    } else {
      return formatSearchResult(getRegion(region_index, region_len));
    }
  }
  return std::string();
}

} // namespace Database
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework
