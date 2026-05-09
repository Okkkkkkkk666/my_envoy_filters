# load("@rules_python//python:defs.bzl") #, "py_binary", "py_test"
load("@rules_cc//cc:defs.bzl", "cc_test") #, "cc_binary", "cc_library"
# load("@rules_fuzzing//fuzzing:cc_defs.bzl", "fuzzing_decoration")
# load("@envoy//bazel:envoy_binary.bzl", "envoy_cc_binary")
# load("@envoy//bazel:envoy_library.bzl", "tcmalloc_external_deps")
load("@envoy//bazel:envoy_pch.bzl", "envoy_pch_copts")
load(
    "@envoy//bazel:envoy_internal.bzl",
    "envoy_copts",
    "envoy_external_dep_path",
    "envoy_linkstatic",
    "envoy_select_force_libcpp",
    "envoy_stdlib_deps",
    "tcmalloc_external_dep",
)
# load("@bazel_skylib//rules:common_settings.bzl", "config_setting")
load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
# bool_flag(
#     name = "config_build_coverage_flag",
#     build_setting_default = False,
# )

def _envoy_test_linkopts():
    return select({
        "@envoy//bazel:apple": [],
        "@envoy//bazel:windows_x86_64": [
            "-DEFAULTLIB:ws2_32.lib",
            "-DEFAULTLIB:iphlpapi.lib",
            "-WX",
        ],

        # TODO(mattklein123): It's not great that we universally link against the following libs.
        # In particular, -latomic and -lrt are not needed on all platforms. Make this more granular.
        "//conditions:default": ["-pthread", "-lrt", "-ldl"],
    }) + envoy_select_force_libcpp([], ["-lstdc++fs", "-latomic"])


def srhino_cc_test(
        name,
        srcs = [],
        data = [],
        # List of pairs (Bazel shell script target, shell script args)
        repository = "",
        external_deps = [],
        deps = [],
        tags = [],
        args = [],
        copts = [],
        condition = None,
        shard_count = None,
        coverage = True,
        size = "medium",
        flaky = False,
        env = {}):
    # coverage_copts = list(copts)
    # if coverage:
    #     if "-fprofile-arcs" not in copts:
    #         coverage_copts.append("-fprofile-arcs")
    #         # coverage_copts.append("-fprofile-dir=/root")
    #     if "-ftest-coverage" not in copts:
    #         coverage_copts.append("-ftest-coverage")
            
    # coverage_linkopts = (["-lgcov"] if coverage else [])
    # coverage_tags = tags + ["no-sandbox"]

    native.config_setting(
        name = name+"-config_build_coverage_enable",
        values = {"define": "build_coverage=true"},
        visibility = ["//visibility:private"],
    )
    coverage_copts = copts + select({":" +name+ "-config_build_coverage_enable": ["-fprofile-arcs","-ftest-coverage"], "//conditions:default": []})
    # print("coverage_copts info:",coverage_copts)
    coverage_linkopts = select({":" +name+ "-config_build_coverage_enable": ["-lgcov"], "//conditions:default": []})
    # print("coverage_linkopts info:",coverage_linkopts)
    coverage_tags = tags + ["no-sandbox"] 
    # print("coverage_tags info:",coverage_tags)

    cc_test(
        name = name,
        srcs = srcs,
        data = data,
        copts = envoy_copts(repository, test = True) + coverage_copts + envoy_pch_copts(repository, "//test:test_pch"),
        linkopts = _envoy_test_linkopts() + coverage_linkopts,
        linkstatic = envoy_linkstatic(),
        malloc = tcmalloc_external_dep(repository),
        deps = envoy_stdlib_deps() + deps + [envoy_external_dep_path(dep) for dep in external_deps + ["googletest"]] + [
            repository + "//test:main",
            repository + "//test/test_common:test_version_linkstamp",
        ] + select({
            repository + "//bazel:clang_pch_build": [repository + "//test:test_pch"],
            "//conditions:default": [],
        }),
        # from https://github.com/google/googletest/blob/6e1970e2376c14bf658eb88f655a054030353f9f/googlemock/src/gmock.cc#L51
        # 2 - by default, mocks act as StrictMocks.
        args = args + ["--gmock_default_mock_behavior=2"],
        shard_count = shard_count,
        size = size,
        flaky = flaky,
        env = env,
        tags = coverage_tags,
    )
 

