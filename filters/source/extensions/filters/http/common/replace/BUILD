load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)
package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "replace_lib",
    repository = "@envoy",
    srcs = ["string_utils.cc","shuffle.cc","transform.cc","hash_encrypt.cc","encrypt_algorithm.cc","cover.cc","replace.cc", "identify_rule_interface.cc", "identify_rule.cc", "rewrite_rule_interface.cc", "rewrite_rule.cc"],
    hdrs = ["string_utils.h","shuffle.h","transform.h","hash_encrypt.h","encrypt_algorithm.h","cover.h","replace.h", "identify_rule_interface.h", "identify_rule.h", "rewrite_rule_interface.h", "rewrite_rule.h" ],
    deps = [
        "//third-party:cryptopp_lib",
        "//filters/api/envoy/extensions/filters/http/common/algorithm/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/regex_matcher:regex_matcher_lib",
        "@envoy//source/extensions/access_loggers/grpc:filter_access_log_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/router:config_lib",
    ],
    copts=[
         "-Wno-old-style-cast",
         "-Ithird-party/Cryptopp",
    ],
)

cc_binary(
    name = "libreplace.so",
    srcs = [
        "cover.cc",
        "encrypt_algorithm.cc",
        "hash_encrypt.cc",
        "replace.cc",
        "rewrite_rule_interface.cc",
        "rewrite_rule.cc",
        "shuffle.cc",
        "string_utils.cc",
        "transform.cc",
        "cover.h",
        "encrypt_algorithm.h",
        "hash_encrypt.h",
        "replace.h",
        "rewrite_rule_interface.h",
        "rewrite_rule.h",
        "shuffle.h",
        "string_utils.h",
        "transform.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
      "//third-party:cryptopp_lib",
    ],
    linkshared = True,
)
