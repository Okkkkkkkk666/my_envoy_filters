# #!/bin/bash
# set -e

# reset
# echo update submodules...
# # git submodule update --init --recursive

# echo apply patches...
# cd envoy
# # git checkout .
# # centos 适配
# if [[ -f /etc/redhat-release ]] && grep </etc/redhat-release -q -i "centos"; then
#   git apply ../patches/centos_compile_remove_tcp_stats.patch
# fi
# cd ..
# cd tools/install-srhino-plugin-framwork/protoc-gen-validate
# git checkout .
# git apply ../../../patches/fix_protoc-gen-validate_makefile.patch
# cd ../../../

# if [ "`git config merge.ff`" != "false" ];then
#   echo "set git confg merge.ff false"
#   git config --global --add merge.ff false
# fi

# echo =====================================================================================================================
# echo build envoy...
# if [ ! -d envoy/bssl-compat/third_party/Tongsuo ]; then
#   echo "create tongsuo link"
#   SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
#   ln -s "${SCRIPT_DIR}/third-party/Tongsuo" "${SCRIPT_DIR}/envoy/bssl-compat/third_party/Tongsuo"
# fi
# if [ $(uname -m) = "aarch64" ]; then
#   # arm架构增加-fsigned-char选项
#   bazel build \
#   --@envoy//source/extensions/wasm_runtime/v8:enabled=false \
#   --define build_hyperscan_mode=debug \
#   --define build_modsecurity_mode=debug \
#   --define build_sqlite3_mode=debug \
#   --define build_cryptopp_mode=debug \
#   --define build_Zstd_mode=debug \
#   --define build_json_c_mode=debug \
#   --define build_libev_mode=debug \
#   --jobs 4 \
#   -c dbg \
#   --copt="-gstrict-dwarf" \
#   --copt="-fsigned-char" \
#   --explain=debug-explain.txt \
#   --verbose_explanations \
#   --strip=never \
#   --verbose_failures \
#   //:envoy || {
#     echo "bazel build error!"
#     exit
#   }
# else
#  bazel build \
#   --@envoy//source/extensions/wasm_runtime/v8:enabled=false \
#   --@envoy//contrib/squash/filters/http/source:enabled=false \
#   --@envoy//contrib/sxg/filters/http/source:enabled=false \
#   --@envoy//contrib/kafka/filters/network/source:enabled=false \
#   --@envoy//contrib/kafka/filters/network/source/mesh:enabled=false \
#   --@envoy//contrib/postgres_proxy/filters/network/source:enabled=false \
#   --@envoy//contrib/rocketmq_proxy/filters/network/source:enabled=false \
#   --@envoy//contrib/vcl/source:enabled=false \
#   --@envoy//contrib/cryptomb/private_key_providers/source:enabled=false \
#   --@envoy//contrib/sip_proxy/filters/network/source:enabled=false \
#   --@envoy//contrib/sip_proxy/filters/network/source/router:enabled=false \
#   --@envoy//contrib/mysql_proxy/filters/network/source:enabled=true \
#   --define build_hyperscan_mode=debug \
#   --define build_modsecurity_mode=debug \
#   --define build_sqlite3_mode=debug \
#   --define build_cryptopp_mode=debug \
#   --define build_Zstd_mode=debug \
#   --define build_json_c_mode=debug \
#   --define build_libev_mode=debug \
#   --jobs 4 \
#   -c dbg \
#   --copt="-gstrict-dwarf" \
#   --copt="-gdwarf-4" \
#   --explain=debug-explain.txt \
#   --verbose_explanations \
#   --strip=never \
#   --incompatible_strict_action_env=false \
#   --verbose_failures \
#   --define=boringssl=none \
#   @envoy//contrib/exe:envoy || {
#     echo "bazel build error!"
#     exit
#   }
# fi

# # 调试时源文件查找路径
# rm external -f && ln -s $(bazel info output_base)/external external#!/bin/bash

# set -e

# reset
# echo update submodules...
# git submodule update --init --recursive

# echo apply patches...
# cd envoy
# # git checkout .
# # centos 适配
# if [[ -f /etc/redhat-release ]] && grep </etc/redhat-release -q -i "centos"; then
#   git apply ../patches/centos_compile_remove_tcp_stats.patch
# fi
# cd ..
# cd tools/install-srhino-plugin-framwork/protoc-gen-validate
# git checkout .
# git apply ../../../patches/fix_protoc-gen-validate_makefile.patch
# cd ../../../

# if [ "`git config merge.ff`" != "false" ];then
#   echo "set git confg merge.ff false"
#   git config --global --add merge.ff false
# fi

# echo =====================================================================================================================
# echo build envoy...
# if [ ! -d envoy/bssl-compat/third_party/Tongsuo ]; then
#   echo "create tongsuo link"
#   SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
#   ln -s "${SCRIPT_DIR}/third-party/Tongsuo" "${SCRIPT_DIR}/envoy/bssl-compat/third_party/Tongsuo"
# fi

if [ $(uname -m) = "aarch64" ]; then
  # arm架构增加-fsigned-char选项
  bazel build \
  --@envoy//source/extensions/wasm_runtime/v8:enabled=false \
  --define build_hyperscan_mode=debug \
  --define build_modsecurity_mode=debug \
  --define build_sqlite3_mode=debug \
  --define build_cryptopp_mode=debug \
  --define build_Zstd_mode=debug \
  --define build_json_c_mode=debug \
  --define build_libev_mode=debug \
  --jobs 4 \
  -c dbg \
  --copt="-gstrict-dwarf" \
  --copt="-fsigned-char" \
  --explain=debug-explain.txt \
  --verbose_explanations \
  --strip=never \
  --verbose_failures \
  //:envoy || {
    echo "bazel build error!"
    exit
  }
else
 bazel build \
  --@envoy//source/extensions/wasm_runtime/v8:enabled=false \
  --@envoy//contrib/squash/filters/http/source:enabled=false \
  --@envoy//contrib/sxg/filters/http/source:enabled=false \
  --@envoy//contrib/kafka/filters/network/source:enabled=false \
  --@envoy//contrib/kafka/filters/network/source/mesh:enabled=false \
  --@envoy//contrib/postgres_proxy/filters/network/source:enabled=false \
  --@envoy//contrib/rocketmq_proxy/filters/network/source:enabled=false \
  --@envoy//contrib/vcl/source:enabled=false \
  --@envoy//contrib/cryptomb/private_key_providers/source:enabled=false \
  --@envoy//contrib/sip_proxy/filters/network/source:enabled=false \
  --@envoy//contrib/sip_proxy/filters/network/source/router:enabled=false \
  --@envoy//contrib/mysql_proxy/filters/network/source:enabled=false \
  --@envoy//contrib/smtp_proxy:enabled=true \
  --define build_hyperscan_mode=debug \
  --define build_modsecurity_mode=debug \
  --define build_sqlite3_mode=debug \
  --define build_cryptopp_mode=debug \
  --define build_Zstd_mode=debug \
  --define build_json_c_mode=debug \
  --define build_libev_mode=debug \
  --jobs 4 \
  -c dbg \
  --copt="-gstrict-dwarf" \
  --copt="-gdwarf-4" \
  --explain=debug-explain.txt \
  --verbose_explanations \
  --strip=never \
  --incompatible_strict_action_env=false \
  --verbose_failures \
  --define=boringssl=none \
  @envoy//contrib/exe:envoy || {
    echo "bazel build error!"
    exit
  }
fi

# 调试时源文件查找路径
rm external -f && ln -s $(bazel info output_base)/external external



# if [ $(uname -m) = "aarch64" ]; then
#   # arm架构增加-fsigned-char选项
#   bazel build \
#   --@envoy//source/extensions/wasm_runtime/v8:enabled=false \
#   --define build_hyperscan_mode=release \
#   --define build_modsecurity_mode=release \
#   --define build_sqlite3_mode=release \
#   --define build_cryptopp_mode=release \
#   --define build_Zstd_mode=release \
#   --define build_json_c_mode=release \
#   --define build_libev_mode=release \
#   --jobs 4 \
#   -c opt \
#   --copt="-fsigned-char" \
#   --verbose_failures \
#   //:envoy || {
#     echo "bazel build error!"
#     exit
#   }
# else
#  bazel build \
#   --@envoy//source/extensions/wasm_runtime/v8:enabled=false \
#   --@envoy//contrib/squash/filters/http/source:enabled=false \
#   --@envoy//contrib/sxg/filters/http/source:enabled=false \
#   --@envoy//contrib/kafka/filters/network/source:enabled=false \
#   --@envoy//contrib/kafka/filters/network/source/mesh:enabled=false \
#   --@envoy//contrib/postgres_proxy/filters/network/source:enabled=false \
#   --@envoy//contrib/rocketmq_proxy/filters/network/source:enabled=false \
#   --@envoy//contrib/vcl/source:enabled=false \
#   --@envoy//contrib/cryptomb/private_key_providers/source:enabled=false \
#   --@envoy//contrib/sip_proxy/filters/network/source:enabled=false \
#   --@envoy//contrib/sip_proxy/filters/network/source/router:enabled=false \
#   --@envoy//contrib/mysql_proxy/filters/network/source:enabled=true \
#   --define build_hyperscan_mode=release \
#   --define build_modsecurity_mode=release \
#   --define build_sqlite3_mode=release \
#   --define build_cryptopp_mode=release \
#   --define build_Zstd_mode=release \
#   --define build_json_c_mode=release \
#   --define build_libev_mode=release \
#   --jobs 4 \
#   -c opt \
#   --incompatible_strict_action_env=false \
#   --verbose_failures \
#   --define=boringssl=none \
#   @envoy//contrib/exe:envoy || {
#     echo "bazel build error!"
#     exit
#   }
# fi