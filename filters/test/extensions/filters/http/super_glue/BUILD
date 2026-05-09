load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)

srhino_cc_test(
    name = "client_filter_test",
    srcs = ["client_filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/super_glue/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/super_glue:super_glue_client_lib",
        "//filters/source/extensions/filters/http/super_glue:client_config",
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
    name = "client_config_test",
    srcs = ["client_config_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/source/extensions/filters/http/super_glue:client_config",
        "@envoy//test/mocks/server:server_mocks",
    ],
)

srhino_cc_test(
    name = "rule_test",
    srcs = ["rule_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/source/extensions/filters/http/super_glue:client_config",
        "@envoy//test/mocks/server:server_mocks",
    ],
)


srhino_cc_test(
    name = "server_config_test",
    srcs = ["server_config_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/source/extensions/filters/http/super_glue:server_config",
        "@envoy//test/mocks/server:server_mocks",
    ],
)

srhino_cc_test(
    name = "server_filter_test",
    srcs = ["server_filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/super_glue/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/super_glue:super_glue_server_lib",
        "//filters/source/extensions/filters/http/super_glue:server_config",
        "@envoy//source/common/network:address_lib",
        "@envoy//source/common/json:json_loader_lib",
        "@envoy//test/common/stream_info:test_util",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:cluster_info_mocks",
    ],
)

