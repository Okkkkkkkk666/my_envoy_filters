load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_test(
    name = "ratelimit_test",
    srcs = ["impl/ratelimit_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/common/ratelimit/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/common/ratelimit:ratelimit.lib",
        ":ratelimit_client_mocks",
        "@envoy//test/mocks/grpc:grpc_mocks"
    ],
)

envoy_cc_mock(
    name = "ratelimit_client_mocks",
    srcs = ["mock_ratelimit_client.cc"],
    hdrs = ["mock_ratelimit_client.h"],
    repository = "@envoy",
    deps = [
        "//filters/source/extensions/filters/http/common/ratelimit:ratelimit.lib",
    ],
)
