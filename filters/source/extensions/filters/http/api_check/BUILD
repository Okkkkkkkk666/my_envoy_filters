load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

package(default_visibility = ["//visibility:public"])

envoy_cc_library(
    name = "api_check_lib",
    srcs = [
        "api_check.cc",
    ],
    hdrs = [
        "api_check.h", "impl/waf_check_mode.h"
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
        "//filters/api/envoy/extensions/filters/http/api_check/v3:pkg_cc_proto",
        "@envoy//source/exe:envoy_common_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
        "//filters/source/extensions/filters/http/waf:waf_lib",
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
        ":api_check_lib",
    ],
)
