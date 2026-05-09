load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)

srhino_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/api_check/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/api_check:config",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/mocks/server:instance_mocks",
        "@envoy//test/test_common:utility_lib",
        "@envoy//test/mocks/server:server_mocks",
    ],
    copts = [
        "-Wno-unused-function",
        "-Wno-unused-parameter",
        "-Wno-reorder",
        "-Wno-unused-variable",
        "-Wno-sign-compare",
        "-Wno-non-virtual-dtor",
        "-Wno-overloaded-virtual",
        "-Ithird-party/ModSecurity/headers",
    ],
    # data = [
    #     "//plugins/waf/conf:rules"
    # ]
)
