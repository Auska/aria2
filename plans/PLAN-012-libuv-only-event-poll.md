# 任务：事件轮询只保留 libuv，清理 epoll/poll/select 后端与 `--event-poll`

## 目标

下载引擎的事件轮询从「epoll（默认）/poll/select/libuv 四选一」改为「只有 libuv」：

- 删除 `src/EpollEventPoll.{cc,h}`、`src/PollEventPoll.{cc,h}`、`src/SelectEventPoll.{cc,h}` 六个文件。
- `src/DownloadEngineFactory.cc` 无条件构造 `LibuvEventPoll`。
- libuv 从可选依赖（`--with-libuv`，默认 no）变成必需依赖；删 `--disable-epoll`、`HAVE_EPOLL`、`HAVE_LIBUV` 宏与 `AM_CONDITIONAL`。
- 完全删除 `--event-poll` 选项（prefs、usage text、OptionHandler、文档、bash completion）。
- **不动**：libcares 异步 DNS、openssl/sqlite3/zlib/jemalloc/wslay 等其他依赖与开关；`HAVE_POLL` 函数检测保留（`src/SocketCore.cc:596,639` 用 `poll(2)`，与事件轮询无关）。

## 当前上下文

- 分支 `slim-features`，HEAD `167656ef`。门禁现状：`autoreconf -i` → `./configure --enable-werror` → `make -j8 check` → `OK (911)`，0 error / 0 C++ warning，公开头 ABI 哈希 `132c72d9f055756769456c35ca7f8e7a879fbaa8`。
- 本机 `pkg-config libuv` → 1.50.0。`g++`（GCC 14）、`clang-18` 可用。
- `src/Makefile.am` 源列表：`SelectEventPoll.cc SelectEventPoll.h` 在基表 `:218`；`EpollEventPoll` 在 `if HAVE_EPOLL` 块 `:290-292`；`PollEventPoll` 在 `if HAVE_POLL` 块 `:526-528`；`LibuvEventPoll` 在 `if HAVE_LIBUV` 块 `:530-532`。
- `configure.ac`：`:39` `ARIA2_ARG_WITH([libuv])`（`m4/aria2_arg.m4` 里 `ARIA2_ARG_WITH` 默认 **no**，所以现在默认构建走 epoll）；`:49` `ARIA2_ARG_DISABLE([epoll])`；`:163-173` libuv 探测 + `AC_DEFINE([HAVE_LIBUV])` + `AM_CONDITIONAL([HAVE_LIBUV])`；`:437-444` epoll 探测；`:460` `AM_CONDITIONAL([HAVE_POLL])`（保留）；`:587` 摘要里 `Epoll: $have_epoll`。
- `src/DownloadEngineFactory.cc`：`:61-71` 四个后端的条件 include；`:81-116` `createEventPoll(Option* op)` 按 `PREF_EVENT_POLL` 分派，两处错误信息都带 `" Try --event-poll=select"`；`:124` 调用 `createEventPoll(op)`。
- 选项定义三处：`src/prefs.cc:139-142` `V_LIBUV/V_EPOLL/V_POLL/V_SELECT`、`src/prefs.cc:265` `PREF_EVENT_POLL`；`src/prefs.h:96-99,218` 对应 extern；`src/usage_text.h:499-500` `TEXT_EVENT_POLL`；`src/OptionHandlerFactory.cc:271-295` 的 `ParameterOptionHandler` 块（默认值 + 取值列表全按宏条件拼装）。
- 测试引用：`test/RequestGroupManTest.cc:20,52`、`test/RpcMethodTest.cc:6,92` 直接 `make_unique<SelectEventPoll>()`；`test/SessionSerializerTest.cc:15` 是死 include（该文件再无 EventPoll 引用）。
- 文档/补全：`doc/manual-src/en/aria2c.rst:1223-1228` 的 `--event-poll` 条目（正文还没列 libuv）；`doc/bash_completion/aria2c:34-36` 硬编码 `'epoll poll select'`；生成脚本 `doc/bash_completion/make_bash_completion.py:96` 在「带参数选项」白名单里列了 `--event-poll`（选项删掉后该键会让脚本 KeyError）。
- 构建脚本/CI：`build_test.sh:57-58` 两条 `--disable-epoll` 组合；`.github/workflows/build.yml` 的 apt 列表**没有** `libuv1-dev`（现在靠 select/epoll 构建通过），libuv 变必需后必须补。
- 选项被删后的兼容语义（已核对 `src/OptionParser.cc`）：命令行传未知选项 → `UNKNOWN_OPTION_EXCEPTION` 直接退出（`:68` 注释、`:207`）；conf 文件 / API KeyVals 里的未知键只 `A2_LOG_WARN("Unknown option: ...")`（`:250`、`:264`）。
- `config.h.in`、`Makefile.in`、`configure` 均未被 git 跟踪，改 `.am`/`.ac` 后重跑 `autoreconf -i` 即可，不入库。

## 待确认决策

无（已确认）：

| 决策点 | 结论 | 状态 |
|---|---|---|
| 清理范围 | 只做事件轮询后端；DNS/其他依赖不动 | 已确认（用户：留 libuv 但不动 DNS/其他依赖） |
| `--event-poll` 选项 | **完全删除**（命令行传它报 Unknown option 退出，conf/API 只警告） | 已确认 |
| configure 开关 | **libuv 硬依赖**，删 `ARIA2_ARG_WITH([libuv])`、`--disable-epoll`、`HAVE_EPOLL`、`HAVE_LIBUV` 宏与 `AM_CONDITIONAL`；保留 `AC_CHECK_FUNCS([poll])` | 已确认 |

## 提议的方法

`createEventPoll` 退化成无参工厂（只此一处调用方）：

```cpp
namespace {
std::unique_ptr<EventPoll> createEventPoll()
{
  auto ep = make_unique<LibuvEventPoll>();
  if (!ep->good()) {
    throw DL_ABORT_EX("Initializing LibuvEventPoll failed.");
  }
  return ep;
}
} // namespace
```

`configure.ac` 的 libuv 段落（替换 `:163-173`）：

```sh
PKG_CHECK_MODULES([LIBUV], [libuv >= 1.13])
```

（失败即 configure 报错，不再有 `have_libuv=no` 回退；`HAVE_LIBUV` 宏删完后代码里无人引用，一并删掉，被引用的 `config.h` 宏从 54 个降到 53 个。）

`src/Makefile.am`：`LibuvEventPoll.cc LibuvEventPoll.h` 进基表（按字母序插在 `JsonDiskWriter.h`（`:149`）之后、`Lock.h `（`:151`）之前），三个条件块 `:290-292`、`:526-528`、`:530-532` 全删。

## 分步计划

1. [x] **阶段 1：事件轮询只保留 libuv（后端删除 + 选项删除 + 构建/测试闭环）**
   - 删 `src/EpollEventPoll.{cc,h}`、`src/PollEventPoll.{cc,h}`、`src/SelectEventPoll.{cc,h}`。
   - `src/DownloadEngineFactory.cc`：include 段只留 `#include "LibuvEventPoll.h"`；`createEventPoll()` 无参化；`:124` 改 `createEventPoll()`。
   - `src/Makefile.am`：基表加 Libuv 两行，删 Select 两行 + 三个条件块。
   - `configure.ac`：删 `:39`、`:49`；`:163-173` 换成必需 `PKG_CHECK_MODULES`；删 `:437-444`；删 `:587` 摘要行。
   - 测试：`test/RequestGroupManTest.cc:20,52`、`test/RpcMethodTest.cc:6,92` 换 `LibuvEventPoll`；`test/SessionSerializerTest.cc:15` 删 include。
   - `build_test.sh:57-58` 删两条 `--disable-epoll`；`.github/workflows/build.yml` apt 列表加 `libuv1-dev`。
   - `src/prefs.cc`：删 `:139-142` 四个 `V_*` 与 `:265` `PREF_EVENT_POLL`；`src/prefs.h`：删 `:96-99`、`:218`。
   - `src/usage_text.h:499-500` 删 `TEXT_EVENT_POLL`。
   - `src/OptionHandlerFactory.cc:271-295` 整块删除。
   - `doc/manual-src/en/aria2c.rst:1223-1228` 删 `--event-poll` 条目；`doc/bash_completion/aria2c:34-36` 删 case 分支并从 `:69` 的候选词表中去掉 `--event-poll`；`doc/bash_completion/make_bash_completion.py:96` 删白名单项。
   - 提交：`PLAN-012 阶段1: 事件轮询只保留 libuv 并删除 --event-poll`
2. [ ] **阶段 2：收尾**
   - 计划勾选、写实测门禁表与实际偏差；`MEMORY.md` 记一条（引擎由 epoll 改为 libuv、libuv 变必需依赖、`--event-poll` 已删）。
   - 提交：`PLAN-012: 勾选全部阶段、标记计划完成并记录实测门禁结果`

## 预计修改的文件

删除：
- `src/EpollEventPoll.cc`、`src/EpollEventPoll.h`
- `src/PollEventPoll.cc`、`src/PollEventPoll.h`
- `src/SelectEventPoll.cc`、`src/SelectEventPoll.h`

修改：
- `configure.ac`、`src/Makefile.am`
- `src/DownloadEngineFactory.cc`
- `src/prefs.cc`、`src/prefs.h`、`src/usage_text.h`、`src/OptionHandlerFactory.cc`
- `test/RequestGroupManTest.cc`、`test/RpcMethodTest.cc`、`test/SessionSerializerTest.cc`
- `doc/manual-src/en/aria2c.rst`、`doc/bash_completion/aria2c`、`doc/bash_completion/make_bash_completion.py`
- `build_test.sh`、`.github/workflows/build.yml`
- 新增 `plans/PLAN-012-libuv-only-event-poll.md`

## 测试与验证

阶段 1：
- `autoreconf -i && ./configure --enable-werror && make -j8 check` → 输出 `OK (911)`，0 error、0 C++ warning。
- `git hash-object src/includes/aria2/aria2.h` → `132c72d9f055756769456c35ca7f8e7a879fbaa8`（ABI 未变）。
- `git grep -n "epoll" -- src test` → 0 命中（`HAVE_EPOLL`/文件/注释全清）。
- 冒烟（证明 libuv 循环真在收 IO，不只是编译过）：`python3 -m http.server 18080` 起在 `/tmp/a2test` 下，`./src/aria2c --enable-rpc --rpc-listen-port=16800 -d /tmp/a2dl http://127.0.0.1:18080/f.bin` 下载成功，再用 `aria2c` RPC（`curl` `aria2.tellStatus`）确认 RPC 服务正常。
- libuv 缺失路径：`PKG_CONFIG_LIBDIR=/nonexistent ./configure` → 报 `Package requirements (libuv >= 1.13) were not met` 并以非 0 退出（不是静默回退）。
- `./configure --help | grep -E "libuv|epoll"` → 无输出。
- `./src/aria2c --help=#all | grep -c event-poll` → 0；`./src/aria2c --event-poll=libuv http://127.0.0.1:1/x` → 退出码非 0 且提示 `Unknown option`（解析阶段就失败，不会发起下载）。
- conf 兼容：写一个含 `event-poll=libuv` 的 `aria2.conf` 跑 `./src/aria2c --conf-path=/tmp/a2.conf --dry-run` → 仅 WARN `Unknown option: event-poll=libuv`，不中断。
- `autoreconf -i && ./configure --enable-werror && make -j8 check` → 仍 `OK (911)`。

阶段 2：
- `git grep -n "event-poll" -- . ':!plans' ':!ChangeLog*' ':!NEWS'` → 0 命中；除计划文档外无遗留。

## 验收标准

1. 仓库中不存在 `EpollEventPoll`/`PollEventPoll`/`SelectEventPoll` 文件与引用；`src`/`test` 里除 `LibuvEventPoll` 外没有任何 `EventPoll` 实现。
2. `configure` 在无 libuv 的机器上明确失败；有 libuv 时 `--enable-werror` 下 911 用例全绿、ABI 哈希不变。
3. `--event-poll` 从 `--help=#all`、manpage、bash completion 中消失；命令行传它报未知选项。
4. `git grep "HAVE_EPOLL\|HAVE_LIBUV\|V_EPOLL\|V_SELECT\|PREF_EVENT_POLL"` 除计划文档外 0 命中；`HAVE_POLL` 检测仍在（SocketCore 依赖）。

## 风险与权衡

- **运行时引擎更换**：默认后端从 epoll 变成 libuv（用户没显式指定时同理）。libuv 在所有平台上走同一套 `uv_poll`/`uv_timer` 逻辑，行为可能与 epoll 版有细微差别（事件合并、EDGE/LEVEL 语义、关闭时的 `uv_stop`）；靠 911 用例 + 一次真实下载/RPC 冒烟把关，长跑稳定性不在本次验证范围内。
- **放弃无 libuv 平台**：删除 select/port 后端后，无法在只有 `select(2)` 的环境（Windows 原生、部分嵌入式）构建。aria2 上游支持 Windows/MSYS，本次清理后需 libuv 才能构建，这是明确接受的取舍。
- **命令行兼容性破坏**：脚本里的 `aria2c --event-poll=epoll` 会直接失败退出；conf/会话文件/API 里该键降级为 WARN。属于本次「真清理」的预期结果。
- **CI 与 build_test.sh 不补会红**：`libuv1-dev` 未装时 `configure` 直接失败；这是有意的门禁，不是回归。
- **`HAVE_POLL` 容易误删**：它同时服务 `SocketCore` 的 `poll(2)` 回退，只删 `PollEventPoll` 源文件与相关块，不动 `configure.ac:460`。

## 备注

- 用户口径三连：先「只保留 libuv，清理多余内容」→ 明确「留 libuv 但不动 DNS/其他依赖」→ 两项子决策均取推荐项（完全删 `--event-poll`、libuv 硬依赖并删 epoll 开关）。
- 上一轮已把 PLAN-012（CMake 迁移）产物与 5 个 src 警告修正全部删除/回退，工作区从 `167656ef` 干净起步，本计划重新占用 012 号。
- 迁移期不做等价性对拍（旧后端已在本次删除），libuv 路径的正确性只由既有 911 用例覆盖；若冒烟暴露 libuv 缺陷，追加修复提交，不悄悄回填旧后端。
