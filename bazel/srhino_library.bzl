# load("@rules_python//python:defs.bzl", "py_binary", "py_test")
load("@rules_cc//cc:defs.bzl", "cc_library")
# load("@rules_fuzzing//fuzzing:cc_defs.bzl", "fuzzing_decoration")
# load("@envoy//bazel:envoy_binary.bzl", "envoy_cc_binary")
load("@envoy//bazel:envoy_library.bzl", "tcmalloc_external_deps")
load("@envoy//bazel:envoy_pch.bzl", "envoy_pch_copts")
load(
    "@envoy//bazel:envoy_internal.bzl",
    "envoy_copts",
    "envoy_external_dep_path",
    "envoy_linkstatic",
    # "envoy_select_force_libcpp",
    # "envoy_stdlib_deps",
    # "tcmalloc_external_dep",
)
# load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
# bool_flag(
#     name = "config_build_coverage_flag",
#     build_setting_default = False,
# )
# load("@bazel_skylib//rules:common_settings.bzl", "config_setting")
# Envoy C++ library targets should be specified with this function.
# config_setting(
#     name = "config_build_coverage",
#     values = {"define": "build_coverage=true"},
# )

def srhino_cc_library(
        name,
        srcs = [],
        hdrs = [],
        copts = [],
        visibility = None,
        external_deps = [],
        tcmalloc_dep = None,
        repository = "",
        tags = [],
        deps = [],
        strip_include_prefix = None,
        include_prefix = None,
        textual_hdrs = None,
        defines = []):
    # coverage_copts = list(copts)
    # if "-fprofile-arcs" not in copts:
    #     coverage_copts.append("-fprofile-arcs")
    # if "-ftest-coverage" not in copts:
    #     coverage_copts.append("-ftest-coverage")

    # coverage_tags = list(tags)
    # if "no-sandbox" not in tags:
    #     coverage_tags.append("no-sandbox")
    native.config_setting(
        name = name+"-config_build_coverage_enable",
        values = {"define": "build_coverage=true"},
        visibility = ["//visibility:private"],
    )
    coverage_copts = copts + select({":" +name+ "-config_build_coverage_enable": ["-fprofile-arcs","-ftest-coverage"], "//conditions:default": []})
    # print("coverage_copts info:",coverage_copts)
    coverage_tags = tags + ["no-sandbox"]  
    # print("coverage_tags info:",coverage_tags)

    if tcmalloc_dep:
        deps += tcmalloc_external_deps(repository)

    cc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        copts = envoy_copts(repository) + envoy_pch_copts(repository, "//source/common/common:common_pch") + coverage_copts,
        visibility = visibility,
        tags = coverage_tags,
        textual_hdrs = textual_hdrs,
        deps = deps + [envoy_external_dep_path(dep) for dep in external_deps] + [
            repository + "//envoy/common:base_includes",
            repository + "//source/common/common:fmt_lib",
            repository + "//source/common/common:common_pch",
            envoy_external_dep_path("abseil_flat_hash_map"),
            envoy_external_dep_path("abseil_flat_hash_set"),
            envoy_external_dep_path("abseil_strings"),
            envoy_external_dep_path("fmtlib"),
        ],
        alwayslink = 1,
        linkstatic = envoy_linkstatic(),
        strip_include_prefix = strip_include_prefix,
        include_prefix = include_prefix,
        defines = defines,
    )

    # Intended for usage by external consumers. This allows them to disambiguate
    # include paths via `external/envoy...`
    cc_library(
        name = name + "_with_external_headers",
        hdrs = hdrs,
        copts = envoy_copts(repository) + coverage_copts,
        visibility = visibility,
        tags = ["nocompdb"] + coverage_tags,
        deps = [":" + name],
        strip_include_prefix = strip_include_prefix,
        include_prefix = include_prefix,
    )



