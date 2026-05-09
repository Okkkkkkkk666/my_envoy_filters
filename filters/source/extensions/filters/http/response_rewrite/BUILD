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
    name = "response_rewrite_lib",
    repository = "@envoy",
    srcs = ["response_rewrite.cc"],
    hdrs = ["response_rewrite.h"],
    deps = [
        "//third-party:cryptopp_lib",
        "//filters/api/envoy/extensions/filters/http/response_rewrite/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/regex_matcher:regex_matcher_lib",
        "//filters/source/extensions/filters/http/common/strong_matcher:strong_matcher.lib",
        "//filters/source/extensions/filters/http/common/ip_whitelist:ip_whitelist.lib",
        "//filters/source/extensions/filters/http/common/algorithm:algorithm.lib",
        "//filters/source/extensions/filters/http/common/utility:utility_lib",
        "@envoy//envoy/compression/decompressor:decompressor_config_interface",
        "@envoy//envoy/compression/decompressor:decompressor_interface",
        "@envoy//envoy/compression/compressor:compressor_config_interface",
        "@envoy//envoy/compression/compressor:compressor_interface",
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
    copts=[
        "-Wno-old-style-cast",
        "-Ithird-party/Cryptopp",
   ],
)

srhino_cc_library(
    name = "config",
    repository = "@envoy",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":response_rewrite_lib",
        "//third-party:cryptopp_lib",
        "//filters/api/envoy/extensions/filters/http/response_rewrite/v3:pkg_cc_proto",
        "@envoy//envoy/registry",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
    copts=[
        "-Wno-old-style-cast",
        "-Ithird-party/Cryptopp",
   ],
)
