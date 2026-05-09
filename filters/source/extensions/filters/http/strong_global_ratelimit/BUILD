load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "strong_global_ratelimit_lib",
    repository = "@envoy",
    srcs = ["strong_global_ratelimit.cc","impl/action.cc","impl/rule.cc"],
    hdrs = ["strong_global_ratelimit.h","impl/action.h","impl/rule.h"],
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/strong_matcher:strong_matcher.lib",
        "//filters/source/extensions/filters/http/common/ratelimit:ratelimit.lib",
        "//filters/source/extensions/filters/http/common/ip_whitelist:ip_whitelist.lib",
        "@envoy//source/common/grpc:typed_async_client_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
    ],
)

srhino_cc_library(
    name = "config",
    repository = "@envoy",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":strong_global_ratelimit_lib",
        "//filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
