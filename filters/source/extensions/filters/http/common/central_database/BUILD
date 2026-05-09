load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "central_database.lib",
    srcs = ["database.cc"],
    hdrs = ["database.h"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/common/central_database/v3:pkg_cc_proto",
        "@envoy//envoy/server:factory_context_interface",
        "@envoy//envoy/grpc:async_client_manager_interface",
        "@envoy//source/common/tracing:null_span_lib",
        "@envoy//source/common/grpc:common_lib",
    ],
)
