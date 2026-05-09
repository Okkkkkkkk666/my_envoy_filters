load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "regex_matcher_lib",
    srcs = [
        "engine.cc",
        "huge.cc",
        "regex_matcher.cc",
    ],
    hdrs = [
        "common.h",
        "engine.h",
        "huge.h",
        "regex_matcher.h",
    ],
    repository = "@envoy",
    deps = [
        "@com_googlesource_code_re2//:re2",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
    ] + select({
        "@envoy//bazel:linux_x86_64": ["//third-party:hyperscan_lib"],
        "@envoy//bazel:linux_aarch64": ["//third-party:hyperscan_aarch64_lib"],
        "//conditions:default": ["//third-party:hyperscan_lib"],
    }),
)
