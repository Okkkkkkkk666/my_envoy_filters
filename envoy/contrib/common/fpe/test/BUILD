load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_contrib_package",
)

licenses(["notice"])  # Apache 2

envoy_contrib_package()

envoy_cc_test(
    name = "fpe_test",
    srcs = ["fpe_test.cc"],
    repository = "@envoy",
    linkopts = ["-lcrypto"],
    deps = [
        "//contrib/common/fpe:fpe_lib", # 依赖你之前写的业务 FPE 库
    ],
)