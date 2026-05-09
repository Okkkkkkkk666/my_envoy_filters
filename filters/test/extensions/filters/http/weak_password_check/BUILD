load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)

srhino_cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/weak_password_check/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/weak_password_check:weak_password_check_lib",
        "//filters/source/extensions/filters/http/weak_password_check:config",
        "@envoy//source/common/network:address_lib",
        "@envoy//test/common/stream_info:test_util",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:cluster_info_mocks",
    ],
    copts=["-Wno-old-style-cast"],
)

srhino_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/source/extensions/filters/http/weak_password_check:config",
        "@envoy//test/mocks/server:server_mocks",
    ],
    copts=["-Wno-old-style-cast"],
)
