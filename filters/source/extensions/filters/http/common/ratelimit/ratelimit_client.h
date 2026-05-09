#pragma once
#include "envoy/tracing/http_tracer.h"

#include "filters/api/envoy/extensions/filters/http/common/ratelimit/v3/ratelimit.pb.h"

#include "impl/ratelimit_policy.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RatelimitClient {

namespace v3 = envoy::extensions::filters::http::common::ratelimit::v3;

using LimitGrpcResponsePtr = std::unique_ptr<v3::RateLimitResponse>;
using CleanGrpcResponsePtr = std::unique_ptr<v3::CleanQuotaResponse>;

enum class LimitStatus {
  // The request is not over limit.
  OK,
  // The rate limit service could not be queried.
  Error,
  // The request is over limit.
  OverLimit
};
enum class CleanStatus {
  // The request is not over limit.
  SUCCESS,
  // The rate limit service could not be queried.
  FAILURE,
};

/**
 * Async callbacks used during limit() calls.
 */
class LimitRequestCallbacks {
public:
  virtual ~LimitRequestCallbacks() = default;

  /**
   * Called when a limit request is complete. The resulting status, response headers
   * and request headers to be forwarded to the upstream are supplied.
   */
  virtual void complete(LimitStatus status, LimitGrpcResponsePtr&& response) PURE;
};

/**
 * Async callbacks used during clean() calls.
 */
class CleanRequestCallbacks {
public:
  virtual ~CleanRequestCallbacks() = default;

  virtual void complete(CleanStatus status) PURE;
};

class Client {
public:
  virtual ~Client() = default;

  /**
   * Cancel an inflight limit request.
   */
  virtual void cancel() PURE;

  /**
   * Request a limit check.This abstract API matches the global ratelimit service on the dam
   * @param ratelimitpolicy ratelimit rules
   * @param callbacks supplies the completion callbacks.
   * @param parent_span source for generating an egress child span as part of the trace.
   *
   */
  virtual void limit(LimitRequestCallbacks& callbacks,
                     const std::vector<std::shared_ptr<Impl::RateLimitPolicy>>& ratelimitpolicy,
                     Tracing::Span& parent_span, const StreamInfo::StreamInfo& stream_info) PURE;

  /**
   * Request a limit check.This abstract API matches the global ratelimit service on the dam
   * @param key ratelimit unique identification
   * @param callbacks supplies the completion callbacks.
   * @param parent_span source for generating an egress child span as part of the trace.
   *
   */
  virtual void clean(CleanRequestCallbacks& callbacks, const std::vector<std::string>& keys,
                     Tracing::Span& parent_span, const StreamInfo::StreamInfo& stream_info) PURE;
};

using ClientPtr = std::unique_ptr<Client>;

} // namespace RatelimitClient
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
