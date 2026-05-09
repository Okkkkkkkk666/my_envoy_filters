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
    name = "sqlite_database_test",
    srcs = ["sqlite_database_test.cc"],
    deps = [
        "//source/common/srhino_plugin_framework/v1_1_x/libs/database:database_lib",
    ],
)