load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "net_lib",
    srcs = [
        "http_call_impl.cc",
        "grpc_call_impl.cc",
        "net_impl.cc",
    ],
    hdrs = [
        "http_call_impl.h",
        "grpc_call_impl.h",
        "net_impl.h",
    ],
    deps = [
        "//envoy/server:factory_context_interface",
        "//envoy/srhino_plugin_framework/v1_2_x/libs/net:net_interface",
        "//source/common/common:assert_lib",
        "//source/common/common:thread_lib",
        "//source/common/http:message_lib",
        "//source/common/grpc:common_lib",
        "//source/common/grpc:typed_async_client_lib",
        "//source/common/tracing:null_span_lib",
        "//source/common/srhino_plugin_framework/v1_2_x:srhino_plugin_framework_lib",
    ],
)
