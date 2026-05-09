load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "anti_cc_lib",
    srcs = [
        "anti_cc.cc",
        "impl/action.cc",
    ],
    hdrs = [
        "anti_cc.h",
        "impl/action.h",

    ],
    external_deps = [
        "rapidjson",
    ],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/anti_cc/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common:common_lib",
        "//filters/source/extensions/filters/http/common/ratelimit:ratelimit.lib",
        "//filters/source/extensions/filters/http/common/ip_whitelist:ip_whitelist.lib",
        "//filters/source/extensions/filters/http/common/central_database:central_database.lib",
        "@envoy//source/common/grpc:typed_async_client_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
    ],
)

srhino_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":anti_cc_lib",
        "//filters/api/envoy/extensions/filters/http/anti_cc/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/ratelimit:ratelimit.lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
