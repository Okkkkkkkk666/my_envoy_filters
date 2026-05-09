#pragma once

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/common/ratelimit/ratelimit_client.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RatelimitClient {

class MockClient : public Client {
public:
  MockClient();
  ~MockClient() override;

  // RateLimit::Client
  MOCK_METHOD(void, cancel, ());
  MOCK_METHOD(void, limit,
              (Filters::Common::RatelimitClient::LimitRequestCallbacks & callbacks,
               const std::vector<std::shared_ptr<Impl::RateLimitPolicy>>& ratelimitpolicy,
               Tracing::Span& parent_span, const StreamInfo::StreamInfo& stream_info));
  MOCK_METHOD(void, clean,
              (Filters::Common::RatelimitClient::CleanRequestCallbacks & callbacks,
               const std::vector<std::string>& keys, Tracing::Span& parent_span,
               const StreamInfo::StreamInfo& stream_info));
};
} // namespace RatelimitClient
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
