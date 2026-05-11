load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_contrib_extension",
    "envoy_cc_library",
    "envoy_contrib_package",
)

licenses(["notice"])  # Apache 2

envoy_contrib_package()

cc_library(
    name = "mimetic_sys_lib",
    linkopts = ["-lmimetic"],  # 这里的 -lmimetic 会直接传给底层的链接器 (ld)
    visibility = ["//visibility:public"],
)

envoy_cc_library(
    name = "filter_lib",
    srcs = [
        "smtp_decoder.cc",
        "smtp_filter.cc",
    ],
    hdrs = [
        "smtp_decoder.h",
        "smtp_filter.h",
        "smtp_session.h",
    ],
    copts = ["-fexceptions"],
    deps = [
        "//envoy/network:connection_interface",
        "//envoy/network:filter_interface",
        "//envoy/stats:stats_interface",
        "//source/common/buffer:buffer_lib",
        "//source/common/common:logger_lib",
        "//source/common/network:filter_lib",
        "@envoy//source/common/event:dispatcher_lib",
        ":mimetic_sys_lib",
    ],
)

envoy_cc_contrib_extension(
    name = "config",
    srcs = ["smtp_config.cc"],
    hdrs = ["smtp_config.h"],
    deps = [
        ":filter_lib",
        "//envoy/registry",
        "//source/extensions/filters/network/common:factory_base_lib",
        "@envoy_api//contrib/envoy/extensions/filters/network/smtp_proxy/v3:pkg_cc_proto",
    ],
)
