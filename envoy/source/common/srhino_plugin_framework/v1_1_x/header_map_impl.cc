#include "source/common/srhino_plugin_framework/v1_1_x/header_map_impl.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
std::string_view HeaderMapImpl::get(const std::string_view& key) const {
  std::string_view result;
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
  Envoy::Http::HeaderMap::GetResult get_result = headers_->get(lower_key);
  if (!get_result.empty()) {
    auto value = get_result[0]->value().getStringView();
    result = std::string_view(value.data(), value.length());
  }

  return result;
}

void HeaderMapImpl::get(const std::string_view& key, std::vector<std::string_view>& values) const {
  values.clear();
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
  Envoy::Http::HeaderMap::GetResult get_result = headers_->get(lower_key);
  for (size_t i = 0; i < get_result.size(); ++i) {
    auto value = get_result[i]->value().getStringView();
    values.emplace_back(value.data(), value.size());
  }
}

void HeaderMapImpl::add(const std::string_view& key, const std::string_view& value) {
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
  Envoy::Http::LowerCaseString lower_value({value.data(), value.size()});
  Envoy::Http::HeaderString header_string_key;
  Envoy::Http::HeaderString header_string_value;
  header_string_key.append(lower_key.get().c_str(), lower_key.get().size());
  header_string_value.append(lower_value.get().c_str(), lower_value.get().size());
  headers_->addViaMove(std::move(header_string_key), std::move(header_string_value));
}

void HeaderMapImpl::add(const std::string_view& key, uint64_t value) {
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
  headers_->addReferenceKey(lower_key, value);
}

void HeaderMapImpl::set(const std::string_view& key, const std::string_view& value) {
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
  headers_->setCopy(lower_key, {value.data(), value.size()});
}

void HeaderMapImpl::addReference(const std::string_view& key, const std::string_view& value) {
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
  headers_->addReference(lower_key, {value.data(), value.size()});
}

void HeaderMapImpl::setReference(const std::string_view& key, const std::string_view& value) {
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
  headers_->setReference(lower_key, {value.data(), value.size()});
}

uint64_t HeaderMapImpl::byteSize() const { return headers_->byteSize(); }

void HeaderMapImpl::clear() const { return headers_->clear(); }

size_t HeaderMapImpl::size() const { return headers_->size(); }

bool HeaderMapImpl::empty() const { return headers_->empty(); }

std::string_view HeaderMapImpl::path() const { return get(":path"); }

std::string_view HeaderMapImpl::authority() const { return get(":authority"); }

std::string_view HeaderMapImpl::protocol() const { return get(":protocol"); }

std::string_view HeaderMapImpl::method() const { return get(":method"); }

std::string_view HeaderMapImpl::host() const { return get("host"); }

uint32_t HeaderMapImpl::statusCode() const { return atoi(get(":status").data()); }

std::string_view HeaderMapImpl::userAgent() const { return get("user-agent"); }

std::string_view HeaderMapImpl::forwardedFor() const { return get("x-forwarded-for"); }

std::string_view HeaderMapImpl::contentEncoding() const { return get("content-encoding"); }

std::string_view HeaderMapImpl::contentType() const { return get("content-type"); }

size_t HeaderMapImpl::removePrefix(const std::string_view& prefix) const {
  Envoy::Http::LowerCaseString lower_key({prefix.data(), prefix.size()});
  return headers_->removePrefix(lower_key);
}

void HeaderMapImpl::parseCookieValue(
    const std::string_view& cookie_key,
    const std::function<bool(const std::string_view)>& onGetCookieValue) const {
  parseCookieHeaderValue(Envoy::Http::Headers::get().Cookie, {cookie_key.data(), cookie_key.size()},
                                onGetCookieValue);
}

void HeaderMapImpl::parseSetCookieValue(
    const std::string_view& cookie_key,
    const std::function<bool(const std::string_view)>& onGetCookieValue) const {
  parseCookieHeaderValue(Envoy::Http::Headers::get().SetCookie, {cookie_key.data(), cookie_key.size()},
                                onGetCookieValue);
}

// 分割字符串函数，可以基于分隔符进行分割
void splitStrings(const std::string_view& str, char delimiter,
                  const std::function<void(const std::string_view)>& callback) {
  size_t start = 0;
  size_t end = str.find(delimiter);

  while (end != std::string_view::npos) {
    callback(str.substr(start, end - start));
    start = end + 1;
    end = str.find(delimiter, start);
  }

  if (start < str.size()) {
    callback(str.substr(start));
  }
}

void HeaderMapImpl::parseCookieHeaderValue(
    const Envoy::Http::LowerCaseString& cookie_header, const absl::string_view& cookie_key,
    const std::function<bool(const std::string_view)>& onGetCookieValue) const {
  Envoy::Http::forEachCookie(*headers_, cookie_header, [&](absl::string_view key, absl::string_view value) {
    if (key == cookie_key) {
      if (onGetCookieValue) {
        return onGetCookieValue({value.data(), value.size()});
      }
      return false;
    }
    return true;
  });
}

void HeaderMapImpl::modify(
    const std::string_view& key,
    std::function<bool(const std::string_view& value, std::string& new_value)> cb) {
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
  Envoy::Http::HeaderMap::GetResult get_result = headers_->get(lower_key);
  for (size_t i = 0; i < get_result.size(); ++i) {
    auto value = get_result[i]->value().getStringView();
    std::string new_value;
    if (cb({value.data(), value.size()}, new_value)) {
      const_cast<Envoy::Http::HeaderEntry*>(get_result[i])->value().setCopy(new_value);
    }
  }
}

std::size_t HeaderMapImpl::remove(const std::string_view& key) {
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});

  return headers_->remove(lower_key);
}

void HeaderMapImpl::traverse(
    std::function<bool(const std::string_view& key, const std::string_view& value)> cb) const {
  std::function<Envoy::Http::HeaderMap::Iterate(const Envoy::Http::HeaderEntry&)> func =
      [&cb](const Envoy::Http::HeaderEntry& entry) {
        auto key = entry.key().getStringView();
        auto value = entry.value().getStringView();
        if (!cb({key.data(), key.size()}, {value.data(), value.size()})) {
          return Envoy::Http::HeaderMap::Iterate::Break;
        }

        return Envoy::Http::HeaderMap::Iterate::Continue;
      };

  headers_->iterate(func);
}

std::unique_ptr<HeaderMap> HeaderMapImpl::create() const {
  return std::make_unique<HeaderMapImpl>();
}

std::unique_ptr<HeaderMap> HeaderMapImpl::create(
    const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers) const {
  auto result = create();
  for (const auto& header : headers) {
    result->add(header.first, header.second);
  }

  return result;
}
} // namespace v1_1_x
} // namespace SrhinoPluginFramework