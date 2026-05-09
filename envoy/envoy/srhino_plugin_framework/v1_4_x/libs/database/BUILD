load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "database_interface",
    hdrs = [
        "database.h",
        "central_database.h",
        "sqlite_database.h",
        "sqlite_db_factory.h",
        "ip_region_search.h",

    ],
    deps = [
    ],
)
