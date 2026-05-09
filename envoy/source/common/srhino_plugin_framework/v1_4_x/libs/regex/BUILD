load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "regex_lib",
    srcs = [
        "huge.cc",
        "hyperscan_engine.cc",
        "regex_impl.cc",
        "regex_matcher_impl.cc",
        "regex_replacer_impl.cc",
        "pcre.cc",
        "constraint.cc",
        "database.cc",
        "hs_base.cc",
        "scanner.cc"
    ],
    hdrs = [
        "huge.h",
        "hyperscan_engine.h",
        "regex_impl.h",
        "regex_matcher_impl.h",
        "regex_replacer_impl.h",
        "regex_utility.hpp",
        "pcre.h",
        "likely.h",
        "constraint.h",
        "database.h",
        "hs_base.h",
        "scanner.h"    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_4_x/libs/regex:regex_interface",
        "//source/common/srhino_plugin_framework/v1_4_x:srhino_plugin_framework_lib",
        "@envoy_filters//third-party/pcre2:pcre2",
    ] + select({
        "@envoy//bazel:linux_x86_64": ["@envoy_filters//third-party:hyperscan_lib"],
        "@envoy//bazel:linux_aarch64": ["@envoy_filters//third-party:hyperscan_aarch64_lib"],
        "//conditions:default": ["@envoy_filters//third-party:hyperscan_lib"],
    }),
)


cc_binary(
    name = "regex.so",
    srcs = [
        "huge.cc",
        "hyperscan_engine.cc",
        "regex_impl.cc",
        "regex_matcher_impl.cc",
        "regex_replacer_impl.cc",
        "regex_exported.cc",
        "huge.h",
        "likely.h",
        "pcre.h",
        "pcre.cc",
        "hyperscan_engine.h",
        "regex_impl.h",
        "regex_matcher_impl.h",
        "regex_replacer_impl.h",
        "regex_utility.hpp",
        "constraint.h",
        "constraint.cc",
        "database.h",
        "database.cc",
        "hs_base.h",
        "hs_base.cc",
        "scanner.h",
        "scanner.cc",
    ],
    copts = [
        "-fPIC",
        "-fvisibility=hidden"
    ],
    linkopts = [
        "-lrt",
        "-Wl,--exclude-libs,ALL",
    ],
    linkshared = True,
    visibility = ["//visibility:public"],
    deps = [
        "//envoy/srhino_plugin_framework/v1_4_x/libs/regex:regex_interface",
        "//source/common/srhino_plugin_framework/v1_4_x:srhino_plugin_framework_lib",
        "@envoy_filters//third-party/pcre2:pcre2",
    ] + select({
        "@envoy//bazel:linux_x86_64": ["@envoy_filters//third-party:hyperscan_lib"],
        "@envoy//bazel:linux_aarch64": ["@envoy_filters//third-party:hyperscan_aarch64_lib"],
        "//conditions:default": ["@envoy_filters//third-party:hyperscan_lib"],
    }),
)
