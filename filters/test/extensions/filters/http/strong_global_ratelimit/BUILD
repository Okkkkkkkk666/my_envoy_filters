load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)

srhino_cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/strong_global_ratelimit:strong_global_ratelimit_lib",
        "//filters/source/extensions/filters/http/strong_global_ratelimit:config",
        "//filters/test/extensions/filters/http/common/ratelimit:ratelimit_client_mocks",
        "@envoy//source/common/network:address_lib",
        "@envoy//test/common/stream_info:test_util",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:cluster_info_mocks",
    ],
)

srhino_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/source/extensions/filters/http/strong_global_ratelimit:config",
        "//filters/test/extensions/filters/http/common/ratelimit:ratelimit_test",
        "@envoy//test/mocks/server:server_mocks",
    ],
)

srhino_cc_test(
    name = "action_test",
    srcs = ["impl/action_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/strong_global_ratelimit:strong_global_ratelimit_lib",
        "@envoy//source/common/network:address_lib",
        "@envoy//test/common/stream_info:test_util",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
    ],
)

srhino_cc_test(
    name = "rule_test",
    srcs = ["impl/rule_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/strong_global_ratelimit:strong_global_ratelimit_lib",
        "@envoy//source/common/network:address_lib",
        "@envoy//test/common/stream_info:test_util",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
    ],
)
