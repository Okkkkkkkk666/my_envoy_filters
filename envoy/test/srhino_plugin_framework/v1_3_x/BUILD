load(
    "//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "header_map_impl_test",
    srcs = ["header_map_impl_test.cc"],
    deps = [
        "//source/common/srhino_plugin_framework/v1_3_x:srhino_plugin_framework_lib",
    ],
)
