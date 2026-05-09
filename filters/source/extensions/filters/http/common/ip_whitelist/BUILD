load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)
package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "ip_whitelist.lib",
    repository = "@envoy",
    srcs = ["ip_whitelist.cc"],
    hdrs = ["ip_whitelist.h"],
    deps = [
        "//filters/api/envoy/extensions/filters/http/common/ip_whitelist/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/strong_matcher:strong_matcher.lib",
        "@envoy//source/common/network:cidr_range_lib",
    ],
)