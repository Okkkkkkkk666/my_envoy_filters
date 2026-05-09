#pragma once

#include <stdint.h>

namespace SrhinoPluginFramework {
namespace v1_3_x {
/**
 * so导出函数调用结果
 */
extern "C" {
#pragma pack(1)
struct CallResult {
  // 返回的数据。当success为true时返回的是自定义数据，为false时则是错误描述字符串。
  unsigned char* data;
  // data的大小，单位字节
  uint32_t data_size;
  // 调用结果
  uint32_t success;
};
#pragma pack()
}
} // namespace v1_3_x
} // namespace SrhinoPluginFramework
