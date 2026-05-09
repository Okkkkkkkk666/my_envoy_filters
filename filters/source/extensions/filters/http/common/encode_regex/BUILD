load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)
package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "encode_regex_lib",
    repository = "@envoy",
    srcs = ["encoding_converter.cc"],
    hdrs = ["encoding_converter.h"],
    deps = [
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@com_googlesource_code_re2//:re2",
    ],
)
