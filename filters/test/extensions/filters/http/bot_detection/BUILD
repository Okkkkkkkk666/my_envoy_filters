load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)

srhino_cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/bot_detection/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/bot_detection:bot_detection_lib",
        "//filters/source/extensions/filters/http/bot_detection:config",
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
    data = ["//filters/source/extensions/filters/http/bot_detection:bot_list"],
    deps = [
        "//filters/source/extensions/filters/http/bot_detection:config",
        "@envoy//test/mocks/server:server_mocks",
    ],
)