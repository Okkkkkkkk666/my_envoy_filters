load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_contrib_package",
)

licenses(["notice"])  # Apache 2

envoy_contrib_package()

envoy_cc_library(
    name = "fpe_lib",
    srcs = ["fpe_impl.cc"],
    hdrs = ["fpe.h"],
    deps = [
        "//source/common/common:logger_lib",
        "//envoy/thread_local:thread_local_interface",
    ],
)
