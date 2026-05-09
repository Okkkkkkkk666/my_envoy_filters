load("@rules_cc//cc:defs.bzl", "cc_binary")
# load("@rules_python//python:defs.bzl", "py_binary", "py_test")
# load("@rules_fuzzing//fuzzing:cc_defs.bzl", "fuzzing_decoration")
# load("@envoy//bazel:envoy_binary.bzl", "envoy_cc_binary")
# load("@envoy//bazel:envoy_library.bzl", "tcmalloc_external_deps")
# load("@envoy//bazel:envoy_pch.bzl", "envoy_pch_copts")
# load("@bazel_skylib//rules:common_settings.bzl", "config_setting")

load(
    "@envoy//bazel:envoy_internal.bzl",
    "envoy_copts",
    "envoy_external_dep_path",
    # "envoy_linkstatic",
    # "envoy_select_force_libcpp",
    "envoy_stdlib_deps",
    "tcmalloc_external_dep",
)
# load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
# bool_flag(
#     name = "config_build_coverage_flag",
#     build_setting_default = False,
# )
# Select the given values if exporting is enabled in the current build.
def _envoy_select_exported_symbols(xs):
    return select({
        "@envoy//bazel:enable_exported_symbols": xs,
        "//conditions:default": [],
    })

# Compute the final linkopts based on various options.
def _envoy_linkopts():
    return select({
        "@envoy//bazel:apple": [],
        "@envoy//bazel:windows_opt_build": [
            "-DEFAULTLIB:ws2_32.lib",
            "-DEFAULTLIB:iphlpapi.lib",
            "-DEFAULTLIB:shell32.lib",
            "-DEBUG:FULL",
            "-WX",
        ],
        "@envoy//bazel:windows_x86_64": [
            "-DEFAULTLIB:ws2_32.lib",
            "-DEFAULTLIB:iphlpapi.lib",
            "-DEFAULTLIB:shell32.lib",
            "-WX",
        ],
        "//conditions:default": [
            "-pthread",
            "-lrt",
            "-ldl",
            "-Wl,-z,relro,-z,now",
            "-Wl,--hash-style=gnu",
        ],
    }) + select({
        "@envoy//bazel:boringssl_fips": [],
        "@envoy//bazel:windows_x86_64": [],
        "//conditions:default": ["-pie"],
    }) + _envoy_select_exported_symbols(["-Wl,-E"])

def _envoy_stamped_deps():
    return select({
        "@envoy//bazel:windows_x86_64": [],
        "@envoy//bazel:apple": [
            "@envoy//bazel:raw_build_id.ldscript",
        ],
        "//conditions:default": [
            "@envoy//bazel:gnu_build_id.ldscript",
        ],
    })

def _envoy_stamped_linkopts():
    return select({
        # Coverage builds in CI are failing to link when setting a build ID.
        #
        # /usr/bin/ld.gold: internal error in write_build_id, at ../../gold/layout.cc:5419
        "@envoy//bazel:coverage_build": [],
        "@envoy//bazel:windows_x86_64": [],

        # macOS doesn't have an official equivalent to the `.note.gnu.build-id`
        # ELF section, so just stuff the raw ID into a new text section.
        "@envoy//bazel:apple": [
            "-sectcreate __TEXT __build_id",
            "$(location @envoy//bazel:raw_build_id.ldscript)",
        ],

        # Note: assumes GNU GCC (or compatible) handling of `--build-id` flag.
        "//conditions:default": [
            "-Wl,@$(location @envoy//bazel:gnu_build_id.ldscript)",
        ],
    })


# config_setting(
#     name = "config_build_coverage",
#     values = {"define": "build_coverage=true"},
# )
# Envoy C++ binary targets should be specified with this function.
def srhino_cc_binary(
        name,
        srcs = [],
        data = [],
        testonly = 0,
        visibility = None,
        external_deps = [],
        repository = "",
        stamped = False,
        deps = [],
        linkopts = [],
        tags = [],
        features = []):
    # coverage_copts = list()
    # coverage_copts.append("-fprofile-arcs")
    # coverage_copts.append("-ftest-coverage")
   
    # coverage_linkopts = (["-lgcov"])
    # coverage_tags = list(tags)
    # if "no-sandbox" not in tags:
    #     coverage_tags.append("no-sandbox")
    
    native.config_setting(
        name = name+"-config_build_coverage_enable",
        values = {"define": "build_coverage=true"},
        visibility = ["//visibility:private"],
    )
    coverage_copts = select({":" +name+ "-config_build_coverage_enable": ["-fprofile-arcs","-ftest-coverage"], "//conditions:default": []})
    # print("coverage_copts info:",coverage_copts)
    coverage_linkopts = select({":" +name+ "-config_build_coverage_enable": ["-lgcov"], "//conditions:default": []})
    # print("coverage_linkopts info:",coverage_linkopts)
    coverage_tags = tags + ["no-sandbox"] 
    # print("coverage_tags info:",coverage_tags)

    if not linkopts:
        linkopts = _envoy_linkopts() + coverage_linkopts
    if stamped:
        linkopts = linkopts + _envoy_stamped_linkopts() + coverage_linkopts
        deps = deps + _envoy_stamped_deps()
    deps = deps + [envoy_external_dep_path(dep) for dep in external_deps] + envoy_stdlib_deps()
    
    cc_binary(
        name = name,
        srcs = srcs,
        data = data,
        copts = envoy_copts(repository) + coverage_copts,
        linkopts = linkopts,
        testonly = testonly,
        linkstatic = 1,
        visibility = visibility,
        malloc = tcmalloc_external_dep(repository),
        stamp = 1,
        deps = deps,
        tags = coverage_tags,
        features = features,
    )



 

