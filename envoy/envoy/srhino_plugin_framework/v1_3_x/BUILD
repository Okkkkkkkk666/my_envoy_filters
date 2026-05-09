load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "srhino_plugin_framework_interface",
    hdrs = [
        "call_result.h",
        "connection_info.h",
        "context.h",
        "thread_info.h",
        "data_slices.h",
        "header_map.h",
        "plugin_interface.h",
        "plugin_status.h",
        "route_info.h",
        "timer.h",
        "version.h",
        "virtual_host_info.h",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_3_x/libs:libs_interface",
        "//envoy/srhino_plugin_framework/v1_3_x/utility:utility_interface",
    ],
)
