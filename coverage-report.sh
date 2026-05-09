#!/bin/bash
ROOT_PATH_DIR=`pwd -P`
EXECUTION_ROOT=`bazel info execution_root`   #/root/.cache/bazel/_bazel_root/294bb41eaf3d1205cc67dd64edb3e89e/execroot/envoy_filters
BAZEL_ENVOY_FILTERS_DIR=$ROOT_PATH_DIR/bazel-envoy-filters
COVERAGE_REPORT_DIR=$ROOT_PATH_DIR/coverage-report-data

#需要过滤的相对目录
lcov_source_include="/filters*"
#lcov配置选项
lcov_rc_options="--rc lcov_branch_coverage=1 --rc lcov_list_full_path=1"
#是否需要common目录的库,0 不需要 1需要
need_common_lib=0

# 是否需要覆蓋率報告 0 不需要 1需要
need_coverage_report=1

# 报告前清理操作 0清理gcda、gcno、pic.d、pic.o文件 1需要清理gcda文件 2 不需要清理
clean_before_coverage_report=0

#需要检索gcda的目录
lcov_d_option=""
make_lcov_d_option() {
    lcov_d_option=""
    # for unit test，such as filter_test
    # /root/.cache/bazel/_bazel_root/294bb41eaf3d1205cc67dd64edb3e89e/execroot/envoy_filters/bazel-out/k8-fastbuild/bin/filters/test/extensions/filters/http/strong_local_ratelimit/_objs/
    lcov_d_option=$lcov_d_option" -d ""$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test/extensions/filters/http/$1/_objs/"
    #for library from source, such as strong_local_ratelimit_lib\acl_filter_lib
    #/root/.cache/bazel/_bazel_root/294bb41eaf3d1205cc67dd64edb3e89e/execroot/envoy_filters/bazel-out/k8-fastbuild/bin/filters/source/extensions/filters/http/strong_local_ratelimit/_objs/
    lcov_d_option=$lcov_d_option" -d ""$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source/extensions/filters/http/$1/_objs/"
    if [ $need_common_lib == 1 ]; then
        #/root/.cache/bazel/_bazel_root/294bb41eaf3d1205cc67dd64edb3e89e/execroot/envoy_filters/bazel-out/k8-fastbuild/bin/filters/source/extensions/filters/http/common/strong_matcher/_objs/
        lcov_d_option=$lcov_d_option" -d ""$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source/extensions/filters/http/common"
    fi
}

help() {
    echo ==== 辅助指令 ====
    echo cleangcda 清除项目下的所有gcda文件
    echo cleanall 清除 目下的所有gcda、gcno、*.pic.o、*.pic.d文件。用于重新生成gcda、gcno
    echo checkgcfiles 搜索gcda、gcno文件
    echo checkallfiles 搜索gcda、gcno、*.pic.o、*.pic.d文件

    echo ==== acl 插件 ====
    echo acl:all acl插件的所有单元测试综合报告
    echo acl:filter_test 单元测试报告
    echo acl:config_test 单元测试报告

    echo ==== bot_detection 机器人检测插件插件 ====
    echo bot_detection:all bot_detection插件的所有单元测试综合报告
    echo bot_detection:filter_test 单元测试报告
    echo bot_detection:config_test 单元测试报告

    echo ==== strong_local_ratelimit 流控插件 ====
    echo strong_local_ratelimit:all  strong_local_ratelimit插件的所有单元测试综合报告
    echo strong_local_ratelimit:filter_test 单元测试报告
    echo strong_local_ratelimit:config_test 单元测试报告
    echo strong_local_ratelimit:quota_test 单元测试报告
    echo strong_local_ratelimit:action_test 单元测试报告
    echo strong_local_ratelimit:hash_table_test 单元测试报告
    echo strong_local_ratelimit:lru_test 单元测试报告

    echo ==== strong_global_ratelimit 全局限速插件 ====
    echo strong_global_ratelimit:all  strong_global_ratelimit插件的所有单元测试综合报告
    echo strong_global_ratelimit:filter_test 单元测试报告
    echo strong_global_ratelimit:config_test 单元测试报告
    echo strong_global_ratelimit:rule_test 单元测试报告
    echo strong_global_ratelimit:action_test 单元测试报告
    echo strong_global_ratelimit:ratelimit_test 单元测试报告

    echo ==== super_glue 流量编排插件 ====
    echo super_glue:all  super_glue插件的所有单元测试综合报告
    echo super_glue:client_filter_test 单元测试报告
    echo super_glue:client_config_test 单元测试报告
    echo super_glue:server_filter_test 单元测试报告
    echo super_glue:server_config_test 单元测试报告

    echo ==== response_rewrite 脱敏插件 ====
    echo response_rewrite:all  response_rewrite插件的所有单元测试综合报告
    echo response_rewrite:response_rewrite_test 单元测试报告
    echo response_rewrite:config_test
    echo response_rewrite:rewrite_rule_test

    echo ==== sensitive_detect 敏感API ====
    echo sensitive_detect:all  sensitive_detect插件的所有单元测试综合报告单
    echo sensitive_detect:sensitive_test 单元测试报告
    echo sensitive_detect:config_test 单元测试报告

    echo ==== waf waf防护 ====
    echo waf:all  waf插件的所有单元测试综合报告单
    echo waf:filter_test 单元测试报告
    echo waf:config_test 单元测试报告
    
    echo ==== api_check检测 ====
    echo api_check:all  waf插件的所有单元测试综合报告单
    echo api_check:config_test 单元测试报告

    echo ==== strong_stateful_session 会话保持 ====
    echo strong_stateful_session:all 所有测试
    echo strong_stateful_session:filter_test 插件功能集成测试
    echo strong_stateful_session:config_test 运行时配置单元测试

    echo ==== anti_cc CC攻击防护 ====
    echo anti_cc:all anti_cc插件的所有单元测试综合报告单
    echo anti_cc:filter_test 插件功能集成测试
    echo anti_cc:config_test 运行时配置单元测试
}

buildAclAll() {
    echo build all acl unit tests...
    buildOne acl:filter_test
    buildOne acl:config_test
    buildOneReport acl:all
}
buildBotDetectionAll() {
    echo build all bot_detection unit tests...
    buildOne bot_detection:filter_test
    buildOne bot_detection:config_test
    buildOneReport bot_detection:all
}
buildStrongLocalRateLimitAll() {
    echo build all strong_local_ratelimit coverage tests...
    buildOne strong_local_ratelimit:filter_test #strong_local_ratelimit:filter_test_coverage
    buildOne strong_local_ratelimit:config_test
    buildOne strong_local_ratelimit:quota_test
    buildOne strong_local_ratelimit:action_test
    buildOne strong_local_ratelimit:hash_table_test
    buildOne strong_local_ratelimit:lru_test
    buildOneReport strong_local_ratelimit:all
}
buildStrongGlobalRateLimitAll() {
    echo build all strong_global_ratelimit coverage tests...
    buildOne strong_global_ratelimit:filter_test #strong_global_ratelimit:filter_test_coverage
    buildOne strong_global_ratelimit:config_test
    buildOne strong_global_ratelimit:rule_test
    buildOne strong_global_ratelimit:action_test
    buildOneReport strong_global_ratelimit:all
}
buildSuperGlueAll() {
    echo build all super_glue unit tests...
    buildOne super_glue:client_filter_test
    buildOne super_glue:client_config_test
    buildOne super_glue:server_filter_test
    buildOne super_glue:server_config_test
    buildOneReport super_glue:all
}
buildResponseRewriteAll() {
    echo build all response_rewrite unit tests...
    buildOne response_rewrite:response_rewrite_test
    buildOne response_rewrite:config_test
    buildOne response_rewrite:rewrite_rule_test
    buildOneReport response_rewrite:all
}
buildSensitiveDetectAll() {
    echo build all sensitive_detect unit tests...
    buildOne sensitive_detect:sensitive_test 插件功能集成测试
    buildOne sensitive_detect:config_test 运行时配置单元测试
    buildOneReport sensitive_detect:all 所有测试
}

buildWafAll() {
    echo build all waf unit tests...
    buildOne waf:filter_test
    buildOne waf:config_test
    buildOneReport waf:all
}

buildApiCheckAll() {
    echo build all api_check unit tests...
    buildOne api_check:config_test
    buildOneReport api_check:all
}

buildStrongStatefulSession() {
    echo build all strong_stateful_session unit tests...
    buildOne strong_stateful_session:filter_test
    buildOne strong_stateful_session:config_test
}

buildUserIdentifyAll() {
    echo build all sensitive_detect unit tests...
    buildOne user_identify:filter_test 插件功能集成测试
    buildOne user_identify:config_test 运行时配置单元测试
    buildOneReport user_identify:all 所有测试
}

buildAntiCCAll() {
    echo build all anti_cc unit tests...
    buildOne anti_cc:filter_test
    buildOne anti_cc:config_test
    buildOneReport anti_cc:all
}

isFileValid(){
    file=$1
    stat $file
    if [ $? != 0 ];then
        echo "======================================================"
        echo "$file size not exist.failed to build file $file"
        echo "======================================================"
        exit 1
    fi
    filesize=`stat -c "%s" $file`
    if [ $filesize == 0 ];then
        echo "======================================================"
        echo "$file size is zero.check the gcda\gcno\source files"
        echo "======================================================"
        exit 1
    fi
}
#
buildOne() {
    #需要先运行bazel test构建
    echo "building $1..."
    if [ $need_coverage_report == 1 ];then
        if [ $clean_before_coverage_report == 0 ];then
            echo "cleanall before build"
            cleanall
        elif [ $clean_before_coverage_report == 1 ];then
            echo "cleangcda before build"
            cleangcda
        fi
        echo "bazel test --define build_coverage=true --jobs 10 --copt="-O0" --copt="-ggdb" --strip=never --verbose_failures //filters/test/extensions/filters/http/$1"
        bazel test --test_arg="-l trace"  --define build_coverage=true --jobs 2 --copt="-O0" --copt="-ggdb" --strip=never --verbose_failures //filters/test/extensions/filters/http/$1
    else
        echo "bazel test --jobs 10 --copt="-O0" --copt="-ggdb" --strip=never --verbose_failures //filters/test/extensions/filters/http/$1"
        bazel test --test_arg="-l trace" --jobs 2 --copt="-O0" --copt="-ggdb" --strip=never --verbose_failures //filters/test/extensions/filters/http/$1
    fi
    if [ $? != 0 ];then
        echo "======================================================"
        echo "failed to run bazel test.check the code"
        echo "======================================================"
        exit 1
    fi
    module=`echo $1 |cut -d : -f 1` #strong_local_ratelimit
    target=`echo $1 |cut -d : -f 2` #filter_test
    if [ $need_coverage_report != 1 ];then
        echo "======================================================"
        echo "just build test(module:$module,target:$target) and exit"
        echo "======================================================"
        exit 0
    fi
    if [ -n "$module" ] && [ -n "$target" ]; then
        echo "start======================================================"
        checkgcdafiles
        make_lcov_d_option $module
        echo "======================================================"
        cd $BAZEL_ENVOY_FILTERS_DIR #进入bazel的代码链接目录bazel-envoy-filters
        #lcov base:先创建基准文件info
        echo "making lcov base for $module"
        echo "lcov $lcov_d_option -b $BAZEL_ENVOY_FILTERS_DIR $lcov_rc_options --include "$BAZEL_ENVOY_FILTERS_DIR$lcov_source_include"  -c -i -o  "$module-$target.info_base""
        lcov $lcov_d_option -b $BAZEL_ENVOY_FILTERS_DIR $lcov_rc_options --include "$BAZEL_ENVOY_FILTERS_DIR$lcov_source_include"  -c -i -o  "$module-$target.info_base"
        isFileValid "$module-$target.info_base"

        exe_file="$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test/extensions/filters/http/$module/$target"
        #run /root/.cache/bazel/_bazel_root/294bb41eaf3d1205cc67dd64edb3e89e/execroot/envoy_filters/bazel-out/k8-fastbuild/bin/filters/test/extensions/filters/http/strong_local_ratelimit/filter_test
        echo "run $exe_file" #运行测试代码
        $exe_file
        if [ $? != 0 ];then
            echo "======================================================"
            echo "failed to run $exe_file.try to rerun"
            echo "======================================================"
            exit 1
        fi
        checkgcdafiles

        #coverage info file:创建测试覆盖率info
        echo "making lcov $module-$target.info_current"
        echo "lcov $lcov_d_option  -b $BAZEL_ENVOY_FILTERS_DIR $lcov_rc_options --include "$BAZEL_ENVOY_FILTERS_DIR$lcov_source_include"  -c -o  "$module-$target.info_current""
        lcov $lcov_d_option  -b $BAZEL_ENVOY_FILTERS_DIR $lcov_rc_options --include "$BAZEL_ENVOY_FILTERS_DIR$lcov_source_include"  -c -o  "$module-$target.info_current"
        isFileValid "$module-$target.info_current"

        #结合基准文件和测试覆盖率文件，为该单元测试的覆盖率文件
        echo "making lcov $module-$target.info"
        echo "lcov $lcov_rc_options -a "$module-$target.info_base" -a "$module-$target.info_current" -o "$module-$target".info"
        lcov $lcov_rc_options -a "$module-$target.info_base" -a "$module-$target.info_current" -o "$module-$target".info
        isFileValid "$module-$target.info"

        #移动单元测试的覆盖率文件到缓存目录coverage-report-data
        rm "$module-$target.info_base" "$module-$target.info_current"
        cp "$module-$target".info "$COVERAGE_REPORT_DIR/"

        cd $ROOT_PATH_DIR
    fi
}

buildOneReport() {
    module=`echo $1 |cut -d : -f 1` #strong_local_ratelimit
    target=`echo $1 |cut -d : -f 2` #filter_test all
    echo "building $target report for module:$module"
    if [ -n "$module" ] && [ -n "$target" ]; then
        cd $COVERAGE_REPORT_DIR #进入覆盖率文件缓存目录coverage-report-data
        #合并某插件的多个单元测试福覆盖率文件，产生插件覆盖率报告
        genhtml -t "$module-$target" --rc genhtml_branch_coverage=1 ./*.info -o "$module-$target"
        tar cf "$module-$target".tar "$module-$target"
        cd $ROOT_PATH_DIR
    fi
}

cleangcda() {
    # /root/.cache/bazel/_bazel_root/294bb41eaf3d1205cc67dd64edb3e89e/execroot/envoy_filters/bazel-out/k8-fastbuild/bin/filters
    echo "clean *.pic.gcda========================================"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters" -name "*.pic.gcda" |xargs -i rm {}
    echo "========================================"
}
cleanall() {
    echo "clean *.pic.gcda========================================"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters" -name "*.pic.gcda" |xargs -i rm {}
    echo "clean *.pic.gcno========================================"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters" -name "*.pic.gcno" |xargs -i rm {}
    echo "clean *.pic.o========================================"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source" -name "*.pic.o" |xargs -i rm {}
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test" -name "*.pic.o" |xargs -i rm {}
    echo "clean *.pic.d========================================"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source" -name "*.pic.d" |xargs -i rm {}
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test" -name "*.pic.d" |xargs -i rm {}
    echo "========================================"
}
checkgcdafiles() {
    echo "*.pic.gcda========================================"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters" -name "*.pic.gcda"
    echo "========================================"
}
checkgcfiles() {
    checkgcdafiles
    echo "*.pic.gcno========================================"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters" -name "*.pic.gcno"
    echo "========================================"
}
checkallfiles() {
    checkgcfiles
    echo "*.pic.o========================================"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source" -name "*.pic.o"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test" -name "*.pic.o"
    echo "*.pic.d========================================"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source"
    echo "from $EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/source" -name "*.pic.d"
    find "$EXECUTION_ROOT/bazel-out/k8-fastbuild/bin/filters/test" -name "*.pic.d"
    echo "========================================"
}
dispatch() {
    if [ "$1" == "help" ]; then
        help
    elif [ "$1" == "cleangcda" ]; then
        cleangcda
    elif [ "$1" == "cleanall" ]; then
        cleanall
    elif [ "$1" == "checkgcfiles" ]; then
        checkgcfiles
    elif [ "$1" == "checkallfiles" ]; then
        checkallfiles
    else
        if [ $need_coverage_report == 1 ];then
            test -d $COVERAGE_REPORT_DIR && rm -r  $COVERAGE_REPORT_DIR
            mkdir -p $COVERAGE_REPORT_DIR
        fi
        if [ "$1" == "acl:all" ]; then
            buildAclAll
        elif [ "$1" == "strong_local_ratelimit:all" ]; then
            buildStrongLocalRateLimitAll
        elif [ "$1" == "response_rewrite:all" ]; then
            buildResponseRewriteAll
        elif [ "$1" == "bot_detection:all" ]; then
            buildBotDetectionAll
        elif [ "$1" == "strong_global_ratelimit:all" ]; then
            buildStrongGlobalRateLimitAll
        elif [ "$1" == "sensitive_detect:all" ]; then
            buildSensitiveDetectAll
        elif [ "$1" == "waf:all" ]; then
            buildWafAll
        elif [ "$1" == "api_check:all" ]; then
            buildApiCheckAll
        elif [ "$1" == "strong_stateful_session:all" ]; then
            buildStrongStatefulSession
        elif [ "$1" == "user_identify:all" ]; then
            buildUserIdentifyAll
        elif [ "$1" == "anti_cc:all" ]; then
            buildAntiCCAll
        else
            buildOne $1
            buildOneReport $1
        fi
    fi
}

if [ $# -eq 1 ]; then
    dispatch $1
else
    help
fi
