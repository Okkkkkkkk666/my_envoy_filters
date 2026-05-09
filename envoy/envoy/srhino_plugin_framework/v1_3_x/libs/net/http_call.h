#pragma once
#include <memory>
#include <unordered_map>
#include <functional>
#include <string_view>

#include "envoy/srhino_plugin_framework/v1_3_x/header_map.h"
#include "envoy/srhino_plugin_framework/v1_3_x/data_slices.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Net {
class HttpCall {
protected:
  virtual ~HttpCall() = default;

public:
  enum class Scheme {
    HTTP,
    HTTPS,
  };

  using Callback = std::function<void(bool, const HeaderMap&, const DataSlices&)>;

public:
  virtual bool get(const std::string_view& path, const HeaderMap& headers, const DataSlices& body,
                   Callback cb) = 0;
  virtual bool get(const std::string_view& path, const HeaderMap& headers, Callback cb) = 0;
  virtual bool
  get(const std::string_view& path,
      const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers,
      const std::string_view& body, Callback cb) = 0;
  virtual bool post(const std::string_view& path, const HeaderMap& headers, const DataSlices& body,
                    Callback cb) = 0;
  virtual bool post(const std::string_view& path, const HeaderMap& headers, Callback cb) = 0;
  virtual bool
  post(const std::string_view& path,
       const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers,
       const std::string_view& body, Callback cb) = 0;
  virtual void cancel() = 0;
};

using HttpCallSharedPtr = std::shared_ptr<HttpCall>;
} // namespace Net
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework