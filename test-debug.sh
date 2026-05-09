#!/bin/bash
help() {
    echo ==== acl 访问控制列表 ====
    echo acl:all 所有测试
    echo acl:filter_test 插件功能集成测试
    echo acl:config_test 运行时配置单元测试
    echo
    echo ==== strong_local_ratelimit 流控插件 ====
    echo strong_local_ratelimit:all 所有测试
    echo strong_local_ratelimit:filter_test 插件功能集成测试
    echo strong_local_ratelimit:config_test 运行时配置单元测试
    echo strong_local_ratelimit:quota_test 配额单元测试
    echo strong_local_ratelimit:action_test 规则及限速动作单元测试
    echo strong_local_ratelimit:hash_table_test 哈希表单元测试
    echo strong_local_ratelimit:lru_test LRU单元测试
    echo
    echo ==== response_rewrite 脱敏插件 ====
    echo response_rewrite:all
    echo response_rewrite:response_rewrite_test 插件功能集成测试
    echo response_rewrite:config_test
    echo
    echo ==== sensitive_detect 敏感API ====
    echo sensitive_detect:all 所有测试
    echo sensitive_detect:sensitive_test 插件功能集成测试
    echo sensitive_detect:config_test 运行时配置单元测试
    echo
    echo ==== user_identify 用户识别 ====
    echo user_identify:all 所有测试
    echo user_identify:filter_test 插件功能集成测试
    echo user_identify:config_test 运行时配置单元测试
    echo
    echo ==== waf 防护 ====
    echo waf:all 所有测试
    echo waf:filter_test 插件功能集成测试
    echo waf:config_test 运行时配置单元测试
    echo
    echo ==== api_check检测 ====
    echo api_check:all 所有测试
    echo api_check:config_test 运行时配置单元测试
    echo
    echo
    echo ==== strong_stateful_session 会话保持 ====
    echo strong_stateful_session:all 所有测试
    echo strong_stateful_session:filter_test 插件功能集成测试
    echo strong_stateful_session:config_test 运行时配置单元测试
    echo
    echo
    echo ==== anti_cc CC攻击防护 ====
    echo anti_cc:all 所有测试
    echo anti_cc:filter_test 插件功能集成测试
    echo anti_cc:config_test 运行时配置单元测试
    echo
    echo ==== weak_password_check 弱密码检测 ====
    echo weak_password_check:all 所有测试
    echo weak_password_check:filter_test 插件功能集成测试
    echo weak_password_check:config_test 运行时配置单元测试
    echo
    echo
    echo ==== super_glue 流量编排 ====
    echo super_glue:all 所有测试
    echo super_glue:client_config_test client插件运行时配置单元测试
    echo super_glue:client_filter_test client插件功能集成测试
    echo super_glue:server_config_test server插件运行时配置单元测试
    echo super_glue:server_filter_test server插件功能集成测试
}

buildAclAll() {
    echo build all acl unit tests...
    buildOne acl:filter_test
    buildOne acl:config_test
    # buildOne acl:filter_integration_test
}

buildStrongLocalRateLimitAll() {
    echo build all strong_local_ratelimit unit tests...
    buildOne strong_local_ratelimit:filter_test
    buildOne strong_local_ratelimit:config_test
    buildOne strong_local_ratelimit:quota_test
    buildOne strong_local_ratelimit:action_test
    buildOne strong_local_ratelimit:hash_table_test
    buildOne strong_local_ratelimit:lru_test
}
buildResponseRewrite() {
    echo build all response_rewrite unit tests...
    buildOne response_rewrite:response_rewrite_test
    buildOne response_rewrite:config_test
}
buildSensitiveDetect() {
    echo build all sensitive_detect unit tests...
    buildOne sensitive_detect:sensitive_test
    buildOne sensitive_detect:config_test
}
buildUserIdentify() {
    echo build all user_identify unit tests...
    buildOne user_identify:filter_test
    buildOne user_identify:config_test
}
buildWafAll() {
    echo build all waf unit tests...
    buildOne waf:filter_test
    buildOne waf:config_test
}
buildApiCheckAll() {
    echo build all api_check unit tests...
    buildOne api_check:config_test
}
buildStrongStatefulSession() {
    echo build all strong_stateful_session unit tests...
    buildOne strong_stateful_session:filter_test
    buildOne strong_stateful_session:config_test
}
buildAntiCCAll() {
    echo build all anti_cc unit tests...
    buildOne anti_cc:filter_test
    buildOne anti_cc:config_test
}
buildWpdAll() {
    echo build all anti_cc unit tests...
    buildOne weak_password_check:filter_test
    buildOne weak_password_check:config_test
}
buildSuperGlueAll() {
    echo build all super_glue unit tests...
    buildOne super_glue:client_config_test
    buildOne super_glue:client_filter_test
    buildOne super_glue:server_config_test
    buildOne super_glue:server_filter_test
}

buildOne() {
    echo build $1...
    bazel test --jobs 4 --copt="-O0" --copt="-ggdb" --strip=never --verbose_failures //filters/test/extensions/filters/http/$1
}

dispatch() {
    if [ "$1" == "help" ]; then
        help
    elif [ "$1" == "acl:all" ]; then
        buildAclAll
    elif [ "$1" == "strong_local_ratelimit:all" ]; then
        buildStrongLocalRateLimitAll
    elif [ "$1" == "response_rewrite:all" ]; then
        buildResponseRewrite
    elif [ "$1" == "sensitive_detect:all" ]; then
        buildSensitiveDetect
    elif [ "$1" == "user_identify:all" ]; then
        buildUserIdentify
    elif [ "$1" == "waf:all" ]; then
        buildWafAll
    elif [ "$1" == "api_check:all" ]; then
        buildApiCheckAll
    elif [ "$1" == "strong_stateful_session:all" ]; then
        buildStrongStatefulSession
    elif [ "$1" == "anti_cc:all" ]; then
        buildAntiCCAll
    elif [ "$1" == "weak_password_check:all" ]; then
        buildWpdAll
    elif [ "$1" == "super_glue:all" ]; then
        buildSuperGlueAll
    else
        buildOne $1
    fi
}

if [ $# -eq 1 ]; then
    dispatch $1
else
    help
fi
