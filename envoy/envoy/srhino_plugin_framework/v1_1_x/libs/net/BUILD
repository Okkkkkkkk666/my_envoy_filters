load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "net_interface",
    hdrs = [
        "http_call.h",
        "grpc_call.h",
        "net.h",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_1_x:srhino_plugin_framework_interface",
    ],
)
