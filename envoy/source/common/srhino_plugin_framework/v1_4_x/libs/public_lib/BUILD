load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "public_lib",
    srcs = [
        "cache_impl.cc",
        "public_lib_impl.cc",
    ],
    hdrs = [
        "cache_impl.h",
        "public_lib_impl.h",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_4_x/libs/public_lib:public_lib_interface",
        "//envoy/srhino_plugin_framework/v1_4_x/utility:utility_interface",
    ],
)
