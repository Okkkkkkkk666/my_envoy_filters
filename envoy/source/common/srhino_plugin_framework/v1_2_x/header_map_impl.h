#pragma once

#include "envoy/srhino_plugin_framework/v1_2_x/header_map.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/utility.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
class HeaderMapImpl : public HeaderMap {
public:
  HeaderMapImpl()
      : empty_headers_(std::make_unique<Envoy::Http::EmptyHeaders>()),
        headers_(empty_headers_->request_headers.get()) {}
  HeaderMapImpl(Envoy::Http::HeaderMap* headers) : headers_(headers) {}

public:
  std::string_view get(const std::string_view& key) const override;
  void get(const std::string_view& key, std::vector<std::string_view>& values) const override;
  void add(const std::string_view& key, const std::string_view& value) override;
  void add(const std::string_view& key, uint64_t value) override;
  void set(const std::string_view& key, const std::string_view& value) override;
  void addReference(const std::string_view& key, const std::string_view& value) override;
  void setReference(const std::string_view& key, const std::string_view& value) override;

  void clear() const override;
  bool empty() const override;
  std::string_view path() const override;
  std::string_view authority() const override;
  std::string_view protocol() const override;
  std::string_view method() const override;
  std::string_view host() const override;
  uint32_t statusCode() const override;
  std::string_view userAgent() const override;
  std::string_view forwardedFor() const override;
  std::string_view contentEncoding() const override;
  std::string_view contentType() const override;

  uint64_t byteSize() const override;
  size_t size() const override;
  size_t removePrefix(const std::string_view& prefix) const override;

  void
  parseCookieHeaderValue(const Envoy::Http::LowerCaseString& cookie_header,
                         const absl::string_view& cookie_key,
                         const std::function<bool(const std::string_view)>& onGetCookieValue) const;
  void parseCookieValue(
      const std::string_view& cookie_key,
      const std::function<bool(const std::string_view)>& onGetCookieValue) const override;

  void parseSetCookieValue(
      const std::string_view& cookie_key,
      const std::function<bool(const std::string_view)>& onGetCookieValue) const override;

  void
  modify(const std::string_view& key,
         std::function<bool(const std::string_view& value, std::string& new_value)> cb) override;
  std::size_t remove(const std::string_view& key) override;
  void traverse(std::function<bool(const std::string_view& key, const std::string_view& value)> cb)
      const override;
  std::unique_ptr<HeaderMap> create() const override;
  std::unique_ptr<HeaderMap>
  create(const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers)
      const override;

public:
  Envoy::Http::HeaderMap* raw() { return headers_; }
  const Envoy::Http::HeaderMap* raw() const { return headers_; }

private:
  std::unique_ptr<Envoy::Http::EmptyHeaders> empty_headers_;
  Envoy::Http::HeaderMap* headers_;
};
} // namespace v1_2_x
} // namespace SrhinoPluginFramework