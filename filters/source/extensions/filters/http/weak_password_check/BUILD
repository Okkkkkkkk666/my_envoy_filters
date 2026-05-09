load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "weak_password_check_lib",
    srcs = ["weak_password_check.cc","impl/rule.cc","impl/custom_password.cc", "impl/database.cc"],
    hdrs = ["weak_password_check.h","impl/rule.h","impl/custom_password.h", "impl/database.h"],
    repository = "@envoy",
    deps = [
        "//third-party:sqlite3_lib",
        "//third-party:cryptopp_lib",
        "//filters/api/envoy/extensions/filters/http/weak_password_check/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/regex_matcher:regex_matcher_lib",
        "//filters/source/extensions/filters/http/common/utility:utility_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_ex_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",

    ],
    copts=[
         "-Wno-old-style-cast",
         "-Ithird-party/Sqlite3",
         "-Ithird-party/Cryptopp",
    ],
)


srhino_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":weak_password_check_lib",
        "//filters/api/envoy/extensions/filters/http/weak_password_check/v3:pkg_cc_proto",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        
    ],
    copts=[
        "-Wno-old-style-cast",
        "-Ithird-party/Sqlite3",
        "-Ithird-party/Cryptopp",
    ],
)