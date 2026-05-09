load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "srhino_plugin_loader_lib",
    srcs = [
        "filter.cc",
    ],
    hdrs = [
        "filter.h",
        "multi_version_helper.h",
    ],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/srhino_plugin_loader/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common:common_lib",
        "//filters/source/extensions/filters/http/srhino_plugin_loader/v1_0_x:filter_status_lib",
        "//filters/source/extensions/filters/http/srhino_plugin_loader/v1_1_x:filter_status_lib",
        "//filters/source/extensions/filters/http/srhino_plugin_loader/v1_2_x:filter_status_lib",
        "//filters/source/extensions/filters/http/srhino_plugin_loader/v1_3_x:filter_status_lib",
        "//filters/source/extensions/filters/http/srhino_plugin_loader/v1_4_x:filter_status_lib",
        "@envoy//source/common/srhino_plugin_framework:plugin_file_lib",
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
        ":srhino_plugin_loader_lib",
        "//filters/api/envoy/extensions/filters/http/srhino_plugin_loader/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
