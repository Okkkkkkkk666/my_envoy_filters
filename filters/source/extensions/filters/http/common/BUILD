load(
    "//bazel:srhino_build_system.bzl",
    "srhino_cc_library",
)

package(default_visibility = ["//visibility:public"])

srhino_cc_library(
    name = "common_lib",
    hdrs = [
        "hash_table.hpp",
        "lru_cache.hpp",
    ],
    repository = "@envoy",
)
