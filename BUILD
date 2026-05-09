package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        # "//filters/source/extensions/filters/http/sensitive_detect:config",
        # "//filters/source/extensions/filters/http/acl:config",
        # "//filters/source/extensions/filters/http/strong_local_ratelimit:config",
        # "//filters/source/extensions/filters/http/response_rewrite:config",
        # "//filters/source/extensions/filters/http/strong_global_ratelimit:config",
        # "//filters/source/extensions/filters/http/user_identify:config",
        # "//filters/source/extensions/filters/http/waf:config",
        # "//filters/source/extensions/filters/http/api_check:config",
        # "//filters/source/extensions/filters/http/bot_detection:config",
        # "//filters/source/extensions/filters/http/super_glue:client_config",
        # "//filters/source/extensions/filters/http/super_glue:server_config",
        # "//filters/source/extensions/filters/http/weak_password_check:config",
        "//filters/source/extensions/filters/http/strong_stateful_session:config",
        # "//filters/source/extensions/filters/http/anti_cc:config",
        "//filters/source/extensions/filters/http/body_to_metadata:config",
        "//filters/source/extensions/filters/http/srhino_plugin_loader:config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

# sh_test(
#     name = "envoy_binary_test",
#     srcs = ["envoy_binary_test.sh"],
#     data = [":envoy"],
# )
