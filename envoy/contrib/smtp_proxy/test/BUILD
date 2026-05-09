load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_contrib_package",
)

licenses(["notice"])  # Apache 2

envoy_contrib_package()

envoy_cc_test(
    name = "smtp_decoder_test",
    srcs = ["smtp_decoder_test.cc"],
    deps = [
        "//contrib/smtp_proxy:filter_lib",
    ],
)

envoy_cc_test(
    name = "smtp_config_test",
    srcs = ["smtp_config_test.cc"],
    deps = [
        "//contrib/smtp_proxy:config",
        "//test/mocks/network:network_mocks",
        "//test/mocks/server:factory_context_mocks",
    ],
)

envoy_cc_test(
    name = "smtp_filter_state_test",
    srcs = ["smtp_filter_state_test.cc"],
    deps = [
        "//contrib/smtp_proxy:filter_lib",
        "//test/mocks/network:network_mocks",
    ],
)
