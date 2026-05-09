load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "super_glue_client_lib",
    srcs = [
        "rule.cc",
        "super_glue_client.cc",
    ],
    hdrs = [
        "common.h",
        "rule.h",
        "super_glue_client.h",
    ],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/super_glue/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/strong_matcher:strong_matcher.lib",
        "//filters/source/extensions/filters/http/common/utility:utility_lib",
        "@envoy//envoy/upstream:cluster_manager_interface",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
    ],
)

srhino_cc_library(
    name = "super_glue_server_lib",
    srcs = [
        "super_glue_server.cc",
    ],
    hdrs = [
        "common.h",
        "super_glue_server.h",
    ],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/super_glue/v3:pkg_cc_proto",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
    ],
)

srhino_cc_library(
    name = "client_config",
    srcs = ["client_config.cc"],
    hdrs = ["client_config.h"],
    repository = "@envoy",
    deps = [
        ":super_glue_client_lib",
        "//filters/api/envoy/extensions/filters/http/super_glue/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)

srhino_cc_library(
    name = "server_config",
    srcs = ["server_config.cc"],
    hdrs = ["server_config.h"],
    repository = "@envoy",
    deps = [
        ":super_glue_server_lib",
        "//filters/api/envoy/extensions/filters/http/super_glue/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",

    ],
)
