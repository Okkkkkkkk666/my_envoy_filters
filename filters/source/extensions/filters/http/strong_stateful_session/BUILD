load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "strong_stateful_session_lib",
    srcs = [
        "filter.cc",
        "impl/cookie.cc",
        "impl/http_header.cc",
        "impl/source_ip.cc",
        "impl/ssl_session.cc",
        "impl/stateful_session.cc",
    ],
    hdrs = [
        "filter.h",
        "impl/cookie.h",
        "impl/http_header.h",
        "impl/source_ip.h",
        "impl/ssl_session.h",
        "impl/stateful_session.h",
    ],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_stateful_session/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/central_database:central_database.lib",
        "//filters/source/extensions/filters/http/common:common_lib",
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
        ":strong_stateful_session_lib",
        "//filters/api/envoy/extensions/filters/http/strong_stateful_session/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
