load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "config_lib",
    srcs = [
        "config_impl.cc",
    ],
    hdrs = [
        "config_impl.h",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_0_x/libs/config:config_interface",
        "//source/common/srhino_plugin_framework/v1_0_x/libs/config/type/matcher:matcher_lib",
    ],
)
