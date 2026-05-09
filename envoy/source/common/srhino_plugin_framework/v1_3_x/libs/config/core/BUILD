load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "core_lib",
    srcs = [
        "cidr_impl.cc",
    ],
    hdrs = [
        "cidr_impl.h",
    ],
    deps = [
        "envoy/source/common/network:cidr_range_lib",
    ],
)
