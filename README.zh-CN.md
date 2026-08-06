# c2go-libc

[![Go Reference](https://pkg.go.dev/badge/github.com/c2gohq/c2go_libc.svg)](https://pkg.go.dev/github.com/c2gohq/c2go_libc)

[English](README.md)

> **预发布状态——尚未达到正式发布条件**
>
> 本仓库正在进行发布、来源和许可整理，项目尚未指定稳定 Go module tag 或
> 正式版本。任何自行再分发该快照的人都必须独立履行全部适用许可证和声明
> 义务。详见[发布阻断项](#发布阻断项)。

`c2go-libc` 是 c2go 工具链为转换后的程序提供的运行时 C 标准库兼容层。
它组合经过选择的 musl 派生实现、c2go 专用 C 适配代码和 Go 运行时桥接，
再生成针对不同目标平台的 Go 绑定与 Plan 9 汇编。

它**不是**宿主系统 libc 的直接替代品，也不是可以独立使用的通用 Go
库：它的 ABI、linkname、生成文件和支持的 Go 版本必须与对应版本的
`c2go-clang`、`c2go-bind` 匹配。

## 在 c2go 工具链中的位置

```text
C 源码和带标注的头文件
        |
        v
c2go-clang -fc2go        -> 每个翻译单元的 LLVM bitcode
        |
        v
c2go-lto                 -> 整包 Plan 9 汇编 + manifest
        |
        v
c2go-bind                -> 目标平台专用 Go 绑定
        |
        v
c2go-libc + 转换后的包   -> Go 链接器/运行时
```

三个 c2go 项目构成一套版本匹配的发行单元：

- **c2go-clang** 负责 C/LLVM 转换流水线和 c2go ABI 降低。
- **c2go-bind** 将产出的 manifest 和汇编转换成 Go 包。
- **c2go-libc** 提供转换后包所依赖的 libc 兼容接口和 Go 运行时桥接。

Go module path 有意保持为：

```text
github.com/c2gohq/c2go_libc
```

其中下划线已经写入现有 linkname，与面向用户的仓库名 `c2go-libc`
不同，不能仅为了外观而修改。

## Managed libc 变体（`mlib`）

根包继续保留基于 unmanaged handle ID 的兼容接口。新的 `mlib` 子包用于提供
有状态 API 的 managed 版本：C 对象直接保存 Go GC 可见的状态指针，因此不再
需要全局 ID 到指针的注册表。目前已实现无名信号量；pthread 生命周期、key 与
同步对象；完整的 managed 目录传播簇（`DIR`、`scandir`、`nftw`/`ftw`、
`glob`）；标准、显式、内存、自定义、宽字符和进程流；以及 managed
`search.h` 树、哈希表和队列容器；以及由每对象 GC arena 持有的 POSIX
正则表达式。

默认名称显式带 `mlib_` 前缀，例如 `mlib_sem_t`、`mlib_sem_init`、
`mlib_pthread_mutex_t`、`mlib_pthread_mutex_lock`、`mlib_DIR`、
`mlib_opendir`。如果在第一次包含 mlib
头文件前定义 `C2GO_MLIB_UNPREFIXED`，则会改为对应的标准名称。这个开关作用于
整个 C2Go/LTO 包，不能在同一包里混用两套路由。含指针的 managed record 必须使用
带类型信息的 `gc_malloc(c2go_typeinfo(T), sizeof(T))`；无指针 buffer 可以使用
`gc_malloc(NULL, size)`，type-erased 对象图还必须有 regex arena 这类显式 owner。
它们都不能使用普通 `malloc`。managed
`scandir`、`glob` 和 managed search 容器均由 GC 持有，不能传给 `free`。
按字节处理元素的 `lsearch`/`lfind` 仍只提供 root 版本，并且只能用于不含指针
的元素。示例和约束见
[mlib/README.md](mlib/README.md)。

当前基于 handle table 的 carrier 迁移已经闭环，但 managed 分配接口仍未完成。
返回新字节缓冲区的函数与内部持有指针的对象图需要单独迁移。managed POSIX regex
已经通过选择性编译第二份 TRE 实例实现：所有分配都经过 `gc_malloc`，并由每个
`regex_t` 对应的 Go arena 直接持有每个 no-scan block，不依赖 TRE record 内隐藏的
指针维持存活。
`iconv` 是有意保留的 root-only 例外：精确兼容 POSIX 必须支持非指针值
`(iconv_t)-1`，它不能存入 precise-GC managed pointer slot。因此 mlib 是 managed
ownership 接口层，而不是把 root libc 的每个函数机械复制一份。使用时应明确包含所需
的 `<c2go/mlib/...>` family header；这个尚未完整覆盖 libc 的接口层不提供聚合头文件。

## 当前目标清单

生成器目前包含以下目标：

| GOOS | GOARCH | 运行时产物 | C selftest 产物 |
| --- | --- | --- | --- |
| Darwin | arm64 | 已跟踪的生成产物 | 已跟踪的生成产物 |
| Darwin | amd64 | 已跟踪的生成产物 | 已跟踪的生成产物 |
| Linux | arm64 | 已跟踪的生成产物 | 已跟踪的生成产物 |
| Linux | amd64 | 已跟踪的生成产物 | 已跟踪的生成产物 |
| Windows | amd64 | 已跟踪的生成产物；部分外部符号使用 `msvcrt` | 不生成 |

该表表示代码生成意图，**不是稳定支持承诺**。仓库还没有从 clean clone
开始验证这些目标的 CI 矩阵。Windows 的 POSIX 接口范围也小于 Darwin
和 Linux。

当前 module 声明 Go 1.25.0。中央 `c2goabi` provider 以 toolchain contract
epoch 1 放行 Go 1.25.x 和 Go 1.26.x；在完成契约验证前，Go 1.27 及以后版本
会被集中拒绝。schema v2 生成包本身不再写死这个上界：如果后续 Go 版本仍
保持 contract epoch 1，只需升级 c2go-libc，目标库无需变化。当前 C2Go ABI
epoch 范围为 `1..1`。

## 仓库结构

```text
.
├── go.mod                         位于仓库根目录的 Go module
├── *.go                           手写 Go 运行时桥接
├── libc_<os>_<arch>.go/.s         生成的发布产物
├── sjlj_<arch>.s                  手写 setjmp/longjmp 支持
├── musl/                          固定 commit 的 c2go musl fork submodule
├── csrc/                          C 适配、原创 C 和头文件
├── internal/posixsync/            libc 与 mlib 共用的状态算法
├── mlib/                          managed libc 包、头文件和自测试
├── selftest/                      C 内部回调/比较器测试
├── CMakeLists.txt                 供 gen.sh 读取的源码清单
├── gen.sh                         实际再生成驱动
└── LICENSE*, NOTICE, *LICENSING*  许可和来源记录
```

重要细节：

- `CMakeLists.txt` 当前只是清单，没有定义可直接执行的 CMake build target。
- `musl/` 是 `https://github.com/c2gohq/musl.git` 的已提交 submodule，
  固定到其 `c2go` 分支的精确 commit。
- `csrc/` **不是**许可证边界，其中同时包含原创代码和第三方派生适配。
- 五个目标的运行时生成产物和四个 Darwin/Linux 目标的 selftest 生成产物均已
  跟踪；其确定性再生成和混合来源声明仍需在发布前验证。
- `dl/` 提供当前 `c2go-bind` 生成代码所需的外部原生调用边界。Unix 调用使用
  固定版本的 PureGo trampoline；Windows 使用 Win32 loader 和 syscall API。

基于证据的源码映射见 [PROVENANCE.md](PROVENANCE.md)。

## 本地维护者工作流

目前没有面向最终用户的受支持安装命令。在下面的阻断项全部关闭并发布
不可变版本前，`go get github.com/c2gohq/c2go_libc` 不是有效的发布安装
说明。

在现有维护者 checkout 中，由 `gen.sh` 驱动再生成：

```bash
CLANG=/absolute/path/to/c2go-clang \
C2GOLTO=/absolute/path/to/c2go-lto \
C2GOBIND=/absolute/path/to/c2go-bind \
./gen.sh

go test ./...
```

`CLANG`、`C2GOLTO` 和 `C2GOBIND` 必须来自相互兼容的 c2go 工具链。
当前脚本仍含本机专用默认路径，因此调用方应显式覆盖三个变量。

再生成前请先执行 `git submodule update --init` 初始化 `musl/`。
这些命令不能证明完整的 clean-clone 矩阵已经通过。

## 测试状态

在 2026-07-28 的验证快照中：

- 在 Darwin arm64、Go 1.25.9 下，当前 checkout 以及使用已跟踪生成文件的
  clean `git archive HEAD` 解包目录均通过 `go test ./...`。
- `go vet ./...` 未通过：标准汇编分析器会报告 C2Go 生成的非标准参数 frame
  和缺失的 Go 声明，另有一个测试报告可能误用 `unsafe.Pointer`。发布前必须
  制定并执行经过复核的 vet/静态分析策略，不能静默忽略该结果。
- clean recursive clone 可以初始化已固定的 musl submodule。Git archive
  只含 gitlink 目录而不含 submodule 内容，因此 archive 测试只能证明已跟踪
  产物可用，不能证明可复现再生成或对应源码完整。
- 当前没有仓库 CI 工作流证明全部五个生成目标。
- `dl` 单元测试已在 Darwin arm64、`CGO_ENABLED=0` 下通过；Darwin amd64
  和 Linux amd64/arm64 包可交叉编译，Windows/amd64 loader 及整数/浮点调用
  测试已在 Wine 7.7 下通过。发布前仍需对生成消费者执行 clean-checkout
  端到端门禁。

在 2026-08-03 的 contract provider 更新中，Go 1.26.0 已在 Darwin arm64
原生通过根包、`dl`、`selftest`，并通过启用 `GOGC=1` 与
`GODEBUG=invalidptr=1` 的 SQLite 消费者测试。协同 toolchain 的 release
workflow 现在要求在开始打包前，分别于 Linux amd64、Linux arm64、Windows
amd64 和 macOS arm64 原生 runner 上运行同一组 Go 1.26.x runtime 测试。

发布声明必须依据 clean checkout 和转换后消费者的端到端测试，不能只依据
准备过的工作树。

## 范围与已知限制

实现有意只暴露在对应目标上确实存在实现的函数和声明。当前设计限制包括：

- 仅有一个 `C.UTF-8` locale，不含消息目录；
- 通过 Go 运行时传递信号，而不是无限制替换原生信号处理器；
- 不提供通用 `fork`/`exec` 系列；
- Windows 上的 POSIX 接口范围较小；
- 部分格式化 I/O 路径将 `long double` 按 `double` 处理；
- 不支持 32 位目标；
- 与受支持的 Go 版本和 c2go ABI epoch 紧密绑定。

这些限制应当作为当前运行时契约的一部分，不应使用虚假的成功 stub 静默补齐。

## 发布阻断项

至少完成以下事项之前，项目不会将本仓库标记为**达到正式发布条件**：

1. 验证确定性再生成，并为每个已跟踪生成产物添加准确的混合来源声明。
2. 让完整递归 clean checkout 构建并通过完整测试矩阵。
3. 删除生成器对本机路径的假设，并记录可复现的工具链引导过程。
4. 从匹配工具链的 clean checkout 端到端验证 `c2go_libc/dl` 的
   unmanaged-extern 和 callback 消费者，包括固定的 PureGo ABI。
5. 恢复并核实 `csrc/termios.c` 中 Apple Libc/FreeBSD 派生 Darwin
   代码所需的声明。
6. 解决 XNU 派生 Darwin ABI 材料和其他第三方定义的来源及许可处理。
7. 完成逐文件版权/许可头和生成产物声明。
8. 解决或明确复核当前 `go vet` 发现，并使 release 静态分析 gate 可复现。
9. 发布包含生成产物所用精确源码的完整对应源码包。
10. 记录并核实已经选定的自然人许可方之法定身份和权属，完成商业条款和 CLA，
    并取得合格的开源/IP 法律审查。

详细清单见 [PROVENANCE.md](PROVENANCE.md) 和
[LICENSING.zh-CN.md](LICENSING.zh-CN.md)。

## 许可证

计划采用的模式是：

- 经核实的 c2go 原创材料：**GNU AGPL-3.0-only**，或另行签署的商业协议；
- musl 派生材料：musl MIT 条款及各源文件附带声明；
- 其他所有第三方材料：各自原始适用条款。

遵守 AGPL-3.0-only 条件时可以商业使用。本仓库不能声称所有商业使用都必须
付费。未来商业协议只能对授权方有权许可的材料授予替代权利，不能改变 musl
或其他第三方材料已有的权利。

GitHub Sponsors 可以在另行签署的商业协议中被指定为付款渠道；赞助付款本身
不会产生授权。

由于逐文件审计尚未完成，当前许可/来源文档有意标记为预发布。任何自行再分发
该快照的人都必须遵守全部适用许可证，且不能将当前文档视为最终声明包。

请阅读：

- [LICENSE](LICENSE)——GNU AGPL version 3 正文；
- [NOTICE](NOTICE)——当前多许可证预发布声明；
- [LICENSING.zh-CN.md](LICENSING.zh-CN.md)——计划模式和发布条件；
- [COMMERCIAL-LICENSING.zh-CN.md](COMMERCIAL-LICENSING.zh-CN.md)——商业授权意图及当前
  尚未构成要约的状态；
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)——尚未完成的第三方清单；
- [PROVENANCE.md](PROVENANCE.md)——审计证据和未解决边界。

这些文档不是法律意见。最终发布需要合格的法律审查。

## 贡献

由于版权所有者、CLA 和商业再授权流程尚未确定，目前不接受外部代码贡献合并。
可以通过未来公开仓库所建立的 issue 渠道提供可复现 bug 报告和原创文档反馈。

提交任何材料前请先阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。
