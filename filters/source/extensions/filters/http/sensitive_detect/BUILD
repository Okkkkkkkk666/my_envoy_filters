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
    name = "sensitive_detect_lib",
    repository = "@envoy",
    srcs = ["sensitive_detect.cc"],
    hdrs = ["sensitive_detect.h"],
    deps = [
        "//filters/api/envoy/extensions/filters/http/sensitive_detect/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/regex_matcher:regex_matcher_lib",
        "@envoy//envoy/compression/decompressor:decompressor_config_interface",
        "@envoy//envoy/compression/decompressor:decompressor_interface",
        # "@envoy//envoy/event:dispatcher_interface",
        "@envoy//envoy/http:codes_interface",
        "@envoy//envoy/http:filter_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:enum_to_int",
        "@envoy//source/common/http:codes_lib",
        "@envoy//source/common/http:header_map_lib",
        # "@envoy//source/common/runtime:runtime_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
        "@envoy//source/common/router:config_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
    ],
)

srhino_cc_library(
    name = "config",
    repository = "@envoy",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":sensitive_detect_lib",
        "//filters/api/envoy/extensions/filters/http/sensitive_detect/v3:pkg_cc_proto",
        "@envoy//envoy/compression/decompressor:decompressor_config_interface",
        "@envoy//envoy/compression/decompressor:decompressor_interface",
        "@envoy//envoy/registry",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
