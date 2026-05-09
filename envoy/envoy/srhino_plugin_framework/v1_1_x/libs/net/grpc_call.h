#pragma once

#include "envoy/srhino_plugin_framework/v1_1_x/data_slices.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Net {
class GrpcCallback {
public:
  virtual ~GrpcCallback() = default;

public:
  enum class Status { SUCCESS, FAILURE };

public:
  virtual void complete(Status status, const DataSlices& response) = 0;
};

class GrpcCall {
protected:
  virtual ~GrpcCall() = default;

public:
  virtual void send(const std::string& method, const DataSlices& data, GrpcCallback& cb) = 0;
  virtual void send(const std::string& method, const void* data, size_t size, GrpcCallback& cb) = 0;
  virtual void cancel() = 0;
};
using GrpcCallSharedPtr = std::shared_ptr<GrpcCall>;
} // namespace Net
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework