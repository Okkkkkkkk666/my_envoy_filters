load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
    "envoy_google_grpc_external_deps",
    "envoy_select_google_grpc",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "pubilc_libs_grpc_service",
    srcs = ["service_impl.cc","grpc_server.cc"],
    hdrs = ["service_impl.h","grpc_server.h"],
    deps = [
        "@envoy_api//envoy/service/public_libs/v3:pkg_cc_proto",
        "@envoy_api//envoy/type/v3:pkg_cc_proto",
        "@envoy_api//envoy/service/public_libs/v3:public_libs_service_grpc",
        "//envoy/grpc:async_client_interface",
        "//envoy/upstream:cluster_manager_interface",
        "//source/common/grpc:async_client_lib",
        "@envoy//envoy/grpc:async_client_manager_interface",
        "@envoy//source/common/grpc:common_lib",
        "//source/common/common:logger_lib",
        "//source/common/common:assert_lib",
        "//source/common/srhino_plugin_framework/v1_4_x/libs/public_lib:public_lib",
    ] + envoy_google_grpc_external_deps(),
)
