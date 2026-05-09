#include "source/common/srhino_plugin_framework/v1_1_x/libs/filesystem/plain_file_impl.h"
#include "envoy/srhino_plugin_framework/v1_1_x/utility/empty_string.hpp"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace FileSystem {
PlainFileImpl::PlainFileImpl(const std::string& path) : path_(path) {
  fs_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
}

PlainFileImpl::~PlainFileImpl() {
  if (fs_.is_open()) {
    fs_.close();
  }
}

size_t PlainFileImpl::size() {
  if (!fs_.is_open() || !fs_.good()) {
    return 0;
  }

  auto old_pos = fs_.tellg();
  fs_.seekg(0, fs_.end);
  auto pos = fs_.tellg();
  fs_.seekg(old_pos, fs_.beg);

  return pos == -1 ? 0 : static_cast<size_t>(pos);
}

void PlainFileImpl::seekg(size_t offset, SeekDir dir) {
  if (!fs_.is_open() || !fs_.good()) {
    return;
  }

  fs_.seekg(offset, static_cast<std::ios::seekdir>(dir));
}

void PlainFileImpl::seekp(size_t offset, SeekDir dir) {
  if (!fs_.is_open() || !fs_.good()) {
    return;
  }

  fs_.seekp(offset, static_cast<std::ios::seekdir>(dir));
}

size_t PlainFileImpl::getLine(char* buffer, size_t max_count, char delim) {
  if (!fs_.is_open() || !fs_.good()) {
    return 0;
  }

  fs_.getline(buffer, max_count, fs_.widen(delim));
  return fs_.gcount();
}

std::string PlainFileImpl::getLine(size_t max_count, char delim) {
  if (!fs_.is_open() || !fs_.good()) {
    return Utility::EMPTY_STRING;
  }

  std::string buffer;
  buffer.resize(max_count);
  fs_.getline(buffer.data(), max_count, fs_.widen(delim));
  buffer.resize(fs_.gcount());

  return buffer;
}

size_t PlainFileImpl::read(char* buffer, size_t count) {
  if (!fs_.is_open() || !fs_.good()) {
    return 0;
  }

  fs_.read(buffer, count);
  return fs_.gcount();
}

std::string PlainFileImpl::read(size_t count) {
  if (!fs_.is_open() || !fs_.good()) {
    return Utility::EMPTY_STRING;
  }

  std::string buffer;
  buffer.resize(count);
  fs_.read(buffer.data(), count);
  buffer.resize(fs_.gcount());

  return buffer;
}

size_t PlainFileImpl::write(std::string_view data) {
  if (!fs_.is_open() || !fs_.good() || data.length()) {
    return 0;
  }

  fs_.write(data.data(), data.length());
  return fs_.gcount();
}

size_t PlainFileImpl::write(const std::string& data) {
  if (!fs_.is_open() || !fs_.good() || data.length()) {
    return 0;
  }

  fs_.write(data.data(), data.length());
  return fs_.gcount();
}

} // namespace FileSystem
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework