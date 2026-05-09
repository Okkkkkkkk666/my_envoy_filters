load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "bot_list",
    srcs = glob(["impl/bot_regex_list.txt"])
)

srhino_cc_library(
    name = "bot_detection_lib",
    repository = "@envoy",
    srcs = ["bot_detection.cc","impl/matcher.cc",],
    hdrs = ["bot_detection.h","impl/matcher.h"],
    deps = [
        "//filters/api/envoy/extensions/filters/http/bot_detection/v3:pkg_cc_proto",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
    ],
)

srhino_cc_library(
    name = "config",
    repository = "@envoy",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":bot_detection_lib",
        "//filters/api/envoy/extensions/filters/http/bot_detection/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)