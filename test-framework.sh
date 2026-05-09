#!/bin/bash

FRAMEWORK_VERSION="v1_1_x"
help() {
    echo usage: test-framework.sh.sh dest [framework_version]
    echo
    echo 参数说明:
    echo '  dest 测试目标'
    echo "  framework_version srhino_plugin_framework的版本号，可选。默认选择 ${FRAMEWORK_VERSION}"
    echo
    echo e.g.
    echo '  test-framework.sh all v1_1_x'
    echo
    echo ==== all 运行所有测试 ====
    echo all 所有测试
    echo
    echo ==== plugin_loader 插件加载测试 ====
    echo plugin_loader:all 所有测试
    echo plugin_loader:file_name_matcher_test 文件名匹配测试
    echo
    echo ==== envoy_impl envoy转换实现测试 ====
    echo envoy_impl:all 所有测试
    echo envoy_impl:header_map_impl_test header_map测试
    echo
    echo ==== factory_impl 工厂实现类测试 ====
    echo factory_impl:all 所有测试
    echo factory_impl:factory_impl_test factory_impl测试
    echo
    echo ==== regex regex库测试 ====
    echo regex:all 所有测试
    echo regex:regex_matcher_test
    echo regex:regex_replacer_test
    echo
    echo ==== config config库测试 ====
    echo config:all 所有测试
    echo config:config_impl_test
    echo
    echo ==== net net库测试 ====
    echo net:all 所有测试
    echo net:net_test
    echo
    echo ==== thread_local thread_local库测试 ====
    echo thread_local:all 所有测试
    echo thread_local:thread_local_test
    echo
}

buildPluginLoader() {
  path="srhino_plugin_framework"
  if [ "$1" == "all" ]; then
    echo build all plugin loaders unit tests...
    buildOne ${path}:file_name_matcher_test
  else
    buildOne ${path}:$1
  fi
}

buildEnvoyImpl() {
  path="srhino_plugin_framework/${FRAMEWORK_VERSION}"
  if [ "$1" == "all" ]; then
    echo build all envoy implement unit tests...
    buildOne ${path}:header_map_impl_test
  else
    buildOne ${path}:$1
  fi
}

buildFactoryImpl() {
  path="srhino_plugin_framework/${FRAMEWORK_VERSION}/libs"
  if [ "$1" == "all" ]; then
    echo build all factory implement unit tests...
    buildOne ${path}:factory_impl_test
  else
    buildOne ${path}:$1
  fi
}

buildRegex() {
  test_data_path="/tmp/test_data"
  if [ -L "$test_data_path" ]; then
    unlink "$test_data_path"
  fi
  ln -s "`pwd`/envoy/test/srhino_plugin_framework/${FRAMEWORK_VERSION}/libs/regex/test_data" /tmp/

  path="srhino_plugin_framework/${FRAMEWORK_VERSION}/libs/regex"
  if [ "$1" == "all" ]; then
    echo build all envoy implement unit tests...
    buildOne ${path}:regex_matcher_test
    buildOne ${path}:regex_replacer_test
  else
    buildOne ${path}:$1
  fi
}

buildDatabase() {
  path="srhino_plugin_framework/${FRAMEWORK_VERSION}/libs/database"
  if [ "$1" == "all" ]; then
    echo build all envoy implement unit tests...
    buildOne ${path}:sqlite_database_test
  else
    buildOne ${path}:$1
  fi
}

buildConfig() {
  path="srhino_plugin_framework/${FRAMEWORK_VERSION}/libs/config"
  if [ "$1" == "all" ]; then
    echo build all config implement unit tests...
    buildOne ${path}:config_impl_test
  else
    buildOne ${path}:$1
  fi
}

buildNet() {
  path="srhino_plugin_framework/${FRAMEWORK_VERSION}/libs/net"
  if [ "$1" == "all" ]; then
    echo build all net implement unit tests...
    buildOne ${path}:net_test
  else
    buildOne ${path}:$1
  fi
}

buildThreadLocal() {
  path="srhino_plugin_framework/${FRAMEWORK_VERSION}/libs/thread_local"
  if [ "$1" == "all" ]; then
    echo build all thread local implement unit tests...
    buildOne ${path}:thread_local_test
  else
    buildOne ${path}:$1
  fi
}

buildOne() {
    echo build $1...
    bazel test --jobs 4 --copt="-O0" --copt="-ggdb" --strip=never --verbose_failures @envoy//test/$1
}

dispatch() {
    if [ "$1" == "help" ]; then
        help
        return
    fi

    main=${1%%:*}
    sub=${1##*:}

    case $main in 
      "all" )
        buildPluginLoader all
        buildEnvoyImpl all
        buildFactoryImpl all
        buildRegex all
        buildDatabase all
        buildConfig all
        buildNet all
        buildThreadLocal all
        ;;
      "plugin_loader" )
        buildPluginLoader $sub
        ;;
      "envoy_impl" )
        buildEnvoyImpl $sub
        ;;
      "factory_impl" )
        buildFactoryImpl $sub
        ;;
      "regex" )
        buildRegex $sub
        ;;
      "database" )
        buildDatabase $sub
        ;;
      "config" )
        buildConfig $sub
        ;;
      "net" )
        buildNet $sub
        ;;
      "thread_local" )
        buildThreadLocal $sub
        ;;
    esac
}

if [ $# -ge 1 ]; then
    if [ $# -eq 2 ]; then
      FRAMEWORK_VERSION="$2"
    fi
    dispatch $1
else
    help
fi
