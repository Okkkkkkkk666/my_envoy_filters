load(
    "//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "regex_matcher_test",
    srcs = ["regex_matcher_test.cc"],
    deps = [
        "//source/common/srhino_plugin_framework/v1_3_x/libs/regex:regex_lib",
    ],
)

envoy_cc_test(
    name = "regex_replacer_test",
    srcs = ["regex_replacer_test.cc"],
    deps = [
        "//source/common/srhino_plugin_framework/v1_3_x/libs/regex:regex_lib",
    ],
)
