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
    name = "net_test",
    srcs = [
        "grpc_call_test.cc",
        "http_call_test.cc",
    ],
    deps = [
        "//source/common/srhino_plugin_framework/v1_1_x:srhino_plugin_framework_lib",
        "//source/common/srhino_plugin_framework/v1_1_x/libs/net:net_lib",
        "//test/mocks/server:factory_context_mocks",
    ],
)
