#include "source/common/srhino_plugin_framework/v1_0_x/header_map_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
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

void HeaderMapImpl::set(const std::string_view& key, const std::string_view& value) {
  Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
  headers_->setCopy(lower_key, {value.data(), value.size()});
}

void HeaderMapImpl::modify(const std::string_view& key, std::function<bool(const std::string_view& value, std::string& new_value)> cb) {
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
} // namespace v1_0_x
} // namespace SrhinoPluginFramework