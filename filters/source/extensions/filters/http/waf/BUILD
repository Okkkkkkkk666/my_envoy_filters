load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

package(default_visibility = ["//visibility:public"])

envoy_cc_library(
    name = "waf_lib",
    srcs = [
        "utility.cc",
        "waf.cc",
        "webhook_fetcher.cc",
    ],
    hdrs = [
        "utility.h",
        "waf.h",
        "webhook_fetcher.h",
    ],
    copts = [
        "-Wno-unused-function",
        "-Wno-unused-parameter",
        "-Wno-reorder",
        "-Wno-unused-variable",
        "-Wno-sign-compare",
        "-Wno-non-virtual-dtor",
        "-Wno-overloaded-virtual",
        "-Wno-unused-result",
        "-Ithird-party/ModSecurity/headers",
    ],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/waf/v3:pkg_cc_proto",
        "//third-party:modsecurity_lib",
        "@envoy//source/exe:envoy_common_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
        "//filters/source/extensions/filters/http/common/ip_whitelist:ip_whitelist.lib", 
        "//filters/source/extensions/filters/http/common/ratelimit:ratelimit.lib",
        "//filters/source/extensions/filters/http/strong_global_ratelimit:strong_global_ratelimit_lib",
        "//filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/utility:utility_lib",
    ],
)

envoy_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    copts = [
        "-Wno-unused-function",
        "-Wno-unused-parameter",
        "-Wno-reorder",
        "-Wno-unused-variable",
        "-Wno-sign-compare",
        "-Wno-non-virtual-dtor",
        "-Wno-overloaded-virtual",
        "-Ithird-party/ModSecurity/headers",
    ],
    repository = "@envoy",
    deps = [
        ":waf_lib",
    ],
)
