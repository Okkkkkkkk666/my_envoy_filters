load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)

srhino_cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/strong_local_ratelimit:strong_local_ratelimit_lib",
        "//filters/source/extensions/filters/http/strong_local_ratelimit:config",
        "@envoy//source/common/network:address_lib",
        "@envoy//source/common/json:json_loader_lib",
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
        "//filters/source/extensions/filters/http/strong_local_ratelimit:config",
        "@envoy//test/mocks/server:server_mocks",
    ],
)

srhino_cc_test(
    name = "quota_test",
    srcs = ["impl/quota_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/source/extensions/filters/http/strong_local_ratelimit:strong_local_ratelimit_lib",
    ],
)

srhino_cc_test(
    name = "action_test",
    srcs = ["impl/action_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/strong_local_ratelimit:strong_local_ratelimit_lib",
        "@envoy//source/common/network:address_lib",
        "@envoy//test/common/stream_info:test_util",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
    ],
)

srhino_cc_test(
    name = "hash_table_test",
    srcs = ["impl/hash_table_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/source/extensions/filters/http/strong_local_ratelimit:strong_local_ratelimit_lib",
    ],
)

srhino_cc_test(
    name = "lru_test",
    srcs = ["impl/lru_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/source/extensions/filters/http/strong_local_ratelimit:strong_local_ratelimit_lib",
    ],
)