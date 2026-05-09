#include "source/common/srhino_plugin_framework/v1_0_x/context_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/timer_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
void ContextImpl::setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                                    std::string&& body) {
  direct_response_.code_ = code;
  direct_response_.headers_ = std::move(headers);
  direct_response_.body_ = std::move(body);
}

std::unordered_map<std::string, std::string>& ContextImpl::sharedData() {
  static const absl::string_view data_name("shared_data");

  if (!stream_callbacks_->streamInfo().filterState()->hasData<Envoy::StreamInfo::UserData>(data_name)) {
    stream_callbacks_->streamInfo().filterState()->setData(
        data_name, std::make_shared<Envoy::StreamInfo::UserData>(),
        Envoy::StreamInfo::FilterState::StateType::Mutable);
  }

  return stream_callbacks_->streamInfo().filterState()->getDataMutable<Envoy::StreamInfo::UserData>(data_name);
}

TimerSharedPtr ContextImpl::createTimer(std::chrono::milliseconds interval,
                                        std::function<bool(Timer& timer)> func,
                                        bool run_in_main_thread) {
  Envoy::Event::Dispatcher& dispatcher = run_in_main_thread
                                             ? factory_context_.mainThreadDispatcher()
                                             : stream_callbacks_->dispatcher();

  return std::make_shared<TimerImpl>(dispatcher, interval, func);
}

void ContextImpl::overrideDestHost(const std::string_view& host) {
  if (decoder_callbacks_) {
    decoder_callbacks_->setUpstreamOverrideHost(absl::string_view(host.data(), host.length()));
  }
}

void ContextImpl::setWaitForBodyBufferLimit(uint32_t size) { buffer_pool_.limit(size); }

uint32_t ContextImpl::waitForBodyBufferLimit() const { return buffer_pool_.limit(); }

void HeaderContextImpl::setDirectResponse(Utility::HttpCode code,
                                          std::unique_ptr<HeaderMap>&& headers,
                                          std::string&& body) {
  // 允许在返回Pause后，相关的回调中进行直接回复
  if (is_pause_) {
    if (decoder_callbacks_) {
      decoder_callbacks_->sendLocalReply(
          static_cast<Envoy::Http::Code>(code), {body.c_str(), body.size()},
          [&](Envoy::Http::HeaderMap& head_map) {
            headers->traverse([&](const std::string_view& key, const std::string_view& value) {
              Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
              head_map.setCopy(lower_key, {value.data(), value.size()});
              return true;
            });
          },
          absl::nullopt, absl::string_view());
    } else if (encoder_callbacks_) {
      encoder_callbacks_->sendLocalReply(
          static_cast<Envoy::Http::Code>(code), {body.c_str(), body.size()},
          [&](Envoy::Http::HeaderMap& head_map) {
            headers->traverse([&](const std::string_view& key, const std::string_view& value) {
              Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
              head_map.setCopy(lower_key, {value.data(), value.size()});
              return true;
            });
          },
          absl::nullopt, absl::string_view());
    }
  } else {
    ContextImpl::setDirectResponse(code, std::move(headers), std::move(body));
  }
}

void BodyContextImpl::setDirectResponse(Utility::HttpCode code,
                                        std::unique_ptr<HeaderMap>&& headers, std::string&& body) {
  // 允许在返回Pause后，相关的回调中进行直接回复
  if (is_pause_) {
    if (decoder_callbacks_) {
      decoder_callbacks_->sendLocalReply(
          static_cast<Envoy::Http::Code>(code), {body.c_str(), body.size()},
          [&](Envoy::Http::HeaderMap& head_map) {
            headers->traverse([&](const std::string_view& key, const std::string_view& value) {
              Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
              head_map.setCopy(lower_key, {value.data(), value.size()});
              return true;
            });
          },
          absl::nullopt, absl::string_view());
    } else if (encoder_callbacks_) {
      encoder_callbacks_->sendLocalReply(
          static_cast<Envoy::Http::Code>(code), {body.c_str(), body.size()},
          [&](Envoy::Http::HeaderMap& head_map) {
            headers->traverse([&](const std::string_view& key, const std::string_view& value) {
              Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
              head_map.setCopy(lower_key, {value.data(), value.size()});
              return true;
            });
          },
          absl::nullopt, absl::string_view());
    }
  } else {
    ContextImpl::setDirectResponse(code, std::move(headers), std::move(body));
  }
}

void HeaderContextImpl::continued() const {
  if (is_pause_) {
    if (decoder_callbacks_) {
      decoder_callbacks_->continueDecoding();
    } else if (encoder_callbacks_) {
      encoder_callbacks_->continueEncoding();
    }
  }
}

void BodyContextImpl::continued() const {
  if (is_pause_) {
    if (decoder_callbacks_) {
      decoder_callbacks_->continueDecoding();
    } else if (encoder_callbacks_) {
      encoder_callbacks_->continueEncoding();
    }
  }
}

void ContextImpl::StreamBufferPool::limit(uint32_t size) {
  grow(size);
  limit_ = size;
}

uint32_t ContextImpl::StreamBufferPool::limit() const {
  if (limit_.has_value()) {
    return limit_.value();
  }

  return decoder_callbacks_ ? decoder_callbacks_->defaultDecoderBufferLimit()
                            : encoder_callbacks_->defaultEncoderBufferLimit();
}

void ContextImpl::StreamBufferPool::grow(uint32_t size) {
  if (decoder_callbacks_) {
    if (size > decoder_callbacks_->decoderBufferLimit()) {
      decoder_callbacks_->setDecoderBufferLimit(size);
    }
  } else {
    if (size > encoder_callbacks_->encoderBufferLimit()) {
      encoder_callbacks_->setEncoderBufferLimit(size);
    }
  }
}

} // namespace v1_0_x
} // namespace SrhinoPluginFramework