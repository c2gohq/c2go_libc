# 许可模式（预发布）

[English](LICENSING.md)

> **状态：预发布——尚未达到正式发布条件。**
>
> 本文档说明计划采用的许可模式以及公开发布前仍需完成的工作。本文档不是
> 法律意见，本身也不授予商业许可。

## 计划模式

`c2go-libc` 是一个包含多种许可证材料的作品，而不是一个全部由同一主体创作
的代码库。计划采用的发布模式是：

- c2go 原创材料：**GNU AGPL-3.0-only**，或作为替代方案另行签署商业协议。
- musl 派生材料：适用 musl 的 **MIT** 条款以及 musl 附带的逐文件宽松许可
  声明。
- 其他第三方派生材料：适用其原始许可条款。
- 混合文件中的 c2go 原创修改：采用 AGPL-3.0-only 或另行签署商业协议，
  但不改变底层第三方材料已有的权利。

标准 AGPL version 3 正文见 [LICENSE](LICENSE)。项目当前使用的完整 musl
`COPYRIGHT` 文件见
[LICENSES/musl-COPYRIGHT.txt](LICENSES/musl-COPYRIGHT.txt)。

项目有意选择 `AGPL-3.0-only`，而不是含义不明确的 `AGPL-3.0`，也不是
`AGPL-3.0-or-later`。逐文件声明尚未完成，所以该选择还没有在整个源码树中
得到一致表达。

## AGPL 方案的含义

AGPL 方案允许商业使用，并不表示每个商业用户都必须付费。只要用户能够并且
确实按照 AGPL 在具体使用方式中的适用要求履行义务，就可以依据 AGPL 使用
c2go 原创材料。

计划中的商业方案面向需要以其他条款使用 c2go 原创材料的用户。商业方案不能
重新许可、收回或限制 musl 或任何其他第三方组件已经授予的权利。详见
[COMMERCIAL-LICENSING.zh-CN.md](COMMERCIAL-LICENSING.zh-CN.md)。

目前不计划为 AGPL 路径提供公开的 runtime/library exception。增加此类例外
可能实质性改变商业边界，未经明确商业决策和法律审查不得加入。

## 许可证边界

许可证与目录之间**不是**一一对应关系。

| 区域 | 当前事实 | 计划处理方式 | 状态 |
| --- | --- | --- | --- |
| `musl/` | 修改后的 musl fork，以 submodule 固定在 `a31facd31f63...` | 保留 musl 及其附带第三方材料的原始条款 | 远端和 gitlink 已建立；逐文件声明清单未完成 |
| `csrc/*.c` | 混合了原创 wrapper、重写实现、musl 适配和其他来源适配 | 按文件处理，必要时按文件中的不同部分处理 | 来源审计未完成 |
| `csrc/include/` | 含 musl、Darwin/XNU、MinGW 和原创内容的带标注接口头文件 | 保留适用的上游权利，单独许可原创新增内容 | 来源审计未完成 |
| 根目录手写 `*.go`/`*.s` | 主要是 c2go 运行时桥接；大多缺少所有权声明 | 经审计后采用 AGPL-3.0-only 或商业许可 | 版权所有者和文件头尚未确定 |
| 生成的 `libc_*.go/.s` | 由混合来源输入生成的已跟踪产物 | 携带混合来源声明并引用所有适用声明 | 已跟踪；声明和可复现性工作未完成 |
| `dl/` | 当前 `c2go-bind` 所需的外部调用桥；依赖 PureGo v0.11.0-alpha.8，并匹配其 Unix 私有 dispatcher ABI | 保留 PureGo 的 Apache-2.0 权利和声明；只有独立拥有的 c2go 部分适用 c2go 许可模式 | 已迁移并固定版本；所有权和端到端发布审计仍未完成 |
| 测试/构建文档 | 主要是项目材料，但部分测试/数据来自以前的实现或第三方 | 不能在审计前统一指定单一许可证 | 未完成 |

目录位置、`Code generated` 标记或 c2go 版权声明都不能消除第三方权利。

## 生成文件

针对不同目标的 `.go` 和 Plan 9 `.s` 文件组合了很多源码文件生成的代码。
它们不能带有声称整个产物均由 c2go 所有或仅受 AGPL 约束的文件头。

未来的生成产物文件头应当表达以下实质内容：

```text
Generated artifact. Do not edit.

This file contains code generated from multiple source components. Original
c2go portions are available under AGPL-3.0-only or a separately executed
commercial agreement. Third-party portions remain under their original
licenses. See NOTICE and THIRD_PARTY_NOTICES.md.
```

生成器必须一致地输出该声明，发布产物也必须携带声明所引用的文件。

## 对应源码与可复现性

Go module 布局有意使嵌套的 `csrc` module 和 musl submodule 不进入常规
proxy zip。生成的 Plan 9 汇编不能替代用于修改它的首选源码形式。

因此，每个公开版本都需要另行提供经过验证、递归完整的源码包，其中包含或
明确固定以下内容：

1. 匹配的 `c2go-clang` 源码和构建身份；
2. 匹配的 `c2go-bind` 源码和构建身份；
3. 本仓库的发布 tag；
4. 精确的 musl fork commit；
5. `csrc`、头文件、生成脚本和生成产物；
6. 所有适用的许可证和第三方声明文件；
7. 足以复现生成文件的说明。

当前 checkout 不满足该发布条件。

## 贡献与商业再授权

进行商业双授权要求商业授权方对已接受贡献持有足够权利。本仓库目前没有完成
CLA、版权所有者法律身份或授权接受贡献的工作流。

在这些事项完成前，不得合并外部代码贡献。DCO 或 `Signed-off-by` 行本身不会
自动授予商业再授权所需的权利。详见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 发布阻断项

以下事项全部解决后，许可工作才算完成：

- 确定合法版权所有者和商业授权方；
- 添加准确的逐文件版权和许可声明；
- 验证所有已跟踪目标产物的确定性再生成和准确混合来源声明；
- 使 clean recursive checkout 在不依赖本地专用输入的情况下通过完整支持矩阵；
- 针对固定的 PureGo ABI 和 clean-checkout 生成消费者验证已迁移的
  `github.com/c2gohq/c2go_libc/dl` 子包；
- 恢复并验证 `csrc/termios.c` 中 Apple Libc/FreeBSD 派生代码所需的声明；
- 确定 XNU 派生 Darwin ABI 材料和其他 MinGW/Darwin 定义的来源及适用
  处理方式；
- 清点通过所选 musl 源码引入的 TRE、math、qsort 和其他逐文件声明；
- 发布完整对应源码和可复现构建说明；
- 完成 CLA 和贡献政策；
- 取得合格的开源/IP 律师审查。

证据和实时审计表见 [PROVENANCE.md](PROVENANCE.md)。
