load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "file_name_matcher_lib",
    srcs = [
        "file_name_matcher.cc",
    ],
    hdrs = [
        "file_name_matcher.h",
    ],
    deps = [
    ],
)

envoy_cc_library(
    name = "multi_version_helpe_lib",
    hdrs = [
        "multi_version_helper.h",
    ],
    deps = [
        ":file_name_matcher_lib",
        "//source/common/srhino_plugin_framework/v1_0_x/libs:libs_lib",
        "//source/common/srhino_plugin_framework/v1_1_x/libs:libs_lib",
        "//source/common/srhino_plugin_framework/v1_2_x/libs:libs_lib",
        "//source/common/srhino_plugin_framework/v1_3_x/libs:libs_lib",
        "//source/common/srhino_plugin_framework/v1_4_x/libs:libs_lib",
    ],
)

envoy_cc_library(
    name = "plugin_file_info_lib",
    hdrs = [
        "plugin_file_info.h",
    ],
    deps = [
    ],
)

envoy_cc_library(
    name = "plugin_file_lib",
    srcs = [
        "plugin_file.cc",
        "plugin_file_manager.cc",
    ],
    hdrs = [
        "plugin_file.h",
        "plugin_file_manager.h",
    ],
    deps = [
        ":multi_version_helpe_lib",
        ":plugin_file_info_lib",
        "//envoy/srhino_plugin_framework/v1_0_x:srhino_plugin_framework_interface",
        "//envoy/srhino_plugin_framework/v1_1_x:srhino_plugin_framework_interface",
        "//envoy/srhino_plugin_framework/v1_2_x:srhino_plugin_framework_interface",
        "//envoy/srhino_plugin_framework/v1_3_x:srhino_plugin_framework_interface",
        "//envoy/srhino_plugin_framework/v1_4_x:srhino_plugin_framework_interface",
        "//source/common/common:assert_lib",
        "//source/common/common:minimal_logger_lib",
        "//source/common/common:thread_lib",
        "//source/common/srhino_plugin_framework/v1_0_x:manifest_lib",
        "//source/common/srhino_plugin_framework/v1_1_x:manifest_lib",
        "//source/common/srhino_plugin_framework/v1_2_x:manifest_lib",
        "//source/common/srhino_plugin_framework/v1_3_x:manifest_lib",
        "//source/common/srhino_plugin_framework/v1_4_x:manifest_lib",
        "//source/common/srhino_plugin_framework/v1_0_x/libs:libs_lib",
        "//source/common/srhino_plugin_framework/v1_1_x/libs:libs_lib",
        "//source/common/srhino_plugin_framework/v1_2_x/libs:libs_lib",
        "//source/common/srhino_plugin_framework/v1_3_x/libs:libs_lib",
        "//source/common/srhino_plugin_framework/v1_4_x/libs:libs_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = [
        "config_impl.cc",
    ],
    hdrs = [
        "config_impl.h",
    ],
    deps = [
        "//envoy/router:router_interface",
        "//envoy/srhino_plugin_framework:config_interface",
        "//envoy/srhino_plugin_framework/v1_0_x:srhino_plugin_framework_interface",
        "//source/common/protobuf",
    ],
)
