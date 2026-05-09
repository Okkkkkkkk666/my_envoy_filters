load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "filesystem_interface",
    hdrs = [
        "filesystem.h",
        "ini_file.h",
        "json_file.h",
        "plain_file.h",
        "xml_file.h",
        "yaml_file.h",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_2_x:srhino_plugin_framework_interface",
    ],
)
