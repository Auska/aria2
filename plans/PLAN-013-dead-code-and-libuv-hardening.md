# 任务：清理确认死代码、加固 LibuvEventPoll（唯一 IO 后端）与清理 configure.ac 死探测

## 目标

1. 删除经 grep/编译双重确认的死代码（5 处，全部零引用）。
2. 把 `LibuvEventPoll` 从「历史上只是可选后端之一」加固到「唯一后端」应有的水准：检查 libuv 返回值、不再每 tick 分配 timer、删掉不可达分支、补单测。
3. 清理 `configure.ac` 里产物宏零引用的函数探测（约 40 项）。

## 当前上下文

代码知识图谱（`codebase-memory-mcp cli`，project `home-code-tmp-aria2`，`src`+`test` 索引，12306 节点 / 81433 边，26 个 `parse_partial`）给出的候选，均已用 `git grep` + `g++ -fsyntax-only` 人工核实。

- `3b`/`6565c4f2` 起 `LibuvEventPoll` 是唯一事件后端（`--event-poll` 已删），但 `test/` 里没有任何针对它的单测。
- 门禁基线：`autoreconf -i && ./configure --enable-werror && make -j8 check` → `OK (911)`，ABI 哈希 `132c72d9f055756769456c35ca7f8e7a879fbaa8`（`git hash-object src/includes/aria2/aria2.h`）。

已确认的死代码：

| # | 位置 | 证据 |
|---|---|---|
| 1 | `src/HashFuncEntry.h`（整文件）+ `src/Makefile.am:116` | `git grep HashFuncEntry` 只命中 Makefile 那一行；`HashFuncEntry`/`FindHashFunc`/`getHashFunc` 零使用者 |
| 2 | `test/CookieBoxTest.cc`（65 行） | 不在 `test/Makefile.am`，从未参与编译；`#include "CookieBox.h"` 已不存在，`g++ -std=c++23 -fsyntax-only -Isrc -I.` → `fatal error: CookieBox.h: No such file or directory` |
| 3 | `src/util.h:731` + `src/util.cc:2143` `strless()` | 声明 + 定义，零调用 |
| 4 | `src/OpenedFileCounter.h:63` + `src/OpenedFileCounter.cc:111` `deactivate()` | 零调用（`rgman_` 仍被 `ensureMaxOpenFileLimit`/`reduceNumOfOpenedFile` 使用，成员保留） |
| 5 | `src/prefs.h:164` + `src/prefs.cc:208` `PREF_MAX_DOWNLOADS` | 无 `OptionHandlerFactory` 注册、无读取点，`--help=#all` 中无 `--max-downloads` |

`LibuvEventPoll` 现状（`src/LibuvEventPoll.cc` / `.h`）：

- `poll()`（`.cc:112-133`）每次调用 `new uv_timer_t` → `uv_timer_init` → `uv_timer_start(timer, timer_callback, timeout, timeout)` → `uv_run(UV_RUN_DEFAULT)` → `uv_timer_stop` → `uv_close`。`DownloadEngine::waitData()`（`src/DownloadEngine.cc:199`）每轮引擎迭代调用一次，`noWait_` 时传 `{0,0}`，即每个 tick 一次堆分配 + 延迟释放。
- `if (timeout >= 0)`（`.cc:117`）恒真：`tv` 由 `src/DownloadEngine.cc:190-196` 从 `refreshInterval_` 或 `{0,0}` 构造，永远非负；`:128-131` 的 `else` 不可达。
- 返回值全部忽略：`KSocketEntry` 侧的 `uv_poll_init_socket`（`.h:96`）、`uv_poll_start`（`.h:101`）、`uv_timer_init` / `uv_timer_start`（`.cc:119-120`）。`uv_poll_init_socket` 失败（fd 已关闭 → `UV_EBADF`）时 `handle_` 未初始化，紧接着 `uv_poll_start` 是未定义行为。
- 被删的 `EpollEventPoll` 语义（`git show 167656ef:src/EpollEventPoll.cc:200-204`）：`epoll_ctl` 失败 → `A2_LOG_DEBUG` + `return false`，不抛异常。

`configure.ac` 现状：`AC_CHECK_FUNCS([__argz_count ...])`（第 354 行起，44 项）中 37 项的 `HAVE_*` 宏在 `src`/`test`/`deps` 零引用；`AC_FUNC_STRFTIME`(:347)/`AC_FUNC_VPRINTF`(:348)/`AC_FUNC_FORK`(:349) 产出的 `HAVE_STRFTIME`/`HAVE_VPRINTF`/`HAVE_FORK`/`HAVE_VFORK`/`HAVE_WORKING_FORK`/`HAVE_WORKING_VFORK` 同样零引用。零引用的 37 项：`__argz_count __argz_next __argz_stringify atexit ftruncate getcwd getentropy gethostbyaddr gethostbyname getpagesize memchr memmove mempcpy memset mkdir munmap nl_langinfo pow putenv rmdir setlocale socket stpcpy strcasecmp strchr strcspn strdup strerror strncasecmp strstr strtol strtoul strtoull tzset unsetenv utime utimes`。有引用的必须保留：`getifaddrs mmap posix_fadvise posix_memalign sigaction sleep usleep`，以及 `AC_CHECK_FUNCS([posix_fallocate])`(:424)、`AC_CHECK_FUNCS([daemon])`(:434)、`AC_CHECK_FUNCS([poll])`(:437)、`AC_CHECK_FUNCS([gzbuffer gzsetparams])`(:155)、`AC_CHECK_FUNCS([sqlite3_open_v2])`(:171)、`AC_CHECK_FUNCS([EVP_sha224])`(:193)。

## 待确认决策

| 决策点 | 选项 | 推荐 | 状态 |
|---|---|---|---|
| 阶段3 死探测清理范围 | A 只清函数探测（`AC_CHECK_FUNCS` 长列表 + 3 个 `AC_FUNC_*`）；B 连 `AC_CHECK_HEADERS` 的 `HAVE_*_H` 一起清；C 不动 | A | 已确认：选 A |
| 阶段2 `LibuvEventPoll` 改动幅度 | A 返回值 + 复用 timer + 删死分支 + 单测；B 只加返回值检查 + 单测；C 只补单测 | A | 已确认：选 A |
| libuv 调用失败策略 | A 沿用旧 epoll 语义（记日志 + `addEvents` 返回 false）；B 抛 `DL_ABORT_EX` | A | 已确认：选 A |

## 提议的方法

- 死代码一次删干净，配套的声明/Makefile 行一起改，单阶段内可编译。
- `LibuvEventPoll` 改为成员 `uv_timer_t timer_`（构造时 `uv_timer_init` 一次，析构时 `uv_close`），`poll()` 只做 `uv_timer_start(&timer_, timer_callback, max(0, timeout), 0)` → `uv_run(UV_RUN_DEFAULT)` → `uv_timer_stop`。
- `KPoll` 记录 `uv_poll_init_socket` 的返回码并暴露 `start()` 的返回码；`addEvents` 失败时 `A2_LOG_WARN` + 回滚 `socketEntries_` 插入 + `return false`（不抛异常，保持调用方现有逻辑）。
- 新单测用 `socketpair()` + 继承 `Command` 的最小 `FakeCommand`（`Command` 只有一个纯虚 `execute()`，构造无副作用，`readEventEnabled()` 为 protected，测试里用公有包装暴露）。

## 分步计划

1. [ ] **阶段1：删 5 处死代码**
   - 删 `src/HashFuncEntry.h`，同时删 `src/Makefile.am:116` 的 `HashFuncEntry.h \` 行。
   - 删 `test/CookieBoxTest.cc`（`criteriaFind` 的断言已由 `test/CookieStorageTest.cc` 覆盖：12 处调用，含 host/path/expires/numeric-host 用例）。
   - 删 `src/util.h:731` 声明与 `src/util.cc:2143` 定义（`strless`）。
   - 删 `src/OpenedFileCounter.h:63` 声明（含其上注释）与 `src/OpenedFileCounter.cc:111` 定义（`deactivate`）；保留 `rgman_` 成员。
   - 删 `src/prefs.h:164` 与 `src/prefs.cc:208`（`PREF_MAX_DOWNLOADS`）。
2. [ ] **阶段2：`LibuvEventPoll` 加固 + 单测**
   - `src/LibuvEventPoll.h`：`KPoll` 增加 `int err_`，构造时存 `uv_poll_init_socket` 返回码（成功才设 `handle_.data`），`start()` 返回 `int`（0 成功）；`LibuvEventPoll` 增加成员 `uv_timer_t timer_;` 与 `bool timerInited_;`，`good()` 追加 `timerInited_` 条件。
   - `src/LibuvEventPoll.cc`：构造函数初始化 `timer_`/`timerInited_`；`poll()` 复用 `timer_`（`timeout` 取 `std::max(0, ...)`，repeat 传 0），检查 `uv_timer_start` 返回值，日志用 `A2_LOG_WARN` + `uv_err_name(r)`；删不可达 `else` 分支；析构里先 `uv_close(&timer_)` 再 `uv_run(...)` + `uv_loop_delete`；`addEvents` 两个分支都检查 `start()` 返回码，失败时 WARN + 回滚 + `return false`。
   - 新增 `test/LibuvEventPollTest.cc`（4 个用例：READ 触发、WRITE 触发、`deleteEvents` 后不再触发、无注册时空 `poll` 按时返回），并加进 `test/Makefile.am` 的 `aria2c_SOURCES` 基表。
3. [ ] **阶段3：清 `configure.ac` 死探测**
   - 删 `AC_FUNC_STRFTIME`(:347)/`AC_FUNC_VPRINTF`(:348)/`AC_FUNC_FORK`(:349)。
   - 从 `AC_CHECK_FUNCS` 长列表删上面 37 项，保留 7 项有引用的。
   - 跑 `autoreconf -i` 重生成 `configure`/`config.h.in`，确认 `config.h` 里不再出现被删的 `HAVE_*`。

## 预计修改的文件

```
src/HashFuncEntry.h          （删除整文件）
src/Makefile.am              （删 1 行）
test/CookieBoxTest.cc        （删除整文件）
src/util.h                   （删 1 行声明）
src/util.cc                  （删 1 行定义）
src/OpenedFileCounter.h      （删声明 + 注释）
src/OpenedFileCounter.cc     （删定义）
src/prefs.h                  （删 1 行 extern）
src/prefs.cc                 （删 1 行定义）
src/LibuvEventPoll.h         （KPoll err_/start()、timer 成员、good()）
src/LibuvEventPoll.cc        （构造/析构/poll/addEvents）
test/LibuvEventPollTest.cc   （新建）
test/Makefile.am             （加 1 行源文件）
configure.ac                 （删 40 项探测）
configure / config.h.in      （autoreconf 生成，不入库）
```

## 测试与验证

每个阶段结束都跑门禁，阶段2 额外跑真机冒烟：

```sh
autoreconf -i && ./configure --enable-werror && make -j8 check
git hash-object src/includes/aria2/aria2.h      # 期望 132c72d9f055756769456c35ca7f8e7a879fbaa8
```

通过标准：

- `test/aria2c.log` 出现 `OK (911)`（阶段2 后应为 `OK (915)`：新增 4 个用例），且没有新增 warning（既存 libtool `-version-info` 提示除外）。
- 阶段2：`test_outdir` 里 4 个新用例名可见；冒烟 `python3 -m http.server 18080` + `./src/aria2c -d /tmp/a2dl -o got.bin http://127.0.0.1:18080/f.bin` 后 `cmp` 一致；`curl -d '{"jsonrpc":"2.0","id":"1","method":"aria2.getVersion","params":[]}'` 的 RPC 冒烟（后台起 + `pkill -x aria2c`）。
- 阶段3：`grep -cE "HAVE_(STRFTIME|VPRINTF|FORK|VFORK|WORKING_FORK|WORKING_VFORK|MEMCHR|STRDUP|TZSET|UNSETENV|UTIME|UTIMES|POW|SOCKET|MKDIR|GETCWD|STRERROR|SETLOCALE|PUTENV|RMDIR|ATEXIT|MEMSET|MEMMOVE|STRCHR|STRSTR|STRTOL|STRTOUL|STRTOULL|STPCPY|STRCASECMP|STRNCASECMP|STRCSPN|NL_LANGINFO|GETPAGESIZE|MUNMAP|FTRUNCATE|GETENTROPY|GETHOSTBYADDR|GETHOSTBYNAME|__ARGZ_COUNT|__ARGZ_NEXT|__ARGZ_STRINGIFY)" config.h` = 0；`make check` 仍 `OK (915)`。
- 阶段1：`git grep -n "HashFuncEntry\|strless\|deactivate\|PREF_MAX_DOWNLOADS\|CookieBoxTest"` 只剩 `plans/` 与无关命中（如 `DeferredTest` 里的同名局部变量），`make check` 仍 `OK (911)`。

## 验收标准

- 三处死代码彻底消失（源码 + Makefile 行），且没有任何 `#include`/调用点残留。
- `LibuvEventPoll` 不再有未检查的 libuv 调用返回值，`poll()` 不再每次分配 timer，没有不可达分支。
- 新增的 4 个用例能覆盖 READ/WRITE/删除后静默/超时返回，且在全量 911+4 用例里稳定通过。
- `configure.ac` 的产物宏与源码引用一一对应：`config.h` 里不存在零引用的函数探测宏。
- ABI 哈希不变，`--help=#all` 无变化。

## 风险与权衡

- 阶段3 只清理函数探测：`AC_CHECK_HEADERS` 的 `HAVE_*_H` 与条件型 `AC_FUNC_*`（`AC_FUNC_MEMCMP`/`AC_FUNC_STAT`/`AC_FUNC_STRTOD`/`AC_FUNC_MKTIME`/`AC_FUNC_ERROR_AT_LINE`，其宏在本机未定义、无法证明无用）保留，遗留一部分死配置。
- 清掉古老函数探测（`strdup`/`memchr`/`strcasecmp` 等）等于放弃对「某些平台缺这些函数」的探测；本项目只在 Linux 上验证，跨平台可移植性不再由 configure 兜底（这些函数在所有现代目标平台都有）。
- `LibuvEventPoll` 从每 tick 新建 timer 改成复用成员 timer，改变了 handle 生命周期：若析构顺序处理不当（timer 未 `uv_close` 就 `uv_loop_delete`）会泄漏或在断言版本 libuv 上 abort，故析构路径改动必须由单测 + 冒烟一起验证。
- `good()` 增加 `timerInited_` 条件后，`DownloadEngineFactory::createEventPoll()` 的失败路径会多覆盖一种情形（libuv 极少数初始化失败），行为仍是抛 `DL_ABORT_EX`。

## 备注

- 用户授权：m00415「创建修复计划，全部都做」= 计划 + 执行三个阶段。
- 三个决策均由用户在提问中选择推荐项（见上表）。
- 审查 #1：审查报告里「`crypto_hash.cc:49 par()` 零调用」是误判——`par` 通过宏 `r2`/`r4`（`src/crypto_hash.cc:402,404`）作为 `r(f, k, ...)` 的 `f` 参数使用，属裸标识符用法，`grep "par("` 抓不到；已从阶段1 范围移除。同理「`CookieStorage::criteriaFind` 无覆盖」也是误判（先前 `git grep ... | head -5` 被 `CookieBoxTest.cc` 的 5 行截断），实际 `test/CookieStorageTest.cc` 有 12 处调用，故阶段1 直接删 `CookieBoxTest.cc`，不需要移植断言。
