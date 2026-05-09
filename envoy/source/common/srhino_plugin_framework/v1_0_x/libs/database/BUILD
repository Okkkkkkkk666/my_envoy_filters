load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "database_lib",
    srcs = [
        "central_database_impl.cc",
        "sqlite_database_impl.cc",
        "database_impl.cc",
    ],
    hdrs = [
        "central_database_impl.h",
        "sqlite_database_impl.h",
        "database_impl.h",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_0_x/libs/database:database_interface",
        "//envoy/srhino_plugin_framework/v1_0_x/proto/database:pkg_cc_proto",
        "//source/common/srhino_plugin_framework/v1_0_x:srhino_plugin_framework_lib",
        "@envoy//envoy/grpc:async_client_manager_interface",
        "@envoy//source/common/grpc:common_lib",
        "@envoy//source/common/tracing:null_span_lib",
        "@envoy_filters//third-party:sqlite3_lib",
    ],
)
