load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "thread_local_lib",
    srcs = [
        "cluster_impl.cc",
        "thread_local_impl.cc",
    ],
    hdrs = [
        "cluster_impl.h",
        "thread_local_impl.h",
    ],
    deps = [
        "//envoy/server:factory_context_interface",
        "//envoy/srhino_plugin_framework/v1_1_x/libs/thread_local:thread_local_interface",
        "//source/common/srhino_plugin_framework/v1_1_x:srhino_plugin_framework_lib",
    ],
)
