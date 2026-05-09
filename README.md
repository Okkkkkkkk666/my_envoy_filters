# Envoy filters

该项目用来为dam项目提供envoy插件。
原有项目(dam-envoy)由于文件组织方式跟envoy本体融为一体，这样不利于后期单独升级envoy本体，所以建立了该项目。
该项目采用envoy官方推荐的第三方插件开发方式，将envoy本体做为该项目的子模块，这样就能很方便的切换envoy本体版本。

## 构建前的准备
首先需要安装依赖项，运行install-dependencies.sh即可自动安装依赖项。
```shell
./install-dependencies.sh
```
## 构建
构建静态二进制文件:

```shell
git submodule update --init --recursive
bazel build //:envoy
```

(推荐)也可以运行特定的脚本来构建特定的Envoy静态二进制文件:

- 构建调试版（支持断点，变量监控）

```shell
./build-debug.sh
```

- 构建发布版(生产环境)

```shell
./build-release.sh
```

## 测试

运行流控插件的单元测试：

```shell
bazel test //filters/test/extensions/filters/http/strong_local_ratelimit:filter_test
```

运行Envoy自身的单元测试：

```shell
bazel test @envoy//test/...
```

(推荐)也可以运行特定的脚本来构建单元测试：

```shell
# 构建流控插件的filter_test
./test-debug.sh strong_local_ratelimit:filter_test

# 构建流控插件的所有单元测试
./test-debug.sh strong_local_ratelimit:all

# 查看支持的构建参数
./test-debug.sh help
```

## 关于构建的工作原理

[Envoy repository](https://github.com/envoyproxy/envoy/) 做为本项目的子模块。

[`WORKSPACE`](WORKSPACE) 文件映射 `@envoy`  到本地路径。

[`BUILD`](BUILD) 文件引入了一个新的Envoy静态二进制目标，`envoy`，
它将新的过滤器和 `@envoy//source/exe:envoy_main_entry_lib` 链接在一起。 本项目实现的过滤器在Envoy二进制文件的静态初始化阶段将自己注册为一个新的过滤器。

## FAQ
请参看[FAQ](FAQ.md)，这里记录了一些团队成员遇到的一些常见问题。
