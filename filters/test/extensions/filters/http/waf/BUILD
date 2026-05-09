load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_test",
)

# srhino_cc_test(
#     name = "regex_test",
#     srcs = ["regex_test.cc"],
#     repository = "@envoy",
#     deps = [
#         "//filters/source/extensions/filters/http/response_rewrite/regex:regex_lib",
#         # "@envoy//test/mocks/server:server_mocks",
#         "@envoy//test/test_common:utility_lib",
#     ]
# )

# srhino_cc_test(
#     name = "rule_test",
#     srcs = ["rewriter_test.cc", "rule_test.cc"],
#     repository = "@envoy",
#     deps = [
#         "//filters/api/envoy/extensions/filters/http/response_rewrite/v3:pkg_cc_proto",
#         "//filters/source/extensions/filters/http/response_rewrite:response_rewrite_lib",
#         "//filters/source/extensions/filters/http/response_rewrite:config",
#         "@envoy//source/common/http:header_map_lib",
#         "@envoy//source/common/stats:stats_lib",
#         "@envoy//test/mocks/http:http_mocks",
#         "@envoy//test/mocks/init:init_mocks",
#         "@envoy//test/mocks/local_info:local_info_mocks",
#         "@envoy//test/mocks/protobuf:protobuf_mocks",
#         "@envoy//test/mocks/thread_local:thread_local_mocks",
#         "@envoy//test/mocks/upstream:upstream_mocks",
#         "@envoy//test/test_common:test_runtime_lib",
#         "@envoy//test/mocks/server:server_mocks",
#         "@envoy//envoy/registry",
#     ],
# )

srhino_cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/waf/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/waf:waf_lib",
        "//filters/source/extensions/filters/http/waf:config",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/stats:stats_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/init:init_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/protobuf:protobuf_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:test_runtime_lib",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/mocks/server:instance_mocks",
        "@envoy//test/test_common:utility_lib",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//envoy/registry",
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

srhino_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//filters/api/envoy/extensions/filters/http/waf/v3:pkg_cc_proto",
        "//filters/source/extensions/filters/http/waf:config",
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
