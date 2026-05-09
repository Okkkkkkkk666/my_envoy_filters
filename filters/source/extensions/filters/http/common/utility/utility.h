#pragma once

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Utility {

#define PROCESS_DECODER_BUFFER_LIMIT(is_buffer_full)                                               \
  {                                                                                                \
    if (is_buffer_full) {                                                                          \
      return Http::FilterDataStatus::Continue;                                                     \
    }                                                                                              \
    uint64_t total_length = data.length();                                                         \
    if (decoder_callbacks_->decodingBuffer()) {                                                    \
      if (decoder_callbacks_->decodingBuffer() != &data) {                                         \
        total_length += decoder_callbacks_->decodingBuffer()->length();                            \
      }                                                                                            \
    }                                                                                              \
    if (total_length > decoder_callbacks_->decoderBufferLimit()) {                                 \
      is_buffer_full = true;                                                                       \
      decoder_callbacks_->setDecoderBufferLimit(decoder_callbacks_->decoderBufferLimit() +         \
                                                data.length());                                    \
      return Http::FilterDataStatus::Continue;                                                     \
    } else {                                                                                       \
      if (!end_stream) {                                                                           \
        return Http::FilterDataStatus::StopIterationAndBuffer;                                     \
      }                                                                                            \
      if (decoder_callbacks_->decodingBuffer()) {                                                  \
        decoder_callbacks_->addDecodedData(data, false);                                           \
      }                                                                                            \
    }                                                                                              \
  }

#define PROCESS_ENCODER_BUFFER_LIMIT(is_buffer_full)                                               \
  {                                                                                                \
    if (is_buffer_full) {                                                                          \
      return Http::FilterDataStatus::Continue;                                                     \
    }                                                                                              \
    uint64_t total_length = data.length();                                                         \
    if (encoder_callbacks_->encodingBuffer()) {                                                    \
      if (encoder_callbacks_->encodingBuffer() != &data) {                                         \
        total_length += encoder_callbacks_->encodingBuffer()->length();                            \
      }                                                                                            \
    }                                                                                              \
    if (total_length > encoder_callbacks_->encoderBufferLimit()) {                                 \
      is_buffer_full = true;                                                                       \
      encoder_callbacks_->setEncoderBufferLimit(encoder_callbacks_->encoderBufferLimit() +         \
                                                data.length());                                    \
      return Http::FilterDataStatus::Continue;                                                     \
    } else {                                                                                       \
      if (!end_stream) {                                                                           \
        return Http::FilterDataStatus::StopIterationAndBuffer;                                     \
      }                                                                                            \
      if (encoder_callbacks_->encodingBuffer()) {                                                  \
        encoder_callbacks_->addEncodedData(data, false);                                           \
      }                                                                                            \
    }                                                                                              \
  }
} // namespace Utility
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
