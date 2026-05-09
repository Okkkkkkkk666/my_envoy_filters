load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "filter_status_lib",
    srcs = [
        "status_converter.cc",
    ],
    hdrs = [
        "filter_status.h",
        "status_converter.h",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/srhino_plugin_framework:plugin_file_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
    ],
)
