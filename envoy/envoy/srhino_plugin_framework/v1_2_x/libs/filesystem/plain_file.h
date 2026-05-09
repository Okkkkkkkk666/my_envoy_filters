#pragma once

#include <cstddef>
#include <string_view>
#include <memory>

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace FileSystem {
class PlainFile {
protected:
  virtual ~PlainFile() = default;

public:
  enum SeekDir {
    // 相对于文件开始
    beg = std::ios_base::beg,
    // 相对于当前位置
    cur = std::ios_base::cur,
    // 相对于文件末尾
    end = std::ios_base::end,
  };

public:
  /**
   * 获取文件大小
   * @return 文件大小，单位字节
   */
  virtual size_t size() = 0;

  /**
   * 改变当前文件读的位置
   * @param offset 文件偏移
   * @param dir 指定文件偏移的起始位置
   */
  virtual void seekg(size_t offset, SeekDir dir) = 0;

  /**
   * 改变当前文件写的位置
   * @param offset 文件偏移
   * @param dir 指定文件偏移的起始位置
   */
  virtual void seekp(size_t offset, SeekDir dir) = 0;

  /**
   * 获取一行数据
   * @param buffer 接收数据的缓冲区
   * @param max_count 获取数据的最大长度，单位字节
   * @param delim 行结束标识符，默认为'\n'
   * @return 实际获取的数据长度
   */
  virtual size_t getLine(char* buffer, size_t max_count, char delim = '\n') = 0;

  /**
   * 获取一行数据
   * @param max_count 获取数据的最大长度，单位字节
   * @param delim 行结束标识符，默认为'\n'
   * @return 获取的数据
   */
  virtual std::string getLine(size_t max_count, char delim = '\n') = 0;

  /**
   * 读取指定长度的数据
   * @param buffer 接收数据的缓冲区
   * @param count 读取数据的长度，单位字节
   * @return 实际读取的数据长度
   */
  virtual size_t read(char* buffer, size_t count) = 0;

  /**
   * 读取指定长度的数据
   * @param count 读取数据的长度，单位字节
   * @return 读取的数据
   */
  virtual std::string read(size_t count) = 0;

  /**
   * 写入指定长度的数据
   * @param data 写入的数据
   * @return 实际写入的数据长度
   */
  virtual size_t write(std::string_view data) = 0;

  /**
   * 写入指定长度的数据
   * @param data 写入的数据
   * @return 实际写入的数据长度
   */
  virtual size_t write(const std::string& data) = 0;

  /**
   * 立即刷新文件的写缓冲区
   */
  virtual void flush() = 0;

  /**
   * 检查文件是否被打开
   * @return 文件被打开返回true
   */
  virtual bool isOpen() = 0;

  /**
   * 检查文件状态是否正常
   * @return
   */
  virtual bool good() = 0;

  /**
   * 检查文件badbit
   * @return
   */
  virtual bool bad() = 0;

  /**
   * 检查文件failbit
   * @return
   */
  virtual bool fail() = 0;

  /**
   * 检查文件eofbit
   * @return
   */
  virtual bool eof() = 0;

  /**
   * 重置文件所有标志位
   * @return
   */
  virtual void clear() = 0;
};

using PlainFileSharedPtr = std::shared_ptr<PlainFile>;
} // namespace FileSystem
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework