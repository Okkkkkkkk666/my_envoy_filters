load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "filesystem_lib",
    srcs = [
        "filesystem_impl.cc",
        "ini_file_impl.cc",
        "json_file_impl.cc",
        "plain_file_impl.cc",
        "xml_file_impl.cc",
        "yaml_file_impl.cc",
    ],
    hdrs = [
        "filesystem_impl.h",
        "ini_file_impl.h",
        "json_file_impl.h",
        "plain_file_impl.h",
        "xml_file_impl.h",
        "yaml_file_impl.h",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_1_x/libs/filesystem:filesystem_interface",
        "//source/common/srhino_plugin_framework/v1_1_x:manifest_lib",
        "//source/common/srhino_plugin_framework/v1_1_x:srhino_plugin_framework_lib",
    ],
)
