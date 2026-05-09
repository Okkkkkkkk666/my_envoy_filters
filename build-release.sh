#!/bin/bash
set -e

if [[ $# -gt 1 || ( $# -eq 1 && $1 != "pkg" ) ]]; then
  echo "Usage: $0 [pkg]"
  echo
  echo "    without pkg: just build release version"
  echo "    with    pkg: build release version and create depoly package"
  exit 0
fi

reset
echo update submodules...
git submodule update --init --recursive

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
  --jobs 4 \
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
  --jobs 4 \
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

if [[ $1 = "pkg" ]]; then
  ./tools/envoy_package.sh
fi