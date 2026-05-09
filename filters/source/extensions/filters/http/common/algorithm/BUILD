load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)
package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "algorithm.lib",
    repository = "@envoy",
    srcs = ["shuffle.cc","transform.cc","hash_encrypt.cc","encrypt_algorithm.cc","cover.cc","replace.cc"],
    hdrs = ["shuffle.h","transform.h","hash_encrypt.h","encrypt_algorithm.h","cover.h","replace.h" ],
    deps = [
        "//third-party:cryptopp_lib",
        "//filters/api/envoy/extensions/filters/http/common/algorithm/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/regex_matcher:regex_matcher_lib",
        "//filters/source/extensions/filters/http/common/replace:replace_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/router:config_lib",
    ],
    copts=[
         "-Wno-old-style-cast",
         "-Ithird-party/Cryptopp",
    ],
)