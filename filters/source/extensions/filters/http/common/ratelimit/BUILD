load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)
package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "ratelimit.lib",
    repository = "@envoy",
    srcs = ["impl/ratelimit_client_impl.cc"],
    hdrs = ["ratelimit_client.h","impl/ratelimit_client_impl.h","impl/ratelimit_policy.h"],
    deps = [
        "//filters/api/envoy/extensions/filters/http/common/ratelimit/v3:pkg_cc_proto",
        "@envoy//source/common/grpc:typed_async_client_lib",
    ],
)
