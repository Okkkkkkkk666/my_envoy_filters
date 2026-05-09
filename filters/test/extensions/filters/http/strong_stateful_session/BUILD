
load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)

srhino_cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_stateful_session/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/strong_stateful_session:strong_stateful_session_lib",
        "//filters/source/extensions/filters/http/strong_stateful_session:config",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/stats:stats_lib",
        "@envoy//source/common/json:json_loader_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/init:init_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/protobuf:protobuf_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:test_runtime_lib",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/mocks/server:instance_mocks",
        "@envoy//test/test_common:utility_lib",
        "@envoy//envoy/registry",
    ],
)

srhino_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/strong_stateful_session/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/strong_stateful_session:config",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/mocks/server:instance_mocks",
        "@envoy//test/test_common:utility_lib",
        "@envoy//test/mocks/server:server_mocks",
    ],
)