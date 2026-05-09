#pragma once

#include <fstream>
#include "envoy/srhino_plugin_framework/v1_0_x/libs/filesystem/plain_file.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace FileSystem {
class PlainFileImpl final : public PlainFile {
public:
  PlainFileImpl(const std::string& path);
  ~PlainFileImpl();

public:
  const std::string& path() const { return path_; }

public:
  size_t size() override;
  void seekg(size_t offset, SeekDir dir) override;
  void seekp(size_t offset, SeekDir dir) override;
  size_t getLine(char* buffer, size_t max_count, char delim = '\n') override;
  std::string getLine(size_t max_count, char delim = '\n') override;
  size_t read(char* buffer, size_t count) override;
  std::string read(size_t count) override;
  size_t write(std::string_view data) override;
  size_t write(const std::string& data) override;
  void flush() override { fs_.flush(); }
  bool isOpen() override { return fs_.is_open(); }
  bool good() override { return fs_.good(); }
  bool bad() override { return fs_.bad(); }
  bool fail() override { return fs_.fail(); }
  bool eof() override { return fs_.eof(); }
  void clear() override { return fs_.clear(); }

private:
  std::string path_;
  std::fstream fs_;
};
} // namespace FileSystem
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework