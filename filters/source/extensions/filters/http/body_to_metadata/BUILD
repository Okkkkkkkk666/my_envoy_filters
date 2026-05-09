# load(
#     "@envoy//bazel:envoy_build_system.bzl",
#     "envoy_cc_library",
# )
load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)
package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "body_to_metadata_lib",
    repository = "@envoy",
    srcs = ["filter.cc"],
    hdrs = ["filter.h"],
    deps = [
        "//filters/api/envoy/extensions/filters/http/body_to_metadata/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/strong_matcher:strong_matcher.lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
    ],
)

srhino_cc_library(
    name = "config",
    repository = "@envoy",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":body_to_metadata_lib",
        "//filters/api/envoy/extensions/filters/http/body_to_metadata/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
