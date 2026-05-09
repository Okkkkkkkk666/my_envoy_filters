# load(
#     "@envoy//bazel:envoy_build_system.bzl",
#     "envoy_cc_library",
# )
load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "strong_local_ratelimit_lib",
    srcs = [
        "impl/action.cc",
        "impl/key.cc",
        "impl/quota.cc",
        "impl/rule.cc",
        "strong_local_ratelimit.cc",
    ],
    hdrs = [
        "impl/action.h",
        "impl/key.h",
        "impl/quota.h",
        "impl/rule.h",
        "strong_local_ratelimit.h",
        "test_stat.h",
    ],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common:common_lib",
        "//filters/source/extensions/filters/http/common/strong_matcher:strong_matcher.lib",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/singleton:threadsafe_singleton",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
    ],
)

srhino_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":strong_local_ratelimit_lib",
        "//filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
