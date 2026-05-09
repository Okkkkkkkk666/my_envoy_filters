#!/bin/bash
set -e

PKG_MANAGER=apt
KYLING=0

if [ -e /etc/centos-release ]; then
   PKG_MANAGER=yum
   $PKG_MANAGER install -y epel-release
   echo centos
elif [ -e /etc/kylin-release ]; then
   PKG_MANAGER=yum
   KYLING=1
   echo kylin
fi

# install wget
$PKG_MANAGER install -y wget

# install boost-1.84.0
BOOST_VERSION=$(cat /usr/local/include/boost/version.hpp | grep '#define BOOST_VERSION ' | awk '{print $3}')
BOOST_MD5SUM="1a84c4e387f491dedc0ece83c64bc815"
function install_boost_1840() {
   if [[ ! -e boost-1.84.0.tar.gz || $(md5sum boost-1.84.0.tar.gz | awk '{print $1}') != $BOOST_MD5SUM ]]; then
      rm -rf boost-1.84.0.tar.gz
      wget https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz
   fi
   tar xvzf boost-1.84.0.tar.gz
   cd boost-1.84.0
   ./bootstrap.sh
   ./b2 install
   cd ..
}
if [ ! -e /usr/local/include/boost/version.hpp ]; then
   echo boost not found!
   install_boost_1840
elif [ "108400" != $BOOST_VERSION ]; then
   echo "boost version $BOOST_VERSION is invalid! required 108400!"
   install_boost_1840
fi

# install bazel
if [ ! -e /usr/local/bin/bazel ]; then
   wget -O /usr/local/bin/bazel https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-$([ $(uname -m) = "aarch64" ] && echo "arm64" || echo "amd64")
   chmod +x /usr/local/bin/bazel
fi

# for build envoy
$PKG_MANAGER install -y \
   autoconf \
   automake \
   cmake \
   curl \
   libtool \
   make \
   ninja-build \
   patch \
   python3-pip \
   unzip \
   virtualenv

# for build ModSecurity
if [ $KYLING == 1 ]; then
   $PKG_MANAGER install -y lmdb-devel pcre-devel libtool libxml2-devel yajl-devel pkgconf
else
   $PKG_MANAGER install -y build-essential libcurl4-openssl-dev liblmdb-dev libpcre++-dev libtool libxml2-dev liblzma-dev libyajl-dev pkgconf zlib1g-dev
fi

# for build Sqlite3
if [ $KYLING == 1 ]; then
  $PKG_MANAGER install -y tcl
else
  $PKG_MANAGER install -y tclsh
fi

# for build hyperscan
$PKG_MANAGER install -y ragel

# for bssl-compat
# clang llvm
if [ ! -e "/usr/bin/clang-16" ] || [ ! -e "/usr/lib/llvm-16" ]; then
   echo install llvm clang
   if [ $PKG_MANAGER == "apt" ]; then
      $PKG_MANAGER install -y wget gnupg software-properties-common
      wget -O- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
      # add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-16 main"
      add-apt-repository "deb https://mirrors.tuna.tsinghua.edu.cn/llvm-apt/focal/ llvm-toolchain-focal-16 main"
      $PKG_MANAGER update
      $PKG_MANAGER install -y llvm-16 llvm-16-dev llvm-16-tools clang-16 libclang-16-dev lld-16 gawk
   else
      rpm --import https://apt.llvm.org/llvm-snapshot.gpg.key
      tee /etc/yum.repos.d/llvm16.repo << EOF
[llvm16]
name=LLVM 16 for CentOS 7
baseurl=https://apt.llvm.org/centos/7/llvm-toolchain-16/x86_64/
enabled=1
gpgcheck=1
gpgkey=https://apt.llvm.org/llvm-snapshot.gpg.key
EOF
      $PKG_MANAGER install -y llvm-16 llvm-16-dev llvm-16-tools clang-16 libclang-16-dev lld-16 gawk
   fi
fi

# 铜锁
if [ ! -f "/tmp/Tongsuo.tar.gz" ]; then
  echo install tongsuo

  # 切换目录
  curdir=$(pwd)
  if [ ! -d "/tmp" ]; then
    mkdir /tmp
  fi
  cd /tmp

  # 拉铜锁代码并打包
  rm -rf Tongsuo
  git clone https://git.ouryun.cn/base/Tongsuo
  
  # 安装铜锁
  cd Tongsuo
  ./config --prefix=/usr/local --libdir=lib64 enable-ntls enable-zlib enable-zlib-dynamic enable-cert-compression enable-evp-cipher-api-compat 
  make -j4
  make install
  
  # 删除代码目录
  rm -rf Tongsuo

  cd $curdir
fi


