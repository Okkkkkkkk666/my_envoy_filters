#include "source/common/srhino_plugin_framework/v1_2_x/libs/net/http_call_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/header_map_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/data_slices_impl.h"
#include "source/common/http/message_impl.h"
#include "source/common/event/dispatcher_impl.h"
#include <thread>

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Net {
bool HttpCallImpl::get(const std::string_view& path, const HeaderMap& headers,
                       const DataSlices& body, HttpCall::Callback cb) {
  if (http_request_) {
    return false;
  }

  callback_ = cb;

  return send(Envoy::Http::Headers::get().MethodValues.Get, path, headers, body);
}

bool HttpCallImpl::get(const std::string_view& path, const HeaderMap& headers, Callback cb) {
  DataSlicesImpl data_slices;
  return get(path, headers, data_slices, cb);
}

bool HttpCallImpl::get(
    const std::string_view& path,
    const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers,
    const std::string_view& body, HttpCall::Callback cb) {
  // 根据初始化列表初始化HeaderMapImpl
  HeaderMapImpl header_map;
  for (const auto& header : headers) {
    Envoy::Http::LowerCaseString key({header.first.data(), header.first.size()});
    header_map.raw()->addCopy(key, {header.second.data(), header.second.size()});
  }

  // 构造DataSlicesImpl并填充
  DataSlicesImpl data_slices;
  data_slices.raw()->add(body.data(), body.size());

  return get(path, header_map, data_slices, cb);
}

bool HttpCallImpl::post(const std::string_view& path, const HeaderMap& headers,
                        const DataSlices& body, HttpCall::Callback cb) {
  if (http_request_) {
    return false;
  }

  callback_ = cb;

  return send(Envoy::Http::Headers::get().MethodValues.Post, path, headers, body);
}

bool HttpCallImpl::post(const std::string_view& path, const HeaderMap& headers, Callback cb) {
  DataSlicesImpl data_slices;
  return post(path, headers, data_slices, cb);
}

bool HttpCallImpl::post(
    const std::string_view& path,
    const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers,
    const std::string_view& body, HttpCall::Callback cb) {
  // 根据初始化列表初始化HeaderMapImpl
  HeaderMapImpl header_map;
  for (const auto& header : headers) {
    Envoy::Http::LowerCaseString key({header.first.data(), header.first.size()});
    header_map.raw()->addCopy(key, {header.second.data(), header.second.size()});
  }

  // 构造DataSlicesImpl并填充
  DataSlicesImpl data_slices;
  data_slices.raw()->add(body.data(), body.size());

  return post(path, header_map, data_slices, cb);
}

void HttpCallImpl::cancel() {
  if (http_request_) {
    http_request_->cancel();
    http_request_ = nullptr;
  }
}

void HttpCallImpl::onSuccess(const Envoy::Http::AsyncClient::Request&,
                             Envoy::Http::ResponseMessagePtr&& response) {
  http_request_ = nullptr;
  if (callback_) {
    HeaderMapImpl headers(&response->headers());
    DataSlicesImpl data(response->body());
    callback_(true, headers, data);
  }
}

void HttpCallImpl::onFailure(const Envoy::Http::AsyncClient::Request&,
                             Envoy::Http::AsyncClient::FailureReason) {
  http_request_ = nullptr;
  if (callback_) {
    callback_(false, HeaderMapImpl(), DataSlicesImpl());
  }
}

bool HttpCallImpl::send(const absl::string_view& method, const std::string_view& path,
                        const HeaderMap& headers, const DataSlices& body) {
  Envoy::Upstream::ThreadLocalCluster* cluster =
      factory_context_.clusterManager().getThreadLocalCluster(cluster_name_);
  if (cluster) {
    Envoy::Http::RequestMessagePtr request = std::make_unique<Envoy::Http::RequestMessageImpl>();
    request->body().add(*(dynamic_cast<const DataSlicesImpl&>(body).raw()));
    request->headers().setReferenceMethod(method);
    request->headers().setReferencePath({path.data(), path.size()});

    headers.traverse([&](const std::string_view& key, const std::string_view& value) {
      request->headers().setCopy(Envoy::Http::LowerCaseString({key.data(), key.size()}),
                                 {value.data(), value.size()});
      return true;
    });

    http_request_ = cluster->httpAsyncClient().send(
        std::move(request), *this,
        Envoy::Http::AsyncClient::RequestOptions().setTimeout(timeout_).setSendXff(false));
    if (http_request_) {
      return true;
    }
  }

  return false;
}

bool HttpCallImpl::validateTimeout(const std::chrono::milliseconds& timeout) {
  if (timeout.count() < 0 || timeout.count() > 1000 * 60) {
    return false;
  }
  return true;
}
} // namespace Net
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework