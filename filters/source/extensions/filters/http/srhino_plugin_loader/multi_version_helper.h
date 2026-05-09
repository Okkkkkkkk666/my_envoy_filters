#pragma once

#include "source/common/srhino_plugin_framework/file_name_matcher.h"

#define DECLARE_FILTER_STATUS(major, minor, patch)                                                 \
  std::unique_ptr<v##major##_##minor##_x::FilterStatus> filter_status_v##major##_##minor##_x_;

#define MULTI_VERSION_BEGIN(framework_min_required)                                                \
  switch (SrhinoPluginFramework::FileNameMatcher::version(framework_min_required) & 0xFFFFC000) {

#define MULTI_VERSION_END()                                                                        \
  default:                                                                                         \
    break;                                                                                         \
    }

#define COMMENT(msg)

#define FILL_FILTER_STATUS_ENTRY(filter_status, major, minor, patch)                               \
  case (major << 28) | (minor << 14): {                                                            \
    filter_status.filter_status_v##major##_##minor##_x =                                           \
        std::make_unique<:v##major##_##minor##_x::FilterStatus>();                                 \
  } break;

#define ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, major, minor, patch)                 \
  case (major << 28) | (minor << 14): {                                                            \
    ENVOY_LOG(trace, "ON_HEADER_ENTRY: select version {}_{}_x.", major, minor);                    \
    if (!plugin_instance_) {                                                                       \
      return Http::FilterHeadersStatus::Continue;                                                  \
    }                                                                                              \
                                                                                                   \
    COMMENT(创建环境上下文)                                                                        \
    if (!filter_status.filter_status_v##major##_##minor##_x_) {                                    \
      ENVOY_LOG(trace, "ON_HEADER_ENTRY: init filter status.");                                    \
      filter_status.filter_status_v##major##_##minor##_x_ =                                        \
          std::make_unique<v##major##_##minor##_x::FilterStatus>();                                \
    }                                                                                              \
    if (!filter_status.filter_status_v##major##_##minor##_x_->header_context_) {                   \
      ENVOY_LOG(trace, "ON_HEADER_ENTRY: init header context.");                                   \
      filter_status.filter_status_v##major##_##minor##_x_->header_context_ =                       \
          std::make_unique<SrhinoPluginFramework::v##major##_##minor##_x::HeaderContextImpl>(      \
              filter_hcm_config_->filterInstanceName(), decoder_callbacks, encoder_callbacks,      \
              factory_context_, headers, end_stream);                                              \
    }                                                                                              \
                                                                                                   \
    SrhinoPluginFramework::v##major##_##minor##_x::HeaderStatus status;                            \
    if (decoder_callbacks) {                                                                       \
      ENVOY_LOG(trace, "ON_HEADER_ENTRY: call onRequestHeader.");                                  \
      status = reinterpret_cast<SrhinoPluginFramework::v##major##_##minor##_x::PluginInterface*>(  \
                   plugin_instance_)                                                               \
                   ->onRequestHeader(                                                              \
                       *filter_status.filter_status_v##major##_##minor##_x_->header_context_);     \
    } else if (encoder_callbacks) {                                                                \
      ENVOY_LOG(trace, "ON_HEADER_ENTRY: call onResponseHeader.");                                 \
      status = reinterpret_cast<SrhinoPluginFramework::v##major##_##minor##_x::PluginInterface*>(  \
                   plugin_instance_)                                                               \
                   ->onResponseHeader(                                                             \
                       *filter_status.filter_status_v##major##_##minor##_x_->header_context_);     \
    }                                                                                              \
                                                                                                   \
    if (status == SrhinoPluginFramework::v##major##_##minor##_x::HeaderStatus::Pause) {            \
      ENVOY_LOG(trace, "ON_HEADER_ENTRY: set pause.");                                             \
      filter_status.filter_status_v##major##_##minor##_x_->header_context_->setPause(true);        \
    } else if (end_stream &&                                                                       \
               (status == SrhinoPluginFramework::v##major##_##minor##_x::HeaderStatus::Break)) {   \
      ENVOY_LOG(trace, "ON_HEADER_ENTRY: set break but body is empty.");                           \
      return Http::FilterHeadersStatus::Continue;                                                  \
    }                                                                                              \
    filter_status.filter_status_v##major##_##minor##_x_->header_status_ = status;                  \
                                                                                                   \
    COMMENT(处理本地回复)                                                                          \
    if (decoder_callbacks) {                                                                       \
      ENVOY_LOG(trace, "ON_HEADER_ENTRY: decoder maybeSendLocalReply.");                           \
      maybeSendLocalReply(status,                                                                  \
                          filter_status.filter_status_v##major##_##minor##_x_->header_context_,    \
                          decoder_callbacks);                                                      \
    } else if (encoder_callbacks) {                                                                \
      ENVOY_LOG(trace, "ON_HEADER_ENTRY: encoder maybeSendLocalReply.");                           \
      maybeSendLocalReply(status,                                                                  \
                          filter_status.filter_status_v##major##_##minor##_x_->header_context_,    \
                          encoder_callbacks);                                                      \
    }                                                                                              \
                                                                                                   \
    return v##major##_##minor##_x::StatusConverter::convert(status);                               \
  } break;

#define ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, major, minor, patch)                   \
  case (major << 28) | (minor << 14): {                                                            \
    if (!plugin_instance_) {                                                                       \
      return Http::FilterDataStatus::Continue;                                                     \
    }                                                                                              \
                                                                                                   \
    COMMENT(创建环境上下文)                                                                        \
    if (!filter_status.filter_status_v##major##_##minor##_x_->body_context_) {                     \
      filter_status.filter_status_v##major##_##minor##_x_->body_context_ =                         \
          std::make_unique<SrhinoPluginFramework::v##major##_##minor##_x::BodyContextImpl>(        \
              filter_hcm_config_->filterInstanceName(), decoder_callbacks, encoder_callbacks,      \
              factory_context_, end_stream);                                                       \
    } else {                                                                                       \
      filter_status.filter_status_v##major##_##minor##_x_->body_context_->setEndStream(            \
          end_stream);                                                                             \
    }                                                                                              \
                                                                                                   \
    COMMENT(处理Break)                                                                             \
    if (filter_status.filter_status_v##major##_##minor##_x_->header_status_.has_value() &&         \
        filter_status.filter_status_v##major##_##minor##_x_->header_status_.value() ==             \
            SrhinoPluginFramework::v##major##_##minor##_x::HeaderStatus::Break) {                  \
      if (filter_status.filter_status_v##major##_##minor##_x_->header_context_->isBreakBody()) {   \
        return Http::FilterDataStatus::StopIterationNoBuffer;                                      \
      }                                                                                            \
    }                                                                                              \
                                                                                                   \
    COMMENT(处理WaitForBody及TryWaitForBody)                                                       \
    Buffer::Instance* body = &data;                                                                \
    if (filter_status.filter_status_v##major##_##minor##_x_->data_status_.has_value() &&           \
        (filter_status.filter_status_v##major##_##minor##_x_->data_status_.value() ==              \
             SrhinoPluginFramework::v##major##_##minor##_x::DataStatus::WaitForBody ||             \
         filter_status.filter_status_v##major##_##minor##_x_->data_status_.value() ==              \
             SrhinoPluginFramework::v##major##_##minor##_x::DataStatus::TryWaitForBody)) {         \
      if (is_buffer_full) {                                                                        \
        return Http::FilterDataStatus::Continue;                                                   \
      }                                                                                            \
                                                                                                   \
      uint64_t total_length = data.length();                                                       \
      if (decoder_callbacks) {                                                                     \
        ASSERT(decoder_callbacks->decodingBuffer(), "the decoding buffer must not be null when "   \
                                                    "already called WaitForBody/TryWaitForBody");  \
        if (decoder_callbacks->decodingBuffer() && decoder_callbacks->decodingBuffer() != &data) { \
          total_length += decoder_callbacks->decodingBuffer()->length();                           \
        }                                                                                          \
        if (total_length >= filter_status.filter_status_v##major##_##minor##_x_->body_context_     \
                                ->waitForBodyBufferLimit()) {                                      \
          is_buffer_full = true;                                                                   \
          if (total_length > decoder_callbacks->decoderBufferLimit()) {                            \
            decoder_callbacks->setDecoderBufferLimit(decoder_callbacks->decoderBufferLimit() +     \
                                                     data.length());                               \
          }                                                                                        \
          if (filter_status.filter_status_v##major##_##minor##_x_->data_status_.value() ==         \
              SrhinoPluginFramework::v##major##_##minor##_x::DataStatus::WaitForBody) {            \
            return Http::FilterDataStatus::Continue;                                               \
          }                                                                                        \
        } else {                                                                                   \
          if (!end_stream) {                                                                       \
            return Http::FilterDataStatus::StopIterationAndBuffer;                                 \
          }                                                                                        \
        }                                                                                          \
                                                                                                   \
        decoder_callbacks->addDecodedData(data, false);                                            \
        body = const_cast<Buffer::Instance*>(decoder_callbacks->decodingBuffer());                 \
      } else if (encoder_callbacks) {                                                              \
        ASSERT(encoder_callbacks->encodingBuffer(), "the encoding buffer must not be null when "   \
                                                    "already called WaitForBody/TryWaitForBody");  \
        if (encoder_callbacks->encodingBuffer() && encoder_callbacks->encodingBuffer() != &data) { \
          total_length += encoder_callbacks->encodingBuffer()->length();                           \
        }                                                                                          \
        if (total_length >= filter_status.filter_status_v##major##_##minor##_x_->body_context_     \
                                ->waitForBodyBufferLimit()) {                                      \
          is_buffer_full = true;                                                                   \
          if (total_length > encoder_callbacks->encoderBufferLimit()) {                            \
            encoder_callbacks->setEncoderBufferLimit(encoder_callbacks->encoderBufferLimit() +     \
                                                     data.length());                               \
          }                                                                                        \
          if (filter_status.filter_status_v##major##_##minor##_x_->data_status_.value() ==         \
              SrhinoPluginFramework::v##major##_##minor##_x::DataStatus::WaitForBody) {            \
            return Http::FilterDataStatus::Continue;                                               \
          }                                                                                        \
        } else {                                                                                   \
          if (!end_stream) {                                                                       \
            return Http::FilterDataStatus::StopIterationAndBuffer;                                 \
          }                                                                                        \
        }                                                                                          \
                                                                                                   \
        encoder_callbacks->addEncodedData(data, false);                                            \
        body = const_cast<Buffer::Instance*>(encoder_callbacks->encodingBuffer());                 \
      }                                                                                            \
    }                                                                                              \
                                                                                                   \
    filter_status.filter_status_v##major##_##minor##_x_->body_context_->setData(*body);            \
    SrhinoPluginFramework::v##major##_##minor##_x::DataStatus status;                              \
    if (decoder_callbacks) {                                                                       \
      status = reinterpret_cast<SrhinoPluginFramework::v##major##_##minor##_x::PluginInterface*>(  \
                   plugin_instance_)                                                               \
                   ->onRequestBody(                                                                \
                       *filter_status.filter_status_v##major##_##minor##_x_->body_context_);       \
    } else if (encoder_callbacks) {                                                                \
      status = reinterpret_cast<SrhinoPluginFramework::v##major##_##minor##_x::PluginInterface*>(  \
                   plugin_instance_)                                                               \
                   ->onResponseBody(                                                               \
                       *filter_status.filter_status_v##major##_##minor##_x_->body_context_);       \
    }                                                                                              \
                                                                                                   \
    v##major##_##minor##_x::StatusConverter::adjustDataStatus(                                     \
        *filter_status.filter_status_v##major##_##minor##_x_, status, end_stream);                 \
    if (status == SrhinoPluginFramework::v##major##_##minor##_x::DataStatus::Pause) {              \
      filter_status.filter_status_v##major##_##minor##_x_->body_context_->setPause(true);          \
    }                                                                                              \
    filter_status.filter_status_v##major##_##minor##_x_->data_status_ = status;                    \
                                                                                                   \
    COMMENT(处理buffer大小防止413及500)                                                            \
    if (status == SrhinoPluginFramework::v##major##_##minor##_x::DataStatus::WaitForBody ||        \
        status == SrhinoPluginFramework::v##major##_##minor##_x::DataStatus::TryWaitForBody) {     \
      if (decoder_callbacks) {                                                                     \
        adjustBufferLimit(decoder_callbacks, data);                                                \
      } else {                                                                                     \
        adjustBufferLimit(encoder_callbacks, data);                                                \
      }                                                                                            \
    }                                                                                              \
                                                                                                   \
    COMMENT(处理本地回复)                                                                          \
    if (decoder_callbacks) {                                                                       \
      maybeSendLocalReply(status,                                                                  \
                          filter_status.filter_status_v##major##_##minor##_x_->body_context_,      \
                          decoder_callbacks);                                                      \
    } else if (encoder_callbacks) {                                                                \
      maybeSendLocalReply(status,                                                                  \
                          filter_status.filter_status_v##major##_##minor##_x_->body_context_,      \
                          encoder_callbacks);                                                      \
    }                                                                                              \
                                                                                                   \
    return v##major##_##minor##_x::StatusConverter::convert(status);                               \
  } break;

#define ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, major, minor, patch)                \
  case (major << 28) | (minor << 14): {                                                            \
    if (!plugin_instance_) {                                                                       \
      return Http::FilterTrailersStatus::Continue;                                                 \
    }                                                                                              \
                                                                                                   \
    COMMENT(创建环境上下文)                                                                        \
    if (!filter_status.filter_status_v##major##_##minor##_x_->trailer_context_) {                  \
      filter_status.filter_status_v##major##_##minor##_x_->trailer_context_ =                      \
          std::make_unique<SrhinoPluginFramework::v##major##_##minor##_x::TrailersContextImpl>(    \
              filter_hcm_config_->filterInstanceName(), decoder_callbacks, encoder_callbacks,      \
              factory_context_, trailers);                                                         \
    }                                                                                              \
                                                                                                   \
    COMMENT(处理Break)                                                                             \
    if (filter_status.filter_status_v##major##_##minor##_x_->header_status_.has_value() &&         \
        filter_status.filter_status_v##major##_##minor##_x_->header_status_.value() ==             \
            SrhinoPluginFramework::v##major##_##minor##_x::HeaderStatus::Break) {                  \
      if (filter_status.filter_status_v##major##_##minor##_x_->header_context_->isBreakBody()) {   \
        return Http::FilterTrailersStatus::StopIteration;                                          \
      }                                                                                            \
    }                                                                                              \
    if (filter_status.filter_status_v##major##_##minor##_x_->data_status_.has_value() &&           \
        filter_status.filter_status_v##major##_##minor##_x_->data_status_.value() ==               \
            SrhinoPluginFramework::v##major##_##minor##_x::DataStatus::Break) {                    \
      return Http::FilterTrailersStatus::StopIteration;                                            \
    }                                                                                              \
                                                                                                   \
    SrhinoPluginFramework::v##major##_##minor##_x::TrailerStatus status;                           \
    if (decoder_callbacks) {                                                                       \
      status = reinterpret_cast<SrhinoPluginFramework::v##major##_##minor##_x::PluginInterface*>(  \
                   plugin_instance_)                                                               \
                   ->onRequestTrailers(                                                            \
                       *filter_status.filter_status_v##major##_##minor##_x_->trailer_context_);    \
    } else if (encoder_callbacks) {                                                                \
      status = reinterpret_cast<SrhinoPluginFramework::v##major##_##minor##_x::PluginInterface*>(  \
                   plugin_instance_)                                                               \
                   ->onResponseTrailers(                                                           \
                       *filter_status.filter_status_v##major##_##minor##_x_->trailer_context_);    \
    }                                                                                              \
    filter_status.filter_status_v##major##_##minor##_x_->trailer_status_ = status;                 \
                                                                                                   \
    COMMENT(处理本地回复)                                                                          \
    if (decoder_callbacks) {                                                                       \
      maybeSendLocalReply(status,                                                                  \
                          filter_status.filter_status_v##major##_##minor##_x_->trailer_context_,   \
                          decoder_callbacks);                                                      \
    } else if (encoder_callbacks) {                                                                \
      maybeSendLocalReply(status,                                                                  \
                          filter_status.filter_status_v##major##_##minor##_x_->trailer_context_,   \
                          encoder_callbacks);                                                      \
    }                                                                                              \
                                                                                                   \
    return v##major##_##minor##_x::StatusConverter::convert(status);                               \
  } break;

#define ON_STREAM_COMPLETE_ENTRY(major, minor, patch)                                              \
  case (major << 28) | (minor << 14): {                                                            \
    if (!plugin_instance_) {                                                                       \
      return;                                                                                      \
    }                                                                                              \
                                                                                                   \
    std::string msg;                                                                               \
    reinterpret_cast<SrhinoPluginFramework::v##major##_##minor##_x::PluginInterface*>(             \
        plugin_instance_)                                                                          \
        ->onCommitLog(msg);                                                                        \
    if (!msg.empty()) {                                                                            \
      log(msg);                                                                                    \
    }                                                                                              \
  } break;

#define FILL_FILTER_STATUS(framework_min_required, filter_status, major, minor, patch)             \
  MULTI_VERSION_BEGIN(framework_min_required)                                                      \
  FILL_FILTER_STATUS_ENTRY(filter_status, major, minor, patch)                                     \
  MULTI_VERSION_END()

/**
 * step 1: 使用tools/srhino_plugin_framework_generator.sh脚本快速生成对应版本的源码
 */

/**
 * step 2: filters/source/extensions/filters/http/srhino_plugin_loader/BUILD 中添加新增版本的依赖
 */

/**
 * step 3: 包含对应版本头文件
 *
 * E.g.
 * #include "v1_0_x/filter_status.h"
 * #include "v1_0_x/status_converter.h"
 * #include "v1_1_x/filter_status.h"
 * #include "v1_1_x/status_converter.h"
 * #include "v1_2_x/filter_status.h"
 * #include "v1_2_x/status_converter.h"
 * #include "v1_3_x/filter_status.h"
 * #include "v1_3_x/status_converter.h"
 * #include "v1_4_x/filter_status.h"
 * #include "v1_4_x/status_converter.h"
 */
#include "v1_0_x/filter_status.h"
#include "v1_1_x/filter_status.h"
#include "v1_2_x/filter_status.h"
#include "v1_3_x/filter_status.h"
#include "v1_4_x/filter_status.h"
#include "v1_0_x/status_converter.h"
#include "v1_1_x/status_converter.h"
#include "v1_2_x/status_converter.h"
#include "v1_3_x/status_converter.h"
#include "v1_4_x/status_converter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SrhinoPluginLoaderFilter {
/**
 * step 4: 使用DECLARE_FILTER_STATUS宏来声明不同版本的插件状态
 *
 * E.g.
 * DECLARE_FILTER_STATUS(1, 0, x);    \
 * DECLARE_FILTER_STATUS(1, 1, x);    \
 * DECLARE_FILTER_STATUS(1, 2, x);    \
 * DECLARE_FILTER_STATUS(1, 3, x);    \
 * DECLARE_FILTER_STATUS(1, 4, x);    \
 */
struct MultiVersionFilterStatus {
  bool is_cotinued_{false};
  DECLARE_FILTER_STATUS(1, 0, x);
  DECLARE_FILTER_STATUS(1, 1, x);
  DECLARE_FILTER_STATUS(1, 2, x);
  DECLARE_FILTER_STATUS(1, 3, x);
  DECLARE_FILTER_STATUS(1, 4, x);
};

/**
 * step 5: 使用ON_HEADER_ENTRY宏来处理对应版本的header事件
 *
 * E.g.
 * ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 0, x)     \
 * ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 1, x)     \
 * ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 2, x)     \
 * ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 3, x)     \
 * ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 4, x)     \
 */
#define ON_HEADER(framework_min_required, decoder_callbacks, encoder_callbacks)                     \
  MULTI_VERSION_BEGIN(framework_min_required)                                                       \
  ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 0, x)                                   \
  ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 1, x)                                   \
  ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 2, x)                                   \
  ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 3, x)                                   \
  ON_HEADER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 4, x)                                   \
  MULTI_VERSION_END()

/**
 * step 6: 使用ON_DATA_ENTRY宏来处理对应版本的data事件
 *
 * E.g.
 * ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 0, x)     \
 * ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 1, x)     \
 * ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 2, x)     \
 * ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 3, x)     \
 * ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 4, x)     \
 */
#define ON_DATA(framework_min_required, decoder_callbacks, encoder_callbacks)                       \
  MULTI_VERSION_BEGIN(framework_min_required)                                                       \
  ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 0, x)                                     \
  ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 1, x)                                     \
  ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 2, x)                                     \
  ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 3, x)                                     \
  ON_DATA_ENTRY(decoder_callbacks, encoder_callbacks, 1, 4, x)                                     \
  MULTI_VERSION_END()

/**
 * step 7: 使用ON_TRAILER宏来处理对应版本的trailer事件
 *
 * E.g.
 * ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 0, x)     \
 * ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 1, x)     \
 * ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 2, x)     \
 * ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 3, x)     \
 * ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 4, x)     \
 */
#define ON_TRAILER(framework_min_required, decoder_callbacks, encoder_callbacks)                    \
  MULTI_VERSION_BEGIN(framework_min_required)                                                       \
  ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 0, x)                                  \
  ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 1, x)                                  \
  ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 2, x)                                  \
  ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 3, x)                                  \
  ON_TRAILER_ENTRY(decoder_callbacks, encoder_callbacks, 1, 4, x)                                  \
  MULTI_VERSION_END()

/**
 * step 8: 使用ON_STREAM_COMPLETE_ENTRY宏来处理对应版本的complete事件
 *
 * E.g.
 * ON_STREAM_COMPLETE_ENTRY(1, 0, x)     \
 * ON_STREAM_COMPLETE_ENTRY(1, 1, x)     \
 * ON_STREAM_COMPLETE_ENTRY(1, 2, x)     \
 * ON_STREAM_COMPLETE_ENTRY(1, 3, x)     \
 * ON_STREAM_COMPLETE_ENTRY(1, 4, x)     \
 */
#define ON_STREAM_COMPLETE(framework_min_required)                                                  \
  MULTI_VERSION_BEGIN(framework_min_required)                                                       \
  ON_STREAM_COMPLETE_ENTRY(1, 0, x)                                                                \
  ON_STREAM_COMPLETE_ENTRY(1, 1, x)                                                                \
  ON_STREAM_COMPLETE_ENTRY(1, 2, x)                                                                \
  ON_STREAM_COMPLETE_ENTRY(1, 3, x)                                                                \
  ON_STREAM_COMPLETE_ENTRY(1, 4, x)                                                                \
  MULTI_VERSION_END()

} // namespace SrhinoPluginLoaderFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy