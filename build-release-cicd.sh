#!/bin/bash
set -e

if [[ $# -gt 1 || ( $# -eq 1 && $1 != "pkg" ) ]]; then
  echo "Usage: $0 [pkg]"
  echo
  echo "    without pkg: just build release version"
  echo "    with    pkg: build release version and create depoly package"
  exit 0
fi

# reset
echo update submodules...
git submodule update --init --depth=1

echo apply patches...
cd envoy
git checkout .
# centos 适配
if [[ -f /etc/redhat-release ]] && grep </etc/redhat-release -q -i "centos"; then
  git apply ../patches/centos_compile_remove_tcp_stats.patch
fi
cd ..
cd tools/install-srhino-plugin-framwork/protoc-gen-validate
git checkout .
git apply ../../../patches/fix_protoc-gen-validate_makefile.patch
cd ../../../

echo =====================================================================================================================
echo build envoy...
if [ ! -d envoy/bssl-compat/third_party/Tongsuo ]; then
  echo "create tongsuo link"
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  ln -s "${SCRIPT_DIR}/third-party/Tongsuo" "${SCRIPT_DIR}/envoy/bssl-compat/third_party/Tongsuo"
fi
arch=$(uname -m)
pkg_arch=$(arch)

rm -rf /root/.cache
if [[ $arch == "x86_64" ]]; then
  pkg_arch=amd64
  ln -s /hostdata/ubuntu/x86_64/cache/master/.cache /root/.cache
elif [[ $arch == "aarch64" ]]; then
  pkg_arch=arm64
  ln -s /hostdata/kylin/aarch64/envoy_cache/cache /root/.cache
fi
rm -rf /root/.cache/bazel/_bazel_root/install/*

if [ $(uname -m) = "aarch64" ]; then
  # arm架构增加-fsigned-char选项
  bazel build \
  --@envoy//source/extensions/wasm_runtime/v8:enabled=false \
  --define build_hyperscan_mode=release \
  --define build_modsecurity_mode=release \
  --define build_sqlite3_mode=release \
  --define build_cryptopp_mode=release \
  --define build_Zstd_mode=release \
  --define build_json_c_mode=release \
  --define build_libev_mode=release \
  -s \
  --jobs 10 \
  -c opt \
  --copt="-fsigned-char" \
  --explain=opt-explain.txt \
  --verbose_explanations \
  --strip=always \
  --verbose_failures \
  //:envoy || {
    echo "bazel build error!"
    exit
  }
else
  bazel build \
  --@envoy//source/extensions/wasm_runtime/v8:enabled=false \
  --define build_hyperscan_mode=release \
  --define build_modsecurity_mode=release \
  --define build_sqlite3_mode=release \
  --define build_cryptopp_mode=release \
  --define build_Zstd_mode=release \
  --define build_json_c_mode=release \
  --define build_libev_mode=release \
  -s \
  --jobs 10 \
  -c opt \
  --explain=opt-explain.txt \
  --verbose_explanations \
  --strip=always \
  --verbose_failures \
  //:envoy || {
    echo "bazel build error!"
    exit
  }
fi

pkg_parent_dir="/tmp/srhino_framework_pkg_`date +%s%3N`"
project_path=$(cd $(dirname $0)/; pwd)
current_path=$(pwd)

function package_srhino_framework() {
  # if [[ ! -d "/usr/local/include/srhino.com/srhino_plugin_framework" || ! `find /usr/local/lib/cmake/ -type d -name srhino_plugin_framework*` ]]; then
  #   echo "compile and install envoy first."
  #   return 1
  # fi

  # install srhino plugin framwork
  cd ${project_path}/tools/install-srhino-plugin-framwork
  ./config.sh
  cd ./build
  make install

  envoy_version=`${project_path}/bazel-bin/envoy --version | awk -F 'version: ' '{if(length != 0) print $2}'`
  envoy_version_hash=`echo ${envoy_version} | awk -F '/' '{print $1}'`
  pkg_name="srhino_framework_pkg_g`echo ${envoy_version_hash:0:6}`-linux-${pkg_arch}"
  pkg_dir_path="${pkg_parent_dir}/$pkg_name"

  mkdir -p ${pkg_dir_path}
  cd ${pkg_dir_path}
  cp /usr/local/include/srhino.com ./ -r
  mkdir cmake
  cp /usr/local/lib/cmake/srhino_plugin_framework* ./cmake/ -r
  cp ${project_path}/bazel-bin/envoy ./

  cd ${pkg_parent_dir}
  tar zcvf ${pkg_name}.tar.gz ${pkg_name}
  mv ${pkg_name}.tar.gz ${current_path}/
  echo "create srhino plugin framework install package success."
  rm -rf ${pkg_parent_dir}
  cd ${current_path}
}

if [[ $1 = "pkg" ]]; then
  ./tools/envoy_package.sh
  package_srhino_framework
fi
