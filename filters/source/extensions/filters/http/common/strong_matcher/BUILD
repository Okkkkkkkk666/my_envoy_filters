load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)
package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "strong_matcher.lib",
    repository = "@envoy",
    srcs = ["matcher.cc","rule.cc","query_parameter_matcher.cc","user_matcher.cc","ip_list.cc"],
    hdrs = ["matcher.h","rule.h","query_parameter_matcher.h","user_matcher.h","ip_list.h"],
    deps = [
        "//filters/api/envoy/extensions/filters/http/common/strong_matcher/v3:pkg_cc_proto",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/network:cidr_range_lib",
        "@envoy//source/common/router:config_lib",
    ],
)