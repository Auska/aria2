# 任务：将 aria2 从 C++11 升级到 C++20

## 目标

把构建标准从 C++11 切到严格 `-std=c++20`，清理 C++11 兼容层，引入一组低风险的
C++20 容器/字符串惯用法，同步 CI 工具链与文档；公共 libaria2 API 与 ABI 不变。

## 当前上下文

- 仓库 `/home/code/tmp/aria2`，git 工作区干净（`git status --porcelain` 为空），
  最近提交是一串功能裁剪（去 Metalink / GnuTLS / tcmalloc / 非 Linux 平台等）。
- 构建是 Autotools：`configure.ac` + `src/Makefile.am` + `test/Makefile.am` +
  `m4/ax_cxx_compile_stdcxx.m4`（autoconf-archive 版本，已支持参数 `20`）。
  `configure` / `Makefile.in` / `config.h.in` / `aclocal.m4` 均为 **gitignore 的生成
  文件，不入 diff**；`makerelease` 与 CI 都靠 `autoreconf -i` 重建。
- `configure.ac:111` 现为 `AX_CXX_COMPILE_STDCXX([11], [], [mandatory])`。
  该宏把 `-std=...` 追加到 **`CXX` 变量本身**，不是 `CXXFLAGS`；而
  `configure.ac:108-115` 的 `CXX1XCXXFLAGS` 只在 `CXXFLAGS` 上取快照，因此实测
  `config.log` 显示 `CXX1XCXXFLAGS` 为空（本机 g++ 14 默认已是 C++17，
  `__cplusplus=201703L`，宏的「默认支持」分支直接通过，根本没加标准开关）。
  **验收必须检查 `src/Makefile` 的 `CXX = ... -std=c++20`，不能检查
  `CXXSTDCXXFLAGS`。**
- `configure.ac:117-149` 有 nullptr 探测、`override` 探测和
  `AC_DEFINE_UNQUOTED([CXX11_OVERRIDE], ...)`。已跟踪源码共 **1265 处
  `CXX11_OVERRIDE` / 277 个文件**（`src` 240 + `test` 37，
  `git grep -l ... -- 'src/*.h' 'src/*.cc' 'test/*.h' 'test/*.cc'`）；
  另 `configure.ac` 本身 1 处，合计 1266。生成的 `config.h` /
  `config.h.in` 里还有 `#define CXX11_OVERRIDE override` / `#undef CXX11_OVERRIDE`
  与 `#undef HAVE_CXX11`，二者都是 gitignore 文件，由 `autoreconf -i` 重建。
- `throw()` 共 9 处 / 7 个文件（其中 `src/Dependency.h:46` 是注释）。
  实测 gcc 14 与 clang 22 在 `-std=c++20 -Wall -Wextra -Werror -pedantic-errors`
  下仍接受 `throw()`（等价 `noexcept(true)`），但属旧写法，按清理范围一并换掉。
- `src/a2functional.h:152-170` 有 `#if __cplusplus > 201103L` 的
  `make_unique` 兼容实现；613 处调用 / 189 文件依赖 `aria2::make_unique` 这个名字。
  C++20 下 fallback 恒为死代码，但 `using std::make_unique;` 必须保留（否则 613 处
  非限定调用全部失效）。其中 **18 处是数组形式** `make_unique<T[]>(n)`；
  `std::make_unique<T[]>(n)` 与自有 fallback 的 `new T[n]()` 同为值初始化，
  语义一致（已用 gcc 14 编译探针确认），无需改写调用方。
- 已确认代码中**没有** C++20 移除的库设施（`std::auto_ptr`、
  `std::unary_function` / `binary_function` / `mem_fun` / `bind1st` / `bind2nd`、
  `std::random_shuffle`、`std::iterator<>`、`std::result_of`、`std::is_pod`、
  `std::uncaught_exception`、`.unique()`），也没有 `u8""`、`volatile` 复合赋值、
  `[=]` 隐式捕获 `this`、逗号下标等 C++20 破坏性写法。
  `GroupId::import` / `import(...)` 是**成员函数名**，不是 C++20 module 关键字，
  安全。`throw()` → `noexcept` 后 `Exception::what()` 与
  `std::exception::what() noexcept` 兼容。
- 项目自有格式化层 `src/fmt.h` 的 `fmt()`（printf 风格，820 处调用）**保留**；
  `__attribute__((format(printf,...)))`、`__builtin_bswap32/64`、
  `__builtin_expect` 在 `-std=c++20 -pedantic-errors -Werror` 下实测可用
  （gcc 14 / clang 22 均通过），因此严格 `-std=c++20` 不会破坏现有 GNU 扩展。
- 关联容器纯存在性判断候选（`count(k)` 比 0/1、`count` 当 bool）：
  `src/SessionSerializer.cc:203,242`、`src/HttpHeader.cc:56`（`multimap`）、
  `src/ValueBase.cc:192`（`map`）、`src/GroupId.cc:50,61`（`set`）、
  `src/PeerSessionResource.cc:159,169`（`set`）、`src/DefaultPeerStorage.cc:80`、
  `test/DefaultPeerStorageTest.cc:214,219`。`std::chrono::duration::count()` 与
  `getInterval().count()` 等**不是**容器，禁止误改。
- `std::find(...) != end()` 的存在性判断
  （`src/DNSCache.cc:114`、`src/Piece.cc:285,290`、
  `src/bittorrent_helper.cc:1035,1039`、`src/RpcMethodImpl.cc:547`、
  `src/util.cc:2106`）作用在 `vector` / `deque` 上，**C++20 无 `contains`**
  （`std::ranges::contains` 是 C++23），保留原写法。
- erase-remove 共 **18 处 / 11 文件**（`git grep -n std::remove -- 'src/*.cc'`
  精确计数），容器均为 `std::vector` / `std::deque`（含
  `deque<unique_ptr<...>>`），头文件都已间接包含 `<vector>` / `<deque>`，
  C++20 `std::erase` / `std::erase_if` 在 gcc 14 / clang 22 实测可用。
  **标准把 `erase_if(c, pred)` 定义为等价于 `remove_if(begin, end, pred)`
  + `erase(it, end)`**（cppreference vector/deque `erase2`），谓词对每个元素
  恰好调用一次且不修改元素本身；因此 6 处带副作用的谓词
  （`FailConnectDelete` ×2、`CollectAddrPortMatch`、`TimeoutCheck`、
  `UTMetadataRequestTracker::removeTimeoutEntry`、
  `DHTMessageTracker::handleTimeout`）**同样可以安全改写**，它们改的是
  `shared_ptr` 指向的对象或外部 `dest`/`indexes` 容器，并未修改容器元素。
  这些谓词都是可拷贝的（引用成员在拷贝后仍指向同一外部对象），
  而 `std::erase_if` 按值接收 `Pred` 不会有额外影响。
- 自有字符串工具：`src/util.h` 有 `startsWith`/`endsWith`/`istartsWith`/
  `iendsWith` 各 2 个迭代器模板重载 + 2 个 `std::string` 重载；
  定义在 `src/util.cc:2140-2177`。大小写敏感版共 56 处调用（`src`+`test`），
  其中 **24 处在 `src/` 里是 `std::string` 重载**（逐行核对所得）；
  其余是迭代器重载（含 `src/json.cc:123,127,131,135`、
  `src/util.cc:652,660,1933`、`src/OptionParser.cc:208`、
  `src/option_processing.cc:145`、`test/Sqlite3CookieParserTest.cc:90`、
  `test/UtilTest1.cc` 的若干断言）。大小写不敏感版有 8 处 `std::string`
  重载调用（`src/ContentTypeRequestGroupCriteria.cc:59`、
  `src/HttpRequest.cc:260,414,415`、`src/HttpConnection.cc:105,108,111,114`），
  **C++20 无 `istarts_with` 等价物**，必须保留。
- 文档里过时的 C++11 描述：`README.rst:130-134`、
  `doc/manual-src/en/libaria2.rst:31-32`（含 "GCC 4.7 works well here"）、
  `examples/libaria2ex.cc:37`、`examples/libaria2wx.cc:41`。
- CI：`.github/workflows/build.yml` 用 `ubuntu-22.04` + `g++-12` / `clang-15`，
  ASan/UBSan + `make check`。已核实 Ubuntu 24.04 (noble) 仓库与
  `ubuntu-24.04` runner 均提供 `g++-14` / `clang-18`
  （packages.ubuntu.com、actions/runner-images、ubuntu.com LLVM/GCC 版本表）。
- **测试口径**：`make check` 只跑一个测试程序 `test/aria2c`（CPPUnit
  单体二进制，内部约 978 个断言），所以 `test/test-suite.log` 固定是
  `# TOTAL: 1` / `# PASS: 1`；真正判断用例是否全绿要看 `test/aria2c.log`
  末尾的 `OK (N)` 且无 `Failures !!!`。当前工作区里 `test/aria2c.log`
  是 16:40 的 `OK (978)`，但**直接运行现存 `test/aria2c` 二进制实测
  `Run: 978 Failure total: 15`**（全是已删除的 Metalink 测试残留），
  说明根目录的 `.o` / 二进制已与当前 `Makefile.am` 不一致。
  这进一步坐实了必须用全新构建目录 / 干净工作树做验证。
- **out-of-tree 风险**：`configure.ac:558` 在启用 websocket 时用
  `AC_CONFIG_SUBDIRS([deps/wslay])`，子包有自己的 `configure.ac` 与
  `configure`。全新 VPATH 构建需验证子包能正确接收 `--srcdir`；若失败，
  退回「`git worktree add` 干净副本后原地 `autoreconf -i && ./configure`」，
  绝不在当前含残留产物的目录里增量构建。
- 本机：`g++ 14.2.0`、`clang++-22`、`autoconf 2.72`、`automake 1.17`、
  `cppunit 1.15.1` 可用。工作区根目录已有大量 `.lo/.o`、`config.h`、
  `src/aria2c` 等 gitignore 构建产物，**增量 `make` 会假绿**。

## 待确认决策

| 决策点 | 选项（含用户备注的约束） | 推荐 | 状态 |
|---|---|---|---|
| 升级范围 | 最小切换 / 切换并清理兼容层 / **切换并引入 C++20 惯用法** | 最小切换 | 已确认：用户第 1 轮选「切换并引入 C++20 惯用法」，即含清理兼容层 |
| `-std` 方言 | **严格 `-std=c++20`** / `-std=gnu++20` | 严格 | 已确认：用户第 1 轮；用 `AX_CXX_COMPILE_STDCXX([20], [noext], [mandatory])` |
| 最低编译器 / CI | 保持现状 / 明确最低版本+门禁 / **升级 CI 编译器** | 保持现状 | 已确认：第 1 轮选升级；第 2 轮细化为 **ubuntu-24.04 + g++-14 / clang-18**（不引第三方源） |
| 惯用法具体层级 | **容器/字符串小工具** / +std::span / +std::format / +concepts,ranges | 容器/字符串 | 已确认：用户第 2 轮；明确**不做** span、format、concepts、ranges、fmt 层重写 |
| 验收深度 | 单次 / **单次 + --enable-werror** / GCC+Clang 双编译 | 单次+werror | 已确认：用户第 1 轮选「仅一次 make check」，第 2 轮细化为单次且带 `--enable-werror`；不跑双编译器、不跑 `build_test.sh` 全矩阵 |
| 公共 API | **不动公共 API** / 允许改 | 不动 | 已确认：用户第 2 轮；`src/includes/aria2/aria2.h` 保持签名与 ABI，示例只改编译命令注释 |
| 旧字符串工具 | **删 string 重载** / 保留重载内部转发 / 完全不动 | 删 | 已确认：用户第 3 轮；口径收敛见「备注 审查 #1」 |
| 变量命名 | **重命名为 `CXXSTDCXXFLAGS`** / 保留 `CXX1XCXXFLAGS` | 重命名 | 已确认：用户第 3 轮；同步 3 个引用点 |

无待确认项。

## 提议的方法

1. **构建系统**：`configure.ac` 把 `AX_CXX_COMPILE_STDCXX` 改为
   `([20], [noext], [mandatory])`。`noext` 会跳过 `-std=gnu++20` 分支、强制走
   `-std=c++20` 探测，即使编译器默认已是 C++20 也会显式带上开关 —— 满足
   「严格」。`CXX1XCXXFLAGS` → `CXXSTDCXXFLAGS`，共 5 个引用点：
   `configure.ac:113`（赋值）、`115`（`AC_SUBST`）、`119`（nullptr 探测块里的
   `CXXFLAGS="$CXXFLAGS $CXX1XCXXFLAGS"`）、`629`（configure 摘要）
   及 `src/Makefile.am:613`、`test/Makefile.am:262` 的 `AM_CXXFLAGS`
   （合计 6 处文本；119 容易漏，漏了虽因 shell 未定义变量展开为空而不报错，
   但语义不一致）。
2. **清理兼容层**：删 nullptr/`override` 探测与 `CXX11_OVERRIDE` 定义；
   用机械替换把 1265 处源码里的 `CXX11_OVERRIDE` 换成 `override`
   （**与删宏同一提交**，否则中间态不可编译；`configure.ac` 里那 1 处是宏定义本身，
   随探测代码一起删除）；9 处 `throw()` → `noexcept`；
   `src/a2functional.h` 的 `#if` 块收敛为单行 `using std::make_unique;`。
3. **C++20 惯用法（仅限已确认层级）**：
   - 关联容器纯存在性判断 11 处 → `.contains(k)`；`count()` 当 bool 的返回值同样
     换 `contains()`（`multimap::count` 可能 >1，但原代码只当 bool 用，等价）。
   - 18 处 erase-remove 全部 → `std::erase(c, v)`（2 处）/ `std::erase_if(c, p)`
     （16 处）；带副作用谓词的 6 处也改，理由见「当前上下文」的标准等价性说明。
     改后不再需要的 `<algorithm>` 包含不主动删除（这些文件多数仍用其他算法，
     保留无害，避免无谓 diff）。
   - 24 处大小写敏感的 `util::startsWith(s, x)` / `util::endsWith(s, x)`
     → `s.starts_with(x)` / `s.ends_with(x)`；删掉 `src/util.h` 里
     `startsWith`/`endsWith` 的 4 个 `std::string` 重载声明
     （`util.h:648-649,686-687`）与 `src/util.cc:2140-2148,2160-2168` 的
     4 个定义，保留全部迭代器模板重载与大小写不敏感的 4 个 `std::string` 重载。
   - **不做**：`std::span`、`std::format`、`concepts`、`ranges`、自有 `fmt()`
     层重写、公共 API 改动、`std::find` 存在性判断改写。
4. **CI / 文档**：`.github/workflows/build.yml` 换 `ubuntu-24.04`、
   `g++-14`、`clang-18`，同步 `CC`/`CXX` 环境变量（保留 ASan/UBSan 与
   4 维矩阵结构）；`README.rst` 与 `doc/manual-src/en/libaria2.rst` 改为
   「需支持 `-std=c++20` 的编译器」，删掉 GCC 4.7 / `-std=c++0x` 的过时描述；
   两个示例的编译命令注释改 `-std=c++20`。
5. **不变**：`src/includes/aria2/aria2.h`、`src/fmt.h`/`fmt.cc`、
   `m4/ax_cxx_compile_stdcxx.m4`、`build_test.sh`、`deps/wslay`（纯 C）、
   所有生成文件（`configure`、`Makefile.in`、`config.h.in` 等，均由
   `autoreconf -i` / `configure` 重建且不入 diff）。

## 分步计划

（一个阶段一次提交；阶段数 3，用户授权后用 `todo` 同步 3 个任务）

1. [x] **阶段 1：构建标准 + CI + 文档切到 C++20**
   - `configure.ac`：`107` 注释改 C++20、`111` 改
     `AX_CXX_COMPILE_STDCXX([20], [noext], [mandatory])`、
     `CXX1XCXXFLAGS` → `CXXSTDCXXFLAGS` 的全部 6 处文本（含 `119` 的
     nullptr 探测块与 `629` 的 configure 摘要）。
     **本阶段保留** nullptr/`override` 探测与 `CXX11_OVERRIDE` 定义
     （C++20 下探测照样通过），保证代码无需改动即可编译。
   - `src/Makefile.am`、`test/Makefile.am` 同步变量重命名。
   - `.github/workflows/build.yml`、`README.rst`、
     `doc/manual-src/en/libaria2.rst`、`examples/libaria2ex.cc`、
     `examples/libaria2wx.cc`。
   - 门禁：全新目录 `autoreconf -i` → `configure` → `make -j check`；核对
     `src/Makefile` 里 `CXX = g++ -std=c++20`。
2. [ ] **阶段 2：清理 C++11 兼容层**
   - `configure.ac` 删 `117-149` 的 nullptr/override 探测与
     `CXX11_OVERRIDE` 定义；`CXXSTDCXXFLAGS` 的 save/restore 逻辑按现状保留。
   - 用 `git grep -l CXX11_OVERRIDE -- 'src/*.h' 'src/*.cc' 'test/*.h' 'test/*.cc'`
     生成精确清单，机械替换 277 个源码文件的 1265 处为 `override`
     （**同一提交内闭环**，不允许跨阶段）。加路径限定是必要的：本计划文档
     自身含 24 处该字符串，提交后全仓 `git grep` 会被污染。
   - 9 处 `throw()` → `noexcept`（含 `Dependency.h:46` 注释措辞）。
   - `src/a2functional.h` 删 `#if __cplusplus > 201103L` 的 fallback，
     仅留 `using std::make_unique;`（数组形式调用行为一致，见「当前上下文」）。
   - 门禁：带路径限定的 `git grep CXX11_OVERRIDE -- 'src/*.h' 'src/*.cc'
     'test/*.h' 'test/*.cc' 'configure.ac'` 与 `git grep -F 'throw()' --
     'src/*.h' 'src/*.cc' 'test/*.h' 'test/*.cc'` 均无命中；
     `config.h` 里也不再由 configure 定义 `CXX11_OVERRIDE`；
     全新构建 `make -j check` 通过。
3. [ ] **阶段 3：C++20 容器/字符串惯用法（即最终验收）**
   - `.contains()` 11 处、`std::erase`/`std::erase_if` 18 处、
     `starts_with`/`ends_with` 24 处；删 4 个 `std::string` 重载声明与
     4 个定义。
   - 核对已有测试：`test/UtilTest2.cc` 的 `testNoProxyDomainMatch` /
     `testDetectDirTraversal` / `testInPrivateAddress`（均已注册）、
     `test/MagnetTest.cc`、`test/UtilTest1.cc` 的 `testStartsWith` /
     `testEndsWith`；对 `starts_with`/`ends_with` 补 2-3 个边界断言
     （空前缀、前缀长于目标、空目标），防止重载删除后行为漂移。
   - 门禁 / 最终验收：`./configure --enable-werror` + `make -j check`，
     核对 `test/test-suite.log` 为 `# TOTAL: 1` / `# PASS: 1`，
     **并核对 `test/aria2c.log` 末尾为 `OK (N)`、无 `Failures !!!`**；
     再确认 `config.h` 已无 `CXX11_OVERRIDE` / `HAVE_CXX11`。

## 预计修改的文件

- `configure.ac`、`src/Makefile.am`、`test/Makefile.am`
- `.github/workflows/build.yml`
- `README.rst`、`doc/manual-src/en/libaria2.rst`、
  `examples/libaria2ex.cc`、`examples/libaria2wx.cc`
- `src/a2functional.h`、`src/util.h`、`src/util.cc`
- `src/Exception.h`、`src/Exception.cc`、
  `src/UnknownOptionException.h`、`src/UnknownOptionException.cc`、
  `src/OptionHandlerException.h`、`src/OptionHandlerException.cc`、
  `src/Dependency.h`
- 关联容器 / erase-remove / 字符串调用点涉及的具体文件（均已核实存在）：
  `src/SessionSerializer.cc`、`src/HttpHeader.cc`、`src/ValueBase.cc`、
  `src/GroupId.cc`、`src/PeerSessionResource.cc`、`src/DefaultPeerStorage.cc`、
  `src/DefaultBtRequestFactory.cc`、`src/FileEntry.cc`、`src/DHTTaskExecutor.cc`、
  `src/DHTBucket.cc`、`src/Piece.cc`、`src/CookieStorage.cc`、
  `src/UDPTrackerClient.cc`、`src/UTMetadataRequestTracker.cc`、
  `src/DHTMessageTracker.cc`、`src/DefaultBtMessageDispatcher.cc`、
  `src/DHTPeerAnnounceEntry.cc`、`src/bittorrent_helper.cc`、
  `src/HttpDownloadCommand.cc`、`src/json.cc`、`src/option_processing.cc`、
  `src/AuthConfigFactory.cc`、`src/magnet.cc`、`src/OptionHandlerImpl.cc`、
  `src/RpcMethod.cc`、`src/HttpSkipResponseCommand.cc`、
  `src/DHTMessageTrackerEntry.cc`、`src/DownloadCommand.cc`、
  `src/cookie_helper.cc`、`src/Request.cc`
- `test/DefaultPeerStorageTest.cc`、`test/UtilTest1.cc`、`test/UtilTest2.cc`
- **批量机械替换**：`src/**`、`test/**` 下所有含 `CXX11_OVERRIDE` 的 277 个文件
  （执行时用 `git grep -l CXX11_OVERRIDE` 生成精确清单，不手写）
- 新增：`plans/PLAN-001-cpp20-upgrade.md`（本文件）

**不变 / 不入 diff**：`src/includes/aria2/aria2.h`、`src/fmt.h`、`src/fmt.cc`、
`m4/ax_cxx_compile_stdcxx.m4`、`build_test.sh`、`deps/**`、
`configure`、`Makefile.in`、`config.h.in`、`aclocal.m4`（gitignore 生成文件）。

## 测试与验证

- 每阶段门禁（工程必需，否则无法保证提交可编译）：
  ```
  # 干净副本优先，避免现存 187 个残留 .o / Metalink 旧测试污染
  git worktree add --detach /tmp/aria2-cxx20-build HEAD
  cd /tmp/aria2-cxx20-build && autoreconf -i
  ./configure [阶段3: --enable-werror]
  make -j"$(nproc)" check
  ```
  或等价的全新 VPATH 目录（`mkdir /tmp/... && /home/code/tmp/aria2/configure`）；
  若 `deps/wslay` 子包在 VPATH 下失败，退回上面的 `git worktree` 干净副本。
  **绝不在当前工作区原地增量 `make`**：现存 `test/aria2c` 与 `Makefile.am`
  已不同步（实测 15 个 Metalink 残留失败），旧 `.lo/.o` 不重编会假绿；
  且阶段 2 删宏后必须重新 `autoreconf -i && configure` 才能证明 `config.h`
  真的不再定义 `CXX11_OVERRIDE`。每阶段验证完用 `git worktree remove`
  清掉临时副本（临时脚本不入库）。
- 通过标准：
  1. `make check` 退出码 0；`test/test-suite.log` 为 `# TOTAL: 1` /
     `# PASS: 1`，**且 `test/aria2c.log` 末尾是 `OK (N)`、无 `Failures !!!`**
     （Autotools 只看到 1 个测试程序，光看 test-suite.log 会假绿）。
  2. 构建目录的 `src/Makefile` 中 `CXX = g++ -std=c++20`
     （不是 `-std=gnu++20`），且 `g++ -dM -E -x c++ /dev/null` 的默认
     `__cplusplus` 为 `201703L`，借此确认标准开关确实由本次改动带入、
     而非编译器默认行为。
  3. 带路径限定的 `git grep CXX11_OVERRIDE -- 'src/*.h' 'src/*.cc'
     'test/*.h' 'test/*.cc' 'configure.ac'` 与
     `git grep -F 'throw()' -- 'src/*.h' 'src/*.cc' 'test/*.h' 'test/*.cc'`
     无命中（计划文档自身含这些字符串，必须加路径限定）；生成的 `config.h`
     无 `CXX11_OVERRIDE` / `HAVE_CXX11`，有 `HAVE_CXX20`。
  4. 阶段 3 用 `--enable-werror`（`-Wall -Werror -Wformat-security`），
     C++20 新增的废弃 / 窄化警告会直接失败，不允许带警告前进。
  5. `git status --porcelain` 中除预期的已跟踪文件改动和本计划文档外，
     无生成文件被误加入（`.gitignore` 已覆盖 `configure` / `Makefile.in` /
     `config.h.in` / `*.lo` / `*.o`）。
- **不执行**（按用户验收决定）：clang 二次构建、`build_test.sh` 的 9 种
  依赖矩阵、ASan/UBSan 复跑、`make distcheck`、公共 API/ABI 检查工具。

## 验收标准

1. 从干净检出 `autoreconf -i && ./configure && make check` 全程通过，
   编译器命令行显式带 `-std=c++20`。
2. 源码中不再有 `CXX11_OVERRIDE`、`throw()` 动态异常规范、
   `make_unique` 的 C++11 fallback，也不再有 `HAVE_CXX11` 引用。
3. 公共头 `src/includes/aria2/aria2.h` 无任何改动，libaria2 ABI 不变。
4. README、libaria2 手册、两个示例的编译说明均指向 `-std=c++20`，
   不再出现 C++11 / `-std=c++0x` / GCC 4.7。
5. CI 定义切到 `ubuntu-24.04` + `g++-14` / `clang-18`，矩阵与
   ASan/UBSan 配置保持有效。
6. 三个提交分别对应三个计划阶段，`PLAN-001` 的勾选项与实际一致。

## 风险与权衡

- **用户选了「最大改造范围 + 最小验收」**：277 文件的机械替换 + 53 处
  语义改写，只跑一次（每阶段一次门禁、无 clang/全矩阵）验证。未覆盖
  clang-18 的诊断差异与 9 种条件编译组合；CI 本身会在 push 后补上
  clang + 4 维矩阵，属可接受的分工，但本地合并前不会发现 clang 专属问题。
- **`CXXSTDCXXFLAGS` 实际是空变量**（宏把开关写进 `CXX`）。重命名只是消除
  命名误导，不产生功能变化；若按变量名去验收会假绿，见「当前上下文」。
- **`std::erase_if` 与带副作用谓词**：`FailConnectDelete` 等 6 处谓词会修改
  `shared_ptr` 指向的对象或收集结果到外部容器。标准定义 `erase_if` 等价于
  `remove_if` + `erase`（每元素恰好一次谓词调用），因此行为不变；已用
  gcc 14 / clang 22 探针确认调用次数，并在阶段 3 靠已有的 UDP tracker /
  DHT tracker / UT metadata 单测回归。若任一实现的 `erase_if` 实现细节
  偏离该等价定义，这是唯一会静默改变行为的地方。
- **删 `std::string` 重载**：`util::startsWith(a, b)` 的迭代器版实现是
  `std::equal` 前缀比较，与 `a.starts_with(b)` 语义一致；但大小写不敏感版
  无标准等价物，必须保留 4 个重载 + 8 处调用。删重载后若有未列出的调用点
  会直接编译失败（编译器兜底，不会静默行为变化）。
- **`throw()` → `noexcept`**：`Exception::~Exception()` 是虚析构，改为
  `noexcept` 与 `std::exception` 基类一致；`what()` 的 override 签名必须
  同时带 `noexcept`，否则 override 失败（编译期兜底）。
- **CI 换 OS**：`ubuntu-24.04` 的 cppunit / c-ares / openssl 包名与 22.04
  相同，但 `clang-15` → `clang-18`、`g++-12` → `g++-14` 需同步改 4 处
  `CC`/`CXX` 环境变量，漏改会让矩阵仍用旧编译器却「看起来升级了」。
- **`-std=c++20` 严格模式**：已实测现有 `__attribute__` / `__builtin_*`
  在 `-pedantic-errors` 下可用；但已跟踪的 `src/*.cc`+`src/*.h` 851 个、
  `test/*.cc`+`test/*.h` 205 个文件中可能存在未 grep 到的方言依赖，
  只能靠阶段 1 的全量编译暴露。
- **现存工作区不是干净基线**：`test/*.o` 有 187 个残留（含已删除的
  Metalink 测试），直接跑 `test/aria2c` 实测 `Failure total: 15`，而
  `test/aria2c.log` 却停留在旧的 `OK (978)`。这是仓库现状，不是本次改动
  引入的；验证必须用干净副本 / 全新构建目录，不能拿现存日志当通过证据。
- **未做**：`std::span`、`std::format`、`concepts`、`ranges`、自有 `fmt()`
  层重写、公共 API 改造 —— 用户明确把这些排除在本次范围外。

## 备注

- 用户第 1 轮选择：「切换并引入 C++20 惯用法」「严格 `-std=c++20`」
  「升级 CI 编译器」「仅一次 make check」。
- 用户第 2 轮选择：惯用法限定为「容器/字符串小工具」、CI 为
  「24.04 + g++14/clang18」、验收为「单次 + `--enable-werror`」、
  「不动公共 API」。选项自带的约束「公共头 aria2.h 默认不动」已落实为
  验收标准第 3 条。
- 用户第 3 轮选择：「删 string 重载」「重命名为 `CXXSTDCXXFLAGS`」。
- 审查 #1：第 3 轮问题里写了「删 8 个 string 重载」又写了「大小写不敏感
  版本保留」，两句口径冲突。按 C++20 是否有等价物收敛为：只删
  `startsWith`/`endsWith` 的 4 个 `std::string` 重载（声明 4 行 + 定义 4 个
  函数），保留 `istartsWith`/`iendsWith` 的 4 个 `std::string` 重载与全部
  迭代器模板重载。这与选项中「大小写不敏感版本保留」一致，且避免为 8 处
  调用手写大小写不敏感比较（无谓改动）。
- 审查 #2：初查以为 `CXX1XCXXFLAGS` 承载 `-std` 开关；核对
  `m4/ax_cxx_compile_stdcxx.m4:86-92,122-128` 与 `config.log` 后确认宏写入
  `CXX` 而非 `CXXFLAGS`，该变量恒为空。已据此修正验证方式（检查
  `src/Makefile` 的 `CXX =` 行），避免验收假绿。
- 审查 #3：初查把「约 90 处」作为惯用法改动规模；逐个 grep 核对后收敛为
  `.contains()` 11 处 + `std::erase(_if)` 18 处 + `starts_with`/`ends_with`
  24 处，共 53 处；`std::find(...) != end()` 因 C++20 无 `ranges::contains`
  等价物而排除（gcc 14 实测 `-std=c++20` 报「`contains` is not a member of
  `std::ranges`」，`-std=c++23` 才有）。
- 审查 #4：初稿以「谓词有副作用」为由保留 2 处旧写法；查 cppreference
  `vector/erase2` 与 `deque/erase2` 后确认 `std::erase_if` 明确规定等价于
  `remove_if`+`erase`（每元素一次谓词调用），故 6 处带副作用谓词同样可改；
  已把 16 处修正为 18 处，并把该假设写进「风险与权衡」。
- 审查 #5：初稿把 `test/test-suite.log` 的 `# TOTAL: 1` 当作 CPPUnit 用例数，
  并以为「用例数 > 0」即可。实测该 log 恒为 1 个测试程序，而现存
  `test/aria2c` 二进制跑出 15 个残留失败；已改为同时核对
  `test/aria2c.log` 的 `OK (N)`，并把「干净基线」写入验证章节。
- 审查 #6：初稿用 `git grep -l CXX11_OVERRIDE`（全仓）。本计划文档自身
  含 24 处该字符串，提交后会污染门禁；已全部改为带 `src/*.h` 等路径限定
  的形式，并把 1266 / 1265 的口径写清（后者不含 `configure.ac`）。
- 审查 #7：初稿提到「out-of-tree 构建」未考虑 `configure.ac:558` 的
  `AC_CONFIG_SUBDIRS([deps/wslay])`；已改为优先 `git worktree` 干净副本，
  VPATH 失败时退回该方案。
- 审查 #8：初稿说删 `make_unique` fallback 但未核对数组形式；实测有 18 处
  `make_unique<T[]>(n)`，`std::make_unique<T[]>` 同为值初始化，语义一致，
  已写入「当前上下文」。
- 审查 #9：初稿列 `CXX1XCXXFLAGS` 引用点时漏了 `configure.ac:119`
  （nullptr 探测块里的 `CXXFLAGS="$CXXFLAGS $CXX1XCXXFLAGS"`）。
  `git grep CXX1XCXXFLAGS` 核实共 6 处文本：configure.ac 的
  113/115/119/629 + 两个 Makefile.am。已补全，避免重命名漏改。
- 「单次 make check」的口径：用户选择针对**额外验收矩阵**（不跑 clang
  二次构建 / 不跑 `build_test.sh` 全矩阵）。三个提交各自仍需一次
  `make check` 门禁以保证阶段可编译；阶段 3 的门禁即最终验收，配置为
  `--enable-werror`，不额外再跑一次复核构建。
- 执行前仓库检查：`git rev-parse --is-inside-work-tree` 已确认为 true；
  `.gitignore` 已覆盖构建产物、依赖目录与生成文件，无需补充。
