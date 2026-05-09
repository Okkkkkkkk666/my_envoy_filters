[1.为什么编译项目时代码没有发生变化，还是需要重新编译？](#1为什么编译项目时代码没有发生变化还是需要重新编译)

[2.怎么使用vscode调试项目？](#2怎么使用vscode调试项目)

[3.为什么断点断不下来？](#3为什么断点断不下来)

[4.为什么调试过程中查看调用栈中的某个函数所在文件时显示不存在该文件？](#4为什么调试过程中查看调用栈中的某个函数所在文件时显示不存在该文件)

[5.为什么高并发时Envoy会崩溃？](#5为什么高并发时envoy会崩溃)

[6.vscode中源码中包含的头文件明明找得到该文件但为什么还有红色波浪线？](#6vscode中源码中包含的头文件明明找得到该文件但为什么还有红色波浪线)

[7.waf插件编译时为什么有些机器需要链接libyajl和libGeoIP而有些机器却不需要链接？](#7waf插件编译时为什么有些机器需要链接libyajl和libgeoip而有些机器却不需要链接)

[8.使用bazel build -c dbg 生成调试版时，为什么无法查看变量的值？](#8使用bazel-build--c-dbg-生成调试版时为什么无法查看变量的值)

[9.怎么使用远程的xDS配置，进行本地调试？](#9怎么使用远程的xds配置进行本地调试)

[10.从gcc9.4如何升级到gcc13.2以支持C++23？](#10从gcc94如何升级到gcc132以支持c23)

## 1.为什么编译项目时代码没有发生变化，还是需要重新编译？

由于环境变量发生变化，导致bazel重新编译。比如一直在windows中的wsl终端中编译，那么相关的环境变量是恒定的，只要代码不发生变化就不会重新编译，而此时如果切换到vscode内嵌的终端中进行编译的话，那么由于PATH环境变量发生变化，导致bazel重新编译。两个终端的PATH变量区别如下：

- windows wsl 终端

```shell
echo $PATH
/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/usr/lib/wsl/lib:/mnt/c/WINDOWS/system32:/mnt/c/WINDOWS:/mnt/c/WINDOWS/System32/Wbem:/mnt/c/WINDOWS/System32/WindowsPowerShell/v1.0/:/mnt/c/WINDOWS/System32/OpenSSH/:/mnt/c/Program Files/Git/cmd:/mnt/c/Program Files/TortoiseGit/bin:/mnt/c/Program Files/PuTTY/:/mnt/c/Program Files/dotnet/:/mnt/c/strawberry/c/bin:/mnt/c/strawberry/perl/bin:/mnt/c/Users/zy/AppData/Local/Programs/Python/Python310:/mnt/c/Users/zy/AppData/Local/Microsoft/WindowsApps:/mnt/c/Users/zy/bazel:/mnt/c/Users/zy/AppData/Local/Programs/Python/Python310/Scripts:/mnt/d/Program Files/Microsoft VS Code/bin:/snap/bin:/usr/local/go/bin:/root/github.com/cilium/proxy/bazel-bin:/root/github.com/cilium/cilium/cilium:/root/go/bin
```

- vscode 内嵌终端

```
echo $PATH
/root/.vscode-server/bin/441438abd1ac652551dbe4d408dfcec8a499b8bf/bin/remote-cli:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/usr/local/go/bin:/root/github.com/cilium/proxy/bazel-bin:/root/github.com/cilium/cilium/cilium:/root/go/bin
```

所以，我们编译项目时一定要保证要么一直都在windows中的wsl终端中编译，要么一直在vscode内嵌终端中编译，不要来回切换。

## 2.怎么使用vscode调试项目？

使用[build-debug.sh](build-debug.sh)脚本编译后，在vscode运行和调试(Ctrl+Shift+D)页面创建launch.json文件后，点击调试（F5）即可调试。launch.json文件类似如下：

```json
    "configurations": [
        {
            "name": "envoy",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/bazel-bin/envoy",
            "args": [
                "-c",
                "${workspaceFolder}/config/envoy-demo.yaml"
            ],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            // "sourceFileMap": {
            //     "/proc/self/cwd": "${workspaceFolder}"
            // },
            "setupCommands": [
                {
                    "description": "为 gdb 启用整齐打印",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                },
                {
                    "description": "将反汇编风格设置为 Intel",
                    "text": "-gdb-set disassembly-flavor intel",
                    "ignoreFailures": true
                }
            ]
        },
```

几个关键字段说明：

- program：要调试的文件路径，其中${workspaceFolder}变量表示vscode打开的文件夹根目录，bazel-bin目录是bazel自动生成的输出目录。
- args：启动参数。注意，参数不支持空格，如"-c envoy-demo.yaml"这样的形式是不支持的。config文件夹在项目根目录自行创建，因为每个人的测试配置文件都不尽相同，所以这些配置文件没有添加到git管理中，大家自由管理自己的配置文件。
- cwd：被调试进程的工作目录。
- sourceFileMap：源码文件重定向。一般情况下无需指定，因为在[`build-debug.sh`](build-debug.sh)脚本中已经在项目根目录自动生成了一个external的软链接，该软链接解决调试时源码路径的查找问题。

## 3.为什么断点断不下来？

检查launch.json文件中，sourceFileMap字段是否配置正确。当文件映射关系不对时，断点是无法断下的。

## 4.为什么调试过程中查看调用栈中的某个函数所在文件时显示不存在该文件？

根据vscode反馈的信息查看不存在文件的路径，在launch.json文件中，对sourceFileMap字段添加相应的源码重定向。

## 5.为什么高并发时Envoy会崩溃？

系统限制了允许打开的文件描述符数量，而一个socket也是一个文件，当高并发时链接数量增多导致打开的socket超过系统限制的打开文件数量，就会引发envoy的错误。解决方法：

```shell
# 查看系统的相关限制
ulimit -a 
```

运行后输出：

```
core file size          (blocks, -c) 0
data seg size           (kbytes, -d) unlimited
scheduling priority             (-e) 0
file size               (blocks, -f) unlimited
pending signals                 (-i) 63964
max locked memory       (kbytes, -l) 65536
max memory size         (kbytes, -m) unlimited
open files                      (-n) 1024
pipe size            (512 bytes, -p) 8
POSIX message queues     (bytes, -q) 819200
real-time priority              (-r) 0
stack size              (kbytes, -s) 8192
cpu time               (seconds, -t) unlimited
max user processes              (-u) 63964
virtual memory          (kbytes, -v) unlimited
file locks                      (-x) unlimited
```

open files 系统默认是1024，我们将此值改大即可:

```shell
ulimit -n 1000000
```

执行完毕后只对当前终端有效，如果想一直生效的话可以将此命令写入/etc/profile中。

## 6.vscode中源码中包含的头文件明明找得到该文件但为什么还有红色波浪线？

直接禁用波浪线简单粗暴，但不推荐。之所以有该问题是由于该文件包含的其他头文件vscode无法找到，可以通过c_cpp_properties.json文件来指定vscode搜索头文件的目录来解决。c_cpp_properties.json文件可以通过点击vscode右下角的“C/C++配置”，然后再选择“编辑配置（JSON）”打开，其内容类似如下：

```json
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "/root/.cache/bazel/_bazel_root/**"
            ],
            "defines": [],
            "compilerPath": "/usr/bin/g++-9",
            "cStandard": "c17",
            "cppStandard": "c++17",
            "intelliSenseMode": "linux-gcc-x64",
            "browse": {
                "databaseFilename": "${workspaceFolder}/.vscode/browse.vc.db"
            }
        }
    ],
    "version": 4
}
```

在"includePath"字段指定对应的头文件目录即可。

## 7.waf插件编译时为什么有些机器需要链接libyajl和libGeoIP而有些机器却不需要链接？

这是因为编译ModSecurity子模块时，调用的configure脚本其内部会根据当前系统安装的库的情况来启用/禁用代码的一些功能。比如，当前系统没有安装libyajl-dev，那么configure就不会启用相关的代码，所以链接时就不会链接libyajl库。为了保持大家的开发环境一致，应该在编译项目前执行一次项目根目录的[install-dependencies.sh](install-dependencies.sh)脚本。

## 8.使用bazel build -c dbg 生成调试版时，为什么无法查看变量的值？
首先需要了解的是bazel支持3种编译模式：
- fastbuild: -O0, 编译速度最快
- opt: -02 -DNDEBUG -ggdb3 -gsplit-dwarf 发布版本
- dbg: -O0 -ggdb3 -gsplit-dwarf 调试版本
我们看到调试版本有个编译器参数 -gsplit-dwarf ，该参数将调试信息从编译后的obj文件种剥离出来，以此加快链接速度，以及加快调试器加载可执行程序的速度。但是在调试的过程中，却只能下断点，而无法查看变量的值。所以在我们项目早期的编译脚本中我们的解决方案是采用如下构建命令：
```bash
bazel build --define build_modsecurity_mode=debug --jobs 4 --copt="-O0" --copt="-ggdb" --explain=fast-explain.txt --verbose_explanations --strip=never --verbose_failures //:envoy
```
该构建命令的几个关键点：
- 采用默认的fastbuild模式，以此来避免应用 -gsplit-dwarf 编译器参数。
- 为了支持调试，增加了 -O0 -ggdb --strip=never
这样，编译出来的可执行文件包含了完整的调试信息，不但可以下断点也可以查看变量的值。唯一的缺点是链接速度慢，生成的可执行程序体积大（将近3G）,调试器加载启动时速度慢。为了彻底解决这个一问题，我们采用新的构建命令：
```bash
bazel build --define build_modsecurity_mode=debug --jobs 4 -c dbg --copt="-gstrict-dwarf" --explain=fast-explain.txt --verbose_explanations --strip=never --verbose_failures //:envoy
```
该构建命令继续采用dbg模式，为了解决无法查看变量的值的问题，加入 -gstrict-dwarf 编译参数，这样完美解决。需要注意的是，我们调试时的工作目录需指定为项目的根目录。
另外，附上解决这一问题的思路过程：
首先，使用dbg模式生成envoy
```bash
bazel build --define build_modsecurity_mode=debug --jobs 4 -c dbg --explain=fast-explain.txt --verbose_explanations --strip=never --verbose_failures //:envoy
```
接着，调试时的工作目录指定为项目根目录，并在根目录建立 external 的软链接，以便调试时的源码能正确找到。
此时，下一断点，并用浏览器访问envoy建立的网关，使断点命中，发现调试器没有任何提示直接退出了。
接着，删掉bazel-out软链接，再次启动调试，再次浏览器访问envoy建立的网关，使断点命中，这一次调试器并没有退出，并且在断点处停了下来，但是无法查看变量的值。
为了搞明白为什么会这样，需要vscode输出更多的信息，在launch.json中加入:
```json
    "logging": {
                "engineLogging": true,
                "trace": true,
                "traceResponse": true
            },
```
恢复删掉的bazel-out软链接，再次调试，发现调试器退出前，输出了一段日志：
```
Error:DW_FORM_strp pointing outside of .debug_str section
```
很明显gdb出错了。
在社区找到了一个类似的issue,但此issue快三年了依旧没有解决：[#13578](https://github.com/envoyproxy/envoy/issues/13578)
看来只能自己想办法。
接下来使用命令查看任意的一个*.dwo文件的dwarf调试信息：

```bash
readelf -w bazel-bin/filters/source/extensions/filters/http/acl/_objs/acl_filter_lib/acl_filter.pic.dwo > /dev/null
```
发现大量出错信息,关键有一行
```
Only GNU extension to DWARF 4 or 5 of .debug_macro.dwo is currently supported
```
大意是.debug_macro只在扩展中受支持。那么直接禁用扩展是不是就解决了呢？于是翻看gcc文档，找到了-gstrict-dwarf 选项，加入该编译选项后问题解决。
总结：
由于在.dwo文件中启用扩展加入了.debug_macro，导致gdb调试过程中在使用bt 命令打印调用栈时gdb出错，vscode就直接退出了调试。禁用扩展使.dwo文件中不包含.debug_macro扩展即可解决此问题。最终构建命令如下：

```bash
bazel build --define build_modsecurity_mode=debug --jobs 4 -c dbg --copt="-gstrict-dwarf" --explain=fast-explain.txt --verbose_explanations --strip=never --verbose_failures //:envoy
```
---
后记：gcc升级到13.2后envoy断点无法生效，经测试需将调试信息格式从默认的-ggdb3改成-gdwarf-4才行。如下：
```bash
bazel build --define build_modsecurity_mode=debug --jobs 4 -c dbg --copt="-gstrict-dwarf" --copt="-gdwarf-4" --explain=fast-explain.txt --verbose_explanations --strip=never --verbose_failures //:envoy
```

## 9.怎么使用远程的xDS配置，进行本地调试？
1、准备好xds配置文件

```
admin:
  address:
    socket_address:
      address: 127.0.0.1
      port_value: 9902
node:
  id: default~local 
  cluster: xds_cluster
  metadata:
    role: default~local
dynamic_resources:
  ads_config:
    api_type: GRPC
    transport_api_version: V3
    grpc_services:

    - envoy_grpc:
        cluster_name: xds_cluster
    cds_config:
        resource_api_version: V3
        ads: {}
    lds_config:
        resource_api_version: V3
        ads: {}
      static_resources:
    clusters:
        - type: STATIC
        typed_extension_protocol_options:
          envoy.extensions.upstreams.http.v3.HttpProtocolOptions:
              "@type": type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions
              explicit_http_config:
            http2_protocol_options: {}
        name: xds_cluster
        load_assignment:
          cluster_name: xds_cluster
          endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: 192.192.100.44
                  port_value: 9977
```
其中，192.192.100.44是dam的部署机器，9902端口是envoy的管理端口，9977端口是envoy与dam的通讯端口，在用xds进行本地调试时，配置文件的其他字段可以不用改，envoy的管理端口与本地不冲突的情况下一般默认为9902，dam的 ip和端口以实际想远程连接的机器为准。下面统一以上方的示例配置文件来说明。

2、端口转发

生产或测试环境dam的端口默认是没有转发出来的，本地envoy要与远程机器的dam通讯，需要用以下命令将dam端口转发出来

```bash
ssh -L 192.192.100.44:9977:127.0.0.1:9977 root@localhost
```

另外，为防止测试请求时断点不能正常触发，需要将网关上的浮动IP改为通配IP  0.0.0.0

3、启动本地envoy进行调试遇到的问题

适配好xds配置文件，将dam端口映射出来，直接启动本地编译的envoy debug版本进行调试，需要注意日志的配置，如果dam没有配置好远程日志服务器，总控上创建网关的时候默认是启动日志的，此时envoy会运行异常，可以在总控上的网关配置  “业务访问日志服务”选择否来规避。也可以手动在本机建立文件夹/var/log/envoy来规避。


4、配置查看

本地能正常的调试后，可以通过本地envoy的管理端口9902，来查看dam下发的配置  : 在浏览器上输入 127.0.0.1:9902，进入config_dump

## 10.从gcc9.4如何升级到gcc13.2以支持C++23？
1、 下载gcc13.2源码
```shell
wget https://ftp.tsukuba.wide.ad.jp/software/gcc/releases/gcc-13.2.0/gcc-13.2.0.tar.gz
```
2、解压gcc13.2源码
```shell
tar xvzf gcc-13.2.0.tar.gz
```
3、进入源码根目录下载依赖组件
```shell
cd gcc-13.2.0
./contrib/download_prerequisites
```
4、安装依赖组件
```shell
apt install flex
```
5、配置生成makefile
```shell
mkdir build
cd build
# 如果../configure失败提示"cannot find crt1.o"，则需要安装gcc-multilib
# apt install gcc-multilib
../configure
```
6、编译及安装
```shell
make -j8
make install
```
7、检查各个版本是否正常
```shell
gcc -v
g++ -v
c++ -v
```
8、对于之前已经用gcc9.x编译过的项目需要先清理后再重新编译
```shell
bazel clean
```

