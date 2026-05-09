load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "encode_interface",
    hdrs = [
        "decompressor.h",
        "compressor.h",
        "encode.h",
        "openssl.h",
        "cryptopp.h"
    ],
    deps = [
    ],
)
