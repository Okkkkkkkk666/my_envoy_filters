
load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)

load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

srhino_cc_test(
    name = "sensitive_test",
    srcs = ["sensitive_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/sensitive_detect/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/sensitive_detect:sensitive_detect_lib",
        "//filters/source/extensions/filters/http/sensitive_detect:config",
        # "@envoy//source/extensions/compression/gzip/compressor:config",
        # "@envoy//source/extensions/compression/brotli/decompressor:config",
        # "@envoy//source/extensions/compression/gzip/decompressor:config",
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
        "@envoy//envoy/compression/decompressor:decompressor_config_interface",
        "@envoy//envoy/compression/decompressor:decompressor_interface",
        "@envoy//test/mocks/compression/decompressor:decompressor_mocks",
        "@envoy//envoy/compression/compressor:compressor_config_interface",
        "@envoy//envoy/compression/compressor:compressor_interface",
        "@envoy//test/mocks/compression/compressor:compressor_mocks",

    ],
)

api_proto_package(
    srcs = ["mock_compressor_library.proto"],
)

srhino_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        ":pkg_cc_proto",
        "//filters/api/envoy/extensions/filters/http/sensitive_detect/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/sensitive_detect:config",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/mocks/server:instance_mocks",
        "@envoy//test/test_common:utility_lib",
        "@envoy//test/mocks/server:server_mocks",

        "@envoy//envoy/compression/decompressor:decompressor_config_interface",
        "@envoy//envoy/compression/decompressor:decompressor_interface",
        "@envoy//envoy/compression/compressor:compressor_config_interface",
        "@envoy//envoy/compression/compressor:compressor_interface",


        "@envoy//test/integration:base_integration_test_lib",
        # "@envoy//source/extensions/compression/gzip/decompressor:config",
        # "@envoy//source/extensions/compression/brotli/decompressor:config",

        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/integration/filters:encoder_decoder_buffer_filter_lib",
        "@envoy//test/test_common:simulated_time_system_lib",
        "@envoy_api//envoy/extensions/compression/gzip/decompressor/v3:pkg_cc_proto",
        "@envoy_api//envoy/extensions/filters/http/decompressor/v3:pkg_cc_proto",
        "@envoy//test/mocks/compression/decompressor:decompressor_mocks",
        "@envoy//test/mocks/compression/compressor:compressor_mocks",

    ],
)