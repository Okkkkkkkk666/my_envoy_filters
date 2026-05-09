load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "matcher_interface",
    hdrs = [
        "matcher.h",
        "rule.h",
        "ip_list.h",
        "query_parameter_matcher.h",
        "user_matcher.h",
        "region_matcher.h",
        "date_matcher.h",

    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_3_x/proto/config/core:pkg_cc_proto",
        "//envoy/srhino_plugin_framework/v1_3_x/proto/config/type/matcher:pkg_cc_proto",
    ],
)
