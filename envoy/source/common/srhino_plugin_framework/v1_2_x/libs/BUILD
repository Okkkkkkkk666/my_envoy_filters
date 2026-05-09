load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "libs_lib",
    srcs = [
        "factory_impl.cc",
    ],
    hdrs = [
        "factory_impl.h",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_2_x/libs:libs_interface",
        "//envoy/srhino_plugin_framework/v1_2_x/utility:utility_interface",
        "//source/common/common:empty_string",
        "//source/common/protobuf",
        "//source/common/srhino_plugin_framework/v1_2_x:manifest_lib",
        "//source/common/srhino_plugin_framework/v1_2_x/libs/config:config_lib",
        "//source/common/srhino_plugin_framework/v1_2_x/libs/encode:encode_lib",
        "//source/common/srhino_plugin_framework/v1_2_x/libs/filesystem:filesystem_lib",
        "//source/common/srhino_plugin_framework/v1_2_x/libs/net:net_lib",
        "//source/common/srhino_plugin_framework/v1_2_x/libs/regex:regex_lib",
        "//source/common/srhino_plugin_framework/v1_2_x/libs/database:database_lib",
        "//source/common/srhino_plugin_framework/v1_2_x/libs/thread_local:thread_local_lib",
    ],
)
