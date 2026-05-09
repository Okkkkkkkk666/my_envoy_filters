load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "encode_lib",
    srcs = [
        "compressor_impl.cc",
        "decompressor_impl.cc",
        "encode_impl.cc",
        "cryptopp_impl.cc",
        "openssl_impl.cc",
    ],
    hdrs = [
        "compressor_impl.h",
        "decompressor_impl.h",
        "encode_impl.h",
        "cryptopp_impl.h",
        "openssl_impl.h",
    ],
    copts = [
        "-Wno-old-style-cast",
    ],
    deps = [
        "//envoy/srhino_plugin_framework/v1_4_x/libs/encode:encode_interface",
        "//source/common/srhino_plugin_framework/v1_4_x:srhino_plugin_framework_lib",
        "//source/common/stats:isolated_store_lib",
        "//source/extensions/compression/brotli/compressor:compressor_lib",
        "//source/extensions/compression/brotli/decompressor:decompressor_lib",
        "//source/extensions/compression/gzip/compressor:compressor_lib",
        "//source/extensions/compression/gzip/decompressor:zlib_decompressor_impl_lib",
        "@envoy_filters//third-party:cryptopp_lib",
        "@envoy_filters//third-party:Zstd_lib",
    ],
)

cc_binary(
    name = "cryptopp.so",
    srcs = [
        "cryptopp_exported.cc",
        "cryptopp_impl.cc",
        "cryptopp_impl.h",
    ],
    copts = [
        "-fPIC",
        "-fvisibility=hidden"
    ],
    linkopts = [
        "-lrt",
        "-Wl,--exclude-libs,ALL",
    ],
    linkshared = True,
    visibility = ["//visibility:public"],
    deps = [
        "//envoy/srhino_plugin_framework/v1_4_x/libs/encode:encode_interface",
        "@envoy_filters//third-party:cryptopp_lib",
    ],
)