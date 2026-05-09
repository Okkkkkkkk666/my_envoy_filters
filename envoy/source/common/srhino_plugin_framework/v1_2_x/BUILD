load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "srhino_plugin_framework_lib",
    srcs = [
        "connection_info_impl.cc",
        "context_impl.cc",
        "data_slices_impl.cc",
        "header_map_impl.cc",
        "route_info_impl.cc",
        "thread_info_impl.cc",
        "timer_impl.cc",
        "virtual_host_info_impl.cc",
    ],
    hdrs = [
        "connection_info_impl.h",
        "context_impl.h",
        "data_slices_impl.h",
        "header_map_impl.h",
        "route_info_impl.h",
        "thread_info_impl.h",
        "timer_impl.h",
        "virtual_host_info_impl.h",
    ],
    deps = [
        "//envoy/buffer:buffer_interface",
        "//envoy/srhino_plugin_framework/v1_2_x:srhino_plugin_framework_interface",
        "//source/common/http:header_map_lib",
        "//source/common/router:config_lib",
        "//source/common/srhino_plugin_framework:config_lib",
        "//source/common/srhino_plugin_framework:plugin_file_info_lib",
        "//source/common/http:utility_lib",
    ],
)

envoy_cc_library(
    name = "manifest_lib",
    srcs = [
        "manifest.cc",
    ],
    hdrs = [
        "manifest.h",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_2_x:srhino_plugin_framework_interface",
        "//envoy/srhino_plugin_framework/v1_2_x/proto/manifest:pkg_cc_proto",
        "//envoy/upstream:cluster_manager_interface",
        "//source/common/buffer:buffer_lib",
        "//source/common/common:hex_lib",
        "//source/common/crypto:utility_lib",
        "//source/common/srhino_plugin_framework:file_name_matcher_lib",
        "//source/common/srhino_plugin_framework:plugin_file_info_lib",
        "@envoy_api//envoy/extensions/upstreams/http/v3:pkg_cc_proto",
    ],
)
