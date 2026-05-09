#include <format>
#include <fstream>

#include "source/common/srhino_plugin_framework/v1_2_x/manifest.h"
#include "envoy/srhino_plugin_framework/v1_2_x/proto/manifest/manifest.pb.validate.h"
#include "source/common/srhino_plugin_framework/file_name_matcher.h"
#include "envoy/srhino_plugin_framework/v1_2_x/version.h"
#include "envoy/srhino_plugin_framework/v1_2_x/utility/proto_tools.hpp"
#include "envoy/srhino_plugin_framework/v1_2_x/utility/empty_string.hpp"
#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/hex.h"
#include "source/common/crypto/utility_impl.h"
#include "envoy/extensions/upstreams/http/v3/http_protocol_options.pb.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
Manifest::Manifest(const std::string& json, const PluginFileInfo& plugin_file_info,
                   Envoy::Upstream::ClusterManager* cm)
    : plugin_file_info_(plugin_file_info), cm_(cm) {
  loadManifestFromJson(json);
}

bool Manifest::check(std::string& error) {
  if (!checkBaseInfo(error)) {
    return false;
  }

  if (!createResource(error)) {
    revertCreateResource();
    return false;
  }

  if (!createController(error)) {
    revertCreateController();
    return false;
  }

  return true;
}

const srhino_plugin_framework::v1_2_x::proto::manifest::resource::Resource* Manifest::getResource(
    srhino_plugin_framework::v1_2_x::proto::manifest::resource::Resource::TypeCase type,
    const std::string& name) const {
  for (const auto& res : manifest_.resources()) {
    if (res.type_case() == type && res.name() == name) {
      return &res;
    }
  }

  return nullptr;
}

std::string Manifest::getFileResourcePath(const std::string& name) const {
  auto res = getResource(
      srhino_plugin_framework::v1_2_x::proto::manifest::resource::Resource::TypeCase::kFile, name);
  if (res && res->has_file()) {
    return std::format("{}/{}", plugin_file_info_.dir_, res->file().path());
  }

  return Utility::EMPTY_STRING;
}

const google::protobuf::RepeatedPtrField<
    srhino_plugin_framework::v1_2_x::proto::manifest::Controller>&
Manifest::getControllers() const {
  return manifest_.controllers();
}

bool Manifest::loadManifestFromJson(const std::string& json) {
  static constexpr char error_prefix[] = "load manifest failure";

  std::string error = Utility::ProtoTools::jsonToMessage(json, manifest_);
  if (!error.empty()) {
    load_error_ = std::format("{}: {}", error_prefix, error);
    return false;
  }

  try {
    if (!srhino_plugin_framework::v1_2_x::proto::manifest::Validate(manifest_, &error)) {
      load_error_ = std::format("{}: {}", error_prefix, error);
      return false;
    }
    return true;
  } catch (const std::exception& e) {
    load_error_ = std::format("{}: {}", error_prefix, e.what());
  }

  return false;
}

bool Manifest::checkBaseInfo(std::string& error) const {
  static constexpr char error_prefix[] = "manifest check base info failure";

  error.clear();

  // 校验文件是否加载成功
  if (!load_error_.empty()) {
    error = load_error_;
    return false;
  }

  // 校验名字
  if (manifest_.name() != plugin_file_info_.name_) {
    error = std::format("{}: plugin name: {}, manifest name: {}", error_prefix,
                        plugin_file_info_.name_, manifest_.name());
    return false;
  }

  // 校验所需的框架版本
  uint32_t curr_framework_ver =
      SrhinoPluginFramework::FileNameMatcher::version(SRHINO_PLUGIN_FRAMEWORK_VERSION);
  uint32_t min_framework_ver =
      SrhinoPluginFramework::FileNameMatcher::version(manifest_.framework_minimum_required());
  if (curr_framework_ver < min_framework_ver) {
    error = std::format("{}: framework version: {}, required: {} ", error_prefix,
                        SRHINO_PLUGIN_FRAMEWORK_VERSION, manifest_.framework_minimum_required());
    return false;
  }

  // 校验manifest版本
  if (SUPPORT_MANIFEST_MINIMUM_VERSION < manifest_.manifest_version()) {
    error = std::format("{}: support version: {}, manifest version: {} ", error_prefix,
                        SUPPORT_MANIFEST_MINIMUM_VERSION, manifest_.manifest_version());
    return false;
  }

  return true;
}

/**
 * 生成资源名称
 * 格式：
 * 开发者组织.插件名.资源类型.资源名称
 */
std::string Manifest::makeResourceName(ManifestResource::TypeCase type,
                                       const std::string& name) const {
  static std::unordered_map<ManifestResource::TypeCase, std::string> resource_type{
      {ManifestResource::TypeCase::kCluster, "cluster"},
      {ManifestResource::TypeCase::kFile, "file"},
      {ManifestResource::TypeCase::kListener, "listener"}};

  return std::format("{}.{}.{}", plugin_file_info_.file_name_, resource_type[type], name);
}

bool Manifest::createResource(std::string& error) {
  error.clear();
  for (auto& res : manifest_.resources()) {
    if (!error.empty()) {
      break;
    }

    switch (res.type_case()) {
    case ManifestResource::TypeCase::kCluster:
      createCluster(res, error);
      break;
    case ManifestResource::TypeCase::kFile:
      createFile(res, error);
      break;
    case ManifestResource::TypeCase::kListener:
      createListener(res, error);
      break;
    default:
      error = "create manifest resource failure: unknow resource type";
      break;
    }
  }

  return error.empty();
}

void Manifest::revertCreateResource() {
  revertCreateListener();
  revertCreateFile();
  revertCreateCluster();
}

bool Manifest::createCluster(const Manifest::ManifestResource& res, std::string& error) {
  static constexpr char error_prefix[] = "create cluster failure";

  using namespace Envoy;
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  if (!cm_) {
    error = std::format("{}: cluster manager pointer is null", error_prefix);
    return false;
  }

  if (!res.has_cluster()) {
    error = std::format("{}: has no cluster", error_prefix);
    return false;
  }

  envoy::config::cluster::v3::Cluster cluster;
  std::string cluster_name = makeResourceName(ManifestResource::TypeCase::kCluster, res.name());
  cluster.set_name(cluster_name);
  cluster.set_type(
      envoy::config::cluster::v3::Cluster::DiscoveryType::Cluster_DiscoveryType_LOGICAL_DNS);
  cluster.set_dns_lookup_family(
      envoy::config::cluster::v3::Cluster::DnsLookupFamily::Cluster_DnsLookupFamily_V4_ONLY);

  // 设置endpoints
  for (const auto& endpoint : res.cluster().endpoints()) {
    envoy::config::endpoint::v3::LbEndpoint* lb_endpoint = cluster.mutable_load_assignment()
                                                               ->mutable_endpoints()
                                                               ->Add()
                                                               ->mutable_lb_endpoints()
                                                               ->Add();
    lb_endpoint->set_health_status(envoy::config::core::v3::HealthStatus::HEALTHY);
    envoy::config::core::v3::SocketAddress* socket_address =
        lb_endpoint->mutable_endpoint()->mutable_address()->mutable_socket_address();
    socket_address->set_address(endpoint.address());
    socket_address->set_port_value(endpoint.port());
  }

  // 设置HTTPS
  if (!res.cluster().sni().empty()) {
    envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext tls_context;
    tls_context.set_sni(res.cluster().sni());
    cluster.mutable_transport_socket()->mutable_typed_config()->PackFrom(tls_context);
    cluster.mutable_transport_socket()->set_name("envoy.transport_sockets.tls");
  }

  // 设置HTTP2
  if (res.cluster().protocol_options() == srhino_plugin_framework::v1_2_x::proto::manifest::
                                              resource::Cluster_HttpProtocolOptions_HTTP2) {
    envoy::config::core::v3::Http2ProtocolOptions http2_protocol_options;
    envoy::extensions::upstreams::http::v3::HttpProtocolOptions protocol_options;
    protocol_options.mutable_explicit_http_config()->mutable_http2_protocol_options()->CopyFrom(
        http2_protocol_options);
    (*cluster.mutable_typed_extension_protocol_options())
        ["envoy.extensions.upstreams.http.v3.HttpProtocolOptions"]
            .PackFrom(protocol_options);
  }

  if (cm_->addOrUpdateCluster(cluster, "http_call")) {
    revert_create_cluster_.push(cluster_name);
  }

  return true;
}

void Manifest::revertCreateCluster() {
  while (!revert_create_cluster_.empty()) {
    cm_->removeCluster(revert_create_cluster_.top());
    revert_create_cluster_.pop();
  }
}

bool Manifest::createFile(const Manifest::ManifestResource& res, std::string& error) {
  static constexpr char error_prefix[] = "create file failure";

  using namespace Envoy;
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  if (!res.has_file()) {
    error = std::format("{}: has no file", error_prefix);
    return false;
  }

  // 打开文件
  const std::string path = getFileResourcePath(res.name());
  std::ifstream ifs;
  ifs.open(path, std::ios::binary);
  if (!ifs.is_open()) {
    error = std::format("{}: can't open the file: {}", error_prefix, path);
    return false;
  }

  // 读取整个文件
  ifs.seekg(0, ifs.end);
  size_t file_size = static_cast<size_t>(ifs.tellg());
  ifs.seekg(0, ifs.beg);

  std::string buffer;
  buffer.resize(file_size);
  ifs.read(buffer.data(), buffer.size());
  if (static_cast<size_t>(ifs.gcount()) != buffer.size()) {
    error = std::format("{}: can't read the file: {}", error_prefix, path);
    return false;
  }
  ifs.close();

  // 校验sha256
  const std::string sha256 = Envoy::Hex::encode(
      Envoy::Common::Crypto::UtilitySingleton::get().getSha256Digest(Buffer::OwnedImpl(buffer)));
  if (sha256 != res.file().sha256()) {
    error = std::format("{}: {} file's sha256sum check fail. declare: {}, actuality: {}",
                        error_prefix, res.name(), res.file().sha256(), sha256);
    return false;
  }

  return true;
}

void Manifest::revertCreateFile() {
  // 没有创建任何文件，不需要回退
  return;
}

bool Manifest::createListener(const Manifest::ManifestResource& /*res*/, std::string& /*error*/) {
  // TODO: 完善此方法
  return true;
}

void Manifest::revertCreateListener() {
  // TODO: 完善此方法
  return;
}

bool Manifest::createController(std::string& error) {
  static constexpr char error_prefix[] = "create controller failure";
  error.clear();

  using namespace Envoy;
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  // 防止重复
  auto& controllers = manifest_.controllers();
  for (auto i = controllers.begin(); i != controllers.end() && error.empty(); ++i) {
    // params 不能重复
    auto& params = i->params();
    for (auto ii = params.begin(); ii != params.end() && error.empty(); ++ii) {
      for (auto jj = ii + 1; jj != params.end(); ++jj) {
        if (ii->key() == jj->key()) {
          error =
              std::format("{}: controller's param key({}) is not unique.", error_prefix, ii->key());
          break;
        }
      }
    }
    if (!error.empty()) {
      break;
    }

    for (auto j = i + 1; j != controllers.end(); ++j) {
      // name 不能重复
      if (i->name() == j->name()) {
        error = std::format("{}: controller's name({}) is not unique.", error_prefix, i->name());
        break;
      }

      // path && method 不能重复
      if (i->path() == j->path() && i->method() == j->method()) {
        error = std::format("{}: controller's path and method({} {}) is not unique", error_prefix,
                            i->Method_Name(i->method()), i->path());
        break;
      }
    }
  }

  return error.empty();
}

void Manifest::revertCreateController() {}
} // namespace v1_2_x
} // namespace SrhinoPluginFramework