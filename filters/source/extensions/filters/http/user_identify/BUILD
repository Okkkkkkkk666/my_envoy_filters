load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "user_identify_list",
    srcs = glob(["impl/url.txt",
                 "impl/username.txt",
                 "impl/token.txt",
                 "impl/guest.txt"])
)

srhino_cc_library(
    name = "user_identify_lib",
    srcs = ["user_identify.cc","impl/rule.cc","impl/data_interactive.cc"],
    hdrs = ["user_identify.h","impl/rule.h","impl/data_interactive.h"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/user_identify/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/central_database:central_database.lib",
        "//filters/source/extensions/filters/http/common:common_lib",
        "//filters/source/extensions/filters/http/common/regex_matcher:regex_matcher_lib",
        "//filters/source/extensions/filters/http/common/utility:utility_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/common/grpc:typed_async_client_lib",
        "@envoy//source/common/srhino_plugin_framework/v1_0_x:srhino_plugin_framework_lib",

    ],
)

srhino_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":user_identify_lib",
        "//filters/api/envoy/extensions/filters/http/user_identify/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)