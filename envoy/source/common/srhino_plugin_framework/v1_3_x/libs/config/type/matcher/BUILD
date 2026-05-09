load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "matcher_lib",
    srcs = [
        "matcher_impl.cc",
        "rule_impl.cc",
        "ip_list_impl.cc",
        "query_parameter_matcher_impl.cc",
        "user_matcher_impl.cc",
        "region_matcher_impl.cc",
        "date_matcher_impl.cc",
    ],
    hdrs = [
        "matcher_impl.h",
        "rule_impl.h",
        "ip_list_impl.h",
        "query_parameter_matcher_impl.h",
        "user_matcher_impl.h",
        "region_matcher_impl.h",
        "date_matcher_impl.h",
    ],
    deps = [
        "//source/common/http:header_utility_lib",
        "//source/common/network:cidr_range_lib",
        "//source/common/router:config_lib",
        "//envoy/srhino_plugin_framework/v1_3_x/libs/config/type/matcher:matcher_interface",
        "//envoy/srhino_plugin_framework/v1_3_x/utility:utility_interface",
        "//source/common/network:address_lib",
        "//source/common/srhino_plugin_framework/v1_3_x:srhino_plugin_framework_lib",
        "@envoy_filters//filters/source/extensions/filters/http/common/strong_matcher:strong_matcher.lib",
    ],
)
