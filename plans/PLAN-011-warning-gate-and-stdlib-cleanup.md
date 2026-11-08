# 任务：警告门禁与标准库清理（PLAN-011）

## 目标

前 10 轮（PLAN-001~010）把语言基线推到 C++23、把 span/optional/expected/`std::format` 铺开、清掉了绝大部分手写债务。本轮针对**仍然存在但一直没进门的四类问题**：

1. **A. 编译器警告门禁**：项目 10 轮都只开 `-Wall -Werror -Wformat-security`，从未开 `-Wextra`。实测 `-Wextra` 下 **1515 条 warning**（去重 327 个站点 / 75 文件）全部漏网。把警告清零并把 `-Wextra` 写进构建门禁与 CI。
2. **B. 删除 `src/array_fun.h` 的表达式模板**：自研 `expr::Array`/`BinExpr`/`operator&`/`operator|` 位运算代理层只有 `src/BitfieldMan.cc` 一处仍在用，其余 36 个 include 是死代码。
3. **C. `IndexedList` 换标准容器**：598 行自研「`deque` + `unordered_map` 索引」容器只有 2 个使用者，实际只用到 9 个方法，`insert()` 家族与自研迭代器全是死代码。
4. **D. 头文件 `const char*` 形参 → `std::string_view`**：PLAN-008 只做了 672 处 `const std::string&` 中的 60 处，`const char*` 形参 71 处从未处理。

## 当前上下文

### 仓库状态

- 路径 `/home/code/tmp/aria2`，分支 `slim-features`，HEAD `d21f4c11`（`src: 默认使用内部 ARC4 实现并移除 OpenSSL RC4 路径`），`git status --porcelain` 空。
- 语言基线：`configure.ac:111` `AX_CXX_COMPILE_STDCXX([23], [noext], [mandatory])` → `g++ -std=c++23`。
- 规模：`git ls-files 'src/*.h' 'src/*.cc' 'src/*.c'` = **844** 文件；`test/*Test.cc` 177 个（CPPUnit），`make check` 现在是 `OK (915)`。
- 门禁警告开关：`configure.ac:43` `ARIA2_ARG_DISABLE([...])` 之后 `:51 ARIA2_ARG_ENABLE([werror])`；`:531-537`

  ```sh
  if test "x$enable_werror" = "xyes"; then
    AX_CHECK_COMPILE_FLAG([-Wall], [CXXFLAGS="$CXXFLAGS -Wall"])
    AX_CHECK_COMPILE_FLAG([-Werror], [CXXFLAGS="$CXXFLAGS -Werror"])
    AX_CHECK_COMPILE_FLAG([-Wformat-security], [CXXFLAGS="$CXXFLAGS -Wformat-security"])
  fi
  ```

  之后 `:539 WARNCXXFLAGS=$CXXFLAGS`、`:542 AC_SUBST([WARNCXXFLAGS])`；`src/Makefile.am:588` 与 `test/Makefile.am:259` 都用 `AM_CXXFLAGS = @WARNCXXFLAGS@ @CXXSTDCXXFLAGS@ @EXTRACXXFLAGS@` —— **一处改动同时覆盖 src 与 test**。
- CI：`.github/workflows/build.yml`（73 行），matrix `os:[ubuntu-24.04] × compiler:[gcc,clang] × crypto:[openssl,internal] × bittorrent:[with-bt,without-bt]`；`:64 ./configure --with-openssl $FEATURE_FLAGS`、`:68 ./configure --without-openssl $FEATURE_FLAGS`、`:73 make -j"$(nproc 2> /dev/null || sysctl -n hw.ncpu)" check`。`CPPFLAGS/LDFLAGS` 已固定 `-fsanitize=address,undefined -fno-sanitize-recover=undefined -g3`。
- 本机工具链：g++ 14.2.0、autoconf 2.72、automake 1.17、nproc 24。**无 clang++**，clang 侧只能由 CI 验证。
- 公开 ABI 哈希：`git hash-object src/includes/aria2/aria2.h` = `132c72d9f055756769456c35ca7f8e7a879fbaa8`（PLAN-010 阶段 2 起固定，本计划不改变它）。

### 已确认决策

| 决策点 | 结果 |
| --- | --- |
| 纳入范围 | **A + B + C + D 全做** |
| `-Wextra` 落地方式 | **全部清零**：未使用形参改成省略参数名（不用 `[[maybe_unused]]`），真实警告全修，然后把 `-Wextra` 写进 `configure.ac` 的 werror 分支与 CI |
| CI | **只加 `--enable-werror`**；不做 clang-tidy、不做覆盖率、不新增 job 维度 |
| 阶段划分 | 7 阶段，一阶段一提交 |

### 实测数据（本次扫描，剥注释）

#### A 组：`-Wextra` 警告

扫描命令（全 `src/*.cc`）：

```sh
g++ -std=c++23 -DHAVE_CONFIG_H -I. -Isrc -Isrc/includes \
    -Ideps/wslay/lib/includes -Itest \
    -Wall -Wextra -Wpedantic -Wformat-security -fsyntax-only <file>.cc
```

结果 `/tmp/wextra2.log`：**1515 条 warning**，其中：

- **`-Wunused-parameter` 1466 条 → 去重 170 个 `file:line` 站点 / 54 文件**。Top：`src/UnknownLengthPieceStorage.cc`(22)、`src/ValueBaseStructParserStateImpl.h`(16)、`src/RpcMethodImpl.cc`(15)、`src/UnknownLengthPieceStorage.h`(14)、`src/ValueBaseStructParserStateImpl.cc`(7)、`src/ValueBase.h`(6)、`src/bittorrent_helper.{h,cc}`(各 4)、`src/ValueBaseDiskWriter.h`(3)、`src/NullSinkStreamFilter.h`(3)、`src/ByteArrayDiskWriter.cc`(3)、`src/AbstractBtMessage.h`(3)，其余 34 文件各 1~2。
- **真实警告 49 条 → 去重 46 个站点 / 17 文件**：
  - `-Wredundant-move` **38 条**：`src/RpcMethodImpl.cc`(14: 895,915,936,976,995,1113,1149,1166,1202,1236,1296,1336,1362,1382)、`src/RpcMethodImpl.h:361`（模板实例化 ×2，同一行只改一次）、`src/DefaultExtensionMessageFactory.cc`(5: 94,107,137,157,164)、`src/HttpInitiateConnectionCommand.cc`(3: 103,116,135)、`src/FtpInitiateConnectionCommand.cc`(3: 124,153,180)、`src/AuthConfigFactory.cc`(2: 140,158)、`src/RequestGroup.cc`(2: 246,262)、`src/DHTMessageReceiver.cc:112`、`src/DefaultBtMessageFactory.cc:234`、`src/DownloadEngineFactory.cc:103`、`src/MultiUrlRequestInfo.cc:129`、`src/RpcMethod.cc:67`。
  - `-Woverflow` **8 条**（`unsigned int` → `char`）：`src/DHTRoutingTableDeserializer.cc:89,90,100,101`、`src/DHTRoutingTableSerializer.cc:93,94`、`src/util.cc:667`、`src/util.cc:670`。
  - `-Wignored-qualifiers` **2 条**：`src/DHTGetPeersReplyMessage.cc:95,100` —— `static_cast<std::make_unsigned<decltype(clen)>::type>((clen))` 的 `(clen)` 带限定符。
  - `-Wvla` **1 条**：`src/InternalDHKeyExchange.cc:72` `ISO C++ forbids variable length array 'buf'`。**该 TU 本机不编译**（`config.h:438 /* #undef USE_INTERNAL_BIGNUM */`，`configure.ac:250 AM_CONDITIONAL([USE_INTERNAL_BIGNUM], false)`），只能语法自查。
- **test 侧**（脚本 `/tmp/wxt.sh`，含 `-Ideps/wslay/lib/includes -Itest`，已跑完）：`/tmp/wextra_test.log` 共 **1852 条 warning**：
  - `-Wunused-parameter` **1839 条 → 去重 151 个站点 / 22 文件**。Top：`test/MockPieceStorage.h`(600)、`test/MockDHTMessageFactory.h`(165)、`test/MockBtMessageFactory.h`(133)、`test/MockBtMessageDispatcher.h`(108)、`test/MockPeerStorage.h`(35)、`test/MockDHTTaskFactory.h`(18)；另有 `test/AllTest.cc`、`test/BtPortMessageTest.cc`、`test/DefaultBtMessageDispatcherTest.cc`、`test/DHTPingMessageTest.cc`、`test/FixedNumberRandomizer.h` 与 `test/MockBtAnnounce.h`、`test/MockBtMessage.h`、`test/MockBtRequestFactory.h`、`test/MockDHTMessageCallback.h`、`test/MockDHTMessage.h`、`test/MockSegment.h` 等。
  - 真实警告 **13 条**：`src/RpcMethodImpl.h:361` 的 `-Wredundant-move`（已算在 src 侧清单里，同一行）\+ `test/IndexedListTest.cc` 的 **12 条 `-Wdeprecated-copy`**（自研 `IndexedListIterator` 隐式拷贝赋值，行 108,283,295,308,310,312,314,320,322,341,403,415）—— **这 12 条在阶段 5 删除 `IndexedList` 后自动消失，不需要单独修**。
  - 合计：src 170 + test 151 = **301 个 `-Wunused-parameter` 站点 / 76 文件**；真实警告 46（src）+ 0（test 独有）= 46 处。
- **不要写进计划的两类假象**（实测已排除）：`-Woverloaded-virtual`（`src/DiskWriter.h:54,68`、`src/DHTConnection.h:48,51`）与 `src/help_tags.h:42` 的 `elaborated-type-specifier`，在带 `-DHAVE_CONFIG_H` 的正确编译下 **0 命中**；早前不带该宏的扫描把它们当成了真警告。
- `src/aria2api.cc` 的 10 条 error 也是探针假象：`ENABLE_LIBARIA2` 未定义（`src/Makefile.am:537 if ENABLE_LIBARIA2`），本机不编译该 TU。

#### B 组：`array_fun.h`

- 文件 134 行：`arraySize(T(&)[N])`(`:45`)、`array_wrapper<T,N>`、`namespace expr` 的 `BinExpr`/`operator&`/`operator|`/`UnExpr`/`operator~`/`bit_neg`/`Array`/`array(T*)`(`:128`)。
- `expr::array` 只在 `src/BitfieldMan.cc`（`:44 using namespace aria2::expr;`，39 处调用：159,160,161,165,174,175,176,182,191,193,282,283,288,289,333,334,339,340,394,395,400,401,413,414,419,420,447,450,464,465,470,485,486,487,492,493,504,505,787）。
- `array_wrapper` 只在 `src/DHTRoutingTableDeserializer.cc:115`。
- `arraySize` 在 5 个 test 文件（`test/BitfieldManTest.cc:249`、`test/BtPortMessageTest.cc:118,125`、`test/DHTTaskExecutorTest.cc:32`、`test/LongestSequencePieceSelectorTest.cc:31`、`test/OptionParserTest.cc:175`）。
- `#include "array_fun.h"` 共 **70 文件**，真正用到符号的只有 **8 个**：`src/BitfieldMan.cc`(39)、`test/array_funTest.cc`(18)、`test/BtPortMessageTest.cc`(2)、`src/DHTRoutingTableDeserializer.cc`、`test/BitfieldManTest.cc`、`test/DHTTaskExecutorTest.cc`、`test/LongestSequencePieceSelectorTest.cc`、`test/OptionParserTest.cc` → **62 个死 include**。
- 测试 `test/array_funTest.cc`（102 行）专测表达式模板；`test/Makefile.am` 里需同步删除。
- 探针 `/tmp/probe_expr.cc` 证明 `expr::Array` 组合可编译；`std::size(int[5])` 可用（`/tmp/p_size.cc` → `SIZE_OK`），可替代 `arraySize`。

#### C 组：`IndexedList`

- `src/IndexedList.h` 598 行，`template <typename KeyType, typename ValuePtrType> class IndexedList`(`:306`)，`IndexType = std::unordered_map<...>`(`:313`)、`SeqType = std::deque<std::pair<...>>`(`:314`)。
- 使用者：`src/RequestGroupMan.h:50` + `:65 using RequestGroupList = IndexedList<a2_gid_t, std::shared_ptr<RequestGroup>>` + `:67` DownloadResult 版本 + `:71` 字段；`src/RpcMethodImpl.h:49` + `:336 using ItemListType = IndexedList<a2_gid_t, std::shared_ptr<T>>`（`AbstractPaginationRpcMethod<T>::process` 只用 `std::begin/std::end` + `getItems()`）。
- 实际调用到的 API 仅 9 个（`grep -ohE '(requestGroups_|downloadResults_)\.[a-z_]+'`）：`push_back`(`src/RequestGroupMan.cc:168,562`、`:919`)、`size`(`:213,479,481,921`)、`empty`(`:161`)、`get`(`:218,907`)、`remove_if`(`:480 ProcessStoppedRequestGroup`)、`remove`(`:912`)、`pop_front`(`:938`)、`clear`(`:942`)。**`insert()` 家族（`:364,386,402,419`）、`push_front`、`erase(iterator)`、自研 `IndexedListIterator` 与 5 个 iterator traits typedef 全部无用**。
- 测试 `test/IndexedListTest.cc`（442 行）。

#### D 组：`const char*` 形参

- 71 处 / 26 头文件。按命中数：`src/OptionHandlerImpl.h`(16)、`src/util.h`(15)、`src/ColorizedStream.h`(4)、`src/SocketCore.h`(4)、`src/bittorrent_helper.h`(3)、`src/BufferedFile.h`(2)、`src/GZipEncoder.h`(2)、`src/GZipFile.h`(2)、`src/GroupId.h`(2)、`src/HttpHeader.h`(2)、`src/MSEHandshake.h`(2)、`src/RequestGroupMan.h`(2)、`src/ValueBase.h`(2)，其余 13 个各 1。
- `src/util.h` 精确行：`:111 strip(..., const char* chars = DEFAULT_STRIP_CHARSET)`、`:134`、`:148`、`:531 split(..., const char* delims, ...)`、`:535 const char* dlast = delims + numDelims;`、`:595 streq`、`:636/:647 strieq`、`:661/:682/:693 startsWith/istartsWith`、`:716 iendsWith`、`:720 strless`、`:787 makeString`。
- **明确不改**：`src/SocketCore.h`(4，`callGetaddrinfo`/`inetPton` 是 C API 包装，需 null-terminated)、`src/ValueBase.h`(2，`String(const char*, size_t)` 是 PLAN-008 阶段 3 判定的高消费点 86 处)、`src/BufferedFile.h`(2，`fopen` 语义)、`src/GZipFile.h`/`src/GZipEncoder.h`(共 4，zlib 语义)。
- **`src/RpcMethodImpl.h` 的 34 处 `static const char* getMethodName()` 是返回类型不是形参，不改。**

## 待确认决策

无（用户已确认全部三项）。

## 提议的方法

### 阶段 1：`-Wunused-parameter` 清零（327 站点 / 75 文件）

按用户要求「全部清零」，未使用形参一律**删掉形参名**（保留类型），不加 `[[maybe_unused]]`：

```cpp
// before
void visit(const String& v) override;
// after
void visit(const String&) override;
```

- 只对**定义体确实不使用该形参**的站点动手。判断依据是编译器的 `unused parameter` 报告，不是人工猜。
- 虚函数族必须整族一致（`src/ValueBase.h` 的 `ValueBaseVisitor` 6 个 visit、`src/ValueBaseStructParserStateImpl.h` 16 个、`src/BinaryStream.h` 2 个、`src/DiskAdaptor.h`/`src/DiskWriter.h`/`src/URISelector.h` 等）。删名不改变签名，`override` 仍匹配。
- 生成辅助脚本 `/tmp/p11_unused.py`：解析 `/tmp/wextra2.log` 与 `/tmp/wextra_test.log` 的 `unused parameter 'NAME'` 行 → 定位 `<file>:<line>:<col>` 对应的形参名 → 只在该 token 范围内删名（跨行的保持缩进）。**脚本同时处理 `.h` 与 `.cc`**，因为不少站点在头文件声明里。
- 阶段 1 结束时 `-Wextra` 下 `-Wunused-parameter` 必须为 0（src + test），否则阶段 3 的门禁会红。

### 阶段 2：真实警告 46 处清零

- **`-Wredundant-move` 38 处**：`return std::move(x);` → `return x;`（返回类型是 `std::unique_ptr<T>`/值类型，`std::move` 抑制不了 NRVO 反而阻止省略）。`src/RpcMethodImpl.h:361` 是模板，一处改动覆盖两个实例化。
- **`-Woverflow` 8 处**：
  - `src/util.cc:667,670`（`iso8859p1ToUtf8`）：`dest += 0xc2u;` → `dest += static_cast<char>(0xc2u);`，`dest += c & (~0x40u);` → `dest += static_cast<char>(c & (~0x40u));`。
  - `src/DHTRoutingTableDeserializer.cc:89,90,100,101` 与 `src/DHTRoutingTableSerializer.cc:93,94`：核对原文后加 `static_cast<char>(...)`（这 6 处是 `unsigned char` 边界常量的写入，语义不变）。
- **`-Wignored-qualifiers` 2 处**：`src/DHTGetPeersReplyMessage.cc:95,100` 把 `static_cast<T>((clen))` 的多余括号去掉（`(clen)` 里的顶层限定符让 `-Wignored-qualifiers` 报），改成 `static_cast<T>(clen)`。
- **`-Wvla` 1 处**：`src/InternalDHKeyExchange.cc:72` 的 VLA 改 `std::array` 或 `std::vector`（长度是编译期还是运行期需读原文确认；该 TU 本机不编译，只能语法自查 + 标「未验证」）。

### 阶段 3：打开 `-Wextra` 门禁

- `configure.ac:531-537` 的 werror 分支加一行：

  ```sh
  AX_CHECK_COMPILE_FLAG([-Wextra], [CXXFLAGS="$CXXFLAGS -Wextra"])
  ```

  `WARNCXXFLAGS` 自动带上，`src/Makefile.am:588` 与 `test/Makefile.am:259` 都生效，无需改两个 Makefile.am。
- `.github/workflows/build.yml:64,68` 的两次 `./configure` 各加 `--enable-werror`。
- 这一阶段的提交必须让 `make check` 在**带 `-Wextra -Werror`** 下通过，是对前两阶段的真正验收。

### 阶段 4：移除 `array_fun.h` 表达式模板

- 重写 `src/BitfieldMan.cc` 里 39 处 `expr::array(...)` 的位运算表达式，改用显式循环或 `std::span` + 手写辅助：
  - `~array(a) & array(b)` 模式 → 新增文件内静态辅助 `void andNotInto(std::span<unsigned char> dst, std::span<const unsigned char> a, std::span<const unsigned char> b)` 之类；`|` 模式 → `orInto(...)`。
  - 由于这些表达式是 `bitfield::getFirstSetBitIndex(...)` / `countSetBitSlow(...)` / `copyBitfield(...)` / `getFirstNSetBitIndex(...)` 的**实参**（模板接受任意容器），改成「先算到临时 `std::vector<unsigned char>` 再传给消费者」最直接，代价是一次分配 —— 这些都在 `BitfieldMan` 的缺块查询路径上，**阶段 4 必须附带一次性能对比**（`test/BitfieldManTest.cc` 的用例集跑一遍计时；如退化明显则改为就地按需计算，不建临时缓冲）。
- 删 `src/array_fun.h` 的 `namespace expr`（`BinExpr`/`UnExpr`/`Array`/`array`/`operator&`/`operator|`/`operator~`/`bit_neg`），保留 `arraySize` + `array_wrapper`（`src/DHTRoutingTableDeserializer.cc:115` 还在用 `array_wrapper`；`arraySize` 可顺手换 `std::size` 并删掉，见下）。
- 36 个死 include：`git grep -l '#include "array_fun.h"' -- src test` 共 70 个，除 `src/BitfieldMan.cc`、`src/DHTRoutingTableDeserializer.cc`、`test/array_funTest.cc`、`test/BtPortMessageTest.cc`、`test/BitfieldManTest.cc`、`test/DHTTaskExecutorTest.cc`、`test/LongestSequencePieceSelectorTest.cc`、`test/OptionParserTest.cc` 外的 **62 个全部删除**。
- `arraySize` → `std::size`（5 个 test 文件），然后删 `arraySize`；`array_wrapper` 换成 `std::array<unsigned char, 255>`（`src/DHTRoutingTableDeserializer.cc:115` 只有 `.data()`/`.size()` 用法需核对），全部换完后**删除 `src/array_fun.h` 与 `test/array_funTest.cc`**，并在 `test/Makefile.am` 移除该测试源。
- 如果 `array_wrapper` 的语义（无初始化 + 255 定长）与 `std::array` 有差异（比如原文用 `array_wrapper<unsigned char, 255> buf;` 依赖未初始化 POD 行为），保留 `array_wrapper` 只在原文件内定义。

### 阶段 5：`IndexedList` 换标准容器

- 新建 `src/IndexedDeque.h`（约 80 行）：`std::deque<std::pair<K, V>>` + `std::unordered_map<K, typename SeqType::iterator>`，只提供 `push_back`、`get`、`remove`、`remove_if`、`pop_front`、`clear`、`size`、`empty`、`begin/end`、`value_type`/`iterator`/`const_iterator`。
- `src/RequestGroupMan.h:50,65,67,71` 与 `src/RpcMethodImpl.h:49,336` 改用新头；`src/RpcMethodImpl.h` 的 `AbstractPaginationRpcMethod` 若只需 `begin/end`，直接换 `std::deque<std::shared_ptr<T>>` 更彻底（需核对 `getItems()` 返回类型与 `getPaginationRange` 用法）。
- 删 `src/IndexedList.h`；`test/IndexedListTest.cc` 按新 API 重写为 `test/IndexedDequeTest.cc`（或删除并在 `test/Makefile.am` 同步）。
- **必须闭环的检查**：`RequestGroupMan::removeStoppedGroup` 依赖 `remove_if` 的「边遍历边删」语义（`src/RequestGroupMan.cc:479-481`），新容器要保证遍历中删除的安全性（用 `std::erase_if` 或显式迭代 + 索引映射更新）。

### 阶段 6：`const char*` 形参 → `std::string_view`（约 43 处）

- 改：`src/OptionHandlerImpl.h`(16，`const char* description = NO_DESCRIPTION` → `std::string_view`；`src/OptionHandler.h:51 extern const char NO_DESCRIPTION[];` 保留)、`src/ColorizedStream.h`(4，**只替换现有 `const char*` 版本，绝不新增重载** —— PLAN-008 阶段 4 的 `operator<<` 歧义教训)、`src/GroupId.h`(2，内部 `strtoull` 前先转 `std::string`)、`src/prefs.h`(1)、`src/uri.h`(1)、`src/help_tags.h`(1)、`src/Logger.h`(1)、`src/RequestGroupMan.h`(2)、`src/bittorrent_helper.h`(3)、`src/MSEHandshake.h`(2)、`src/HttpHeader.h`(2) 等。
- `src/util.h`(15) 单独处理：`strip`/`split`/`splitIterM` 内部用 `strchr`/`delims + numDelims`，改 `string_view` 后要改成 `std::string_view::find_first_of`；`streq`/`startsWith`/`strieq`/`iendsWith`/`strless` 的 `const char* b` 要改成 `std::string_view b`（内部已用 `last - first` 长度比较，改动小）；`makeString(const char*)` 保留（`std::string` 构造语义）。
- 不改：`src/SocketCore.h`(4)、`src/ValueBase.h`(2)、`src/BufferedFile.h`(2)、`src/GZipFile.h`/`src/GZipEncoder.h`(4)。

### 阶段 7：收尾

- 计划文档勾选、状态行、实测门禁表、债务前后对比、实际偏差。
- MEMORY.md 记录。

## 分步计划

1. [x] 阶段 1：`-Wunused-parameter` 清零（实测 344 站点 / 77 文件，删形参名）→ 提交 `PLAN-011 阶段1: 清除未使用形参警告`

### 阶段 1 实测（2026-11-08）

- 脚本 `/tmp/p11_unused.py`：从警告日志提取 `file:line:col` + 形参名，倒序删除「标识符 + 前导空白」；名字对不上就跳过（保证幂等）。
- **实际删除 344 个形参名 / 77 文件**（计划估 301 个站点，偏差原因：①同一行多个形参在日志里算一个 `file:line`；②首轮扫描缺 `-Ideps/wslay/lib/includes`，`src/WebSocketSession.cc` 漏扫，补扫后多 5 处）。
- 补扫脚本：`/tmp/wx2.sh`（src，带 wslay 头路径 + 跳过 `src/aria2api.cc`，`xargs -P8`）、`/tmp/wxt3.sh`（test，带 `-DA2_TEST_DIR=... -DA2_TEST_OUT_DIR=...`）。
- 验证：`/tmp/wextra4.log`（src）`error:` 0 / `unused parameter` **0** / 其余 49；`/tmp/wextra_test3.log`（test）`error:` 0 / `unused parameter` **0** / 其余 13。
- 就地 `make -j8 -C src` + `make -j8 -C test` 均 rc=0、0 error。`clang-format -i` 后 `77 files changed, 349 insertions(+), 494 deletions(-)`。
- 干净 worktree `/tmp/aria2-m11-p1`（base `d21f4c11`）+ `git apply /tmp/p11-stage1.patch` + `autoreconf -i` + `./configure --enable-werror`（`CXX = g++ -std=c++23`、`WARNCXXFLAGS = -Wall -Werror -Wformat-security`）+ `make -j8 check` → `# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`OK (915)`、`error:` 0、无 `Failures !!!`、ABI `132c72d9f055756769456c35ca7f8e7a879fbaa8`。

2. [x] 阶段 2：真实警告 46 处（redundant-move 38 / overflow 8 / ignored-qualifiers 2 / vla 1）→ 提交 `PLAN-011 阶段2: 修复剩余编译器警告`

### 阶段 2 实测（2026-11-08）

- `-Wredundant-move`：**35 个唯一站点**全部 `return std::move(X);` → `return X;`（均在 `src/RpcMethodImpl.cc` 14、`src/DefaultExtensionMessageFactory.cc` 5、`src/HttpInitiateConnectionCommand.cc` 3、`src/FtpInitiateConnectionCommand.cc` 3、`src/AuthConfigFactory.cc` 2、`src/RequestGroup.cc` 2、`src/DHTMessageReceiver.cc` 1、`src/DefaultBtMessageFactory.cc` 1、`src/DownloadEngineFactory.cc` 1、`src/MultiUrlRequestInfo.cc` 1、`src/RpcMethod.cc` 1）。**保留 10 处 `std::move(member_)`**（`src/bencode2.cc:73`、`src/DefaultPieceStorage.cc:843`、`src/DHTMessageTrackerEntry.cc:107`、`src/HttpConnection.cc:79`、`src/HttpHeaderProcessor.cc:465`、`src/RequestGroupEntry.cc:61`、`src/ValueBase.cc:57`、`src/ValueBaseParserController.cc:96`、`src/DHTMessageReceiver.cc:102`、`src/DownloadEngineFactory.cc:92`）—— 这 10 处是移出成员/解引用指针，编译器不报。
- `-Woverflow` 8 处：`src/DHTRoutingTableDeserializer.cc` 4 + `src/DHTRoutingTableSerializer.cc` 2 的 `header[0] = 0xa1u;` 等 → `static_cast<char>(0xa1u)`；`src/util.cc:667,670` 的 `dest += 0xc2u/0xc3u` → `dest += static_cast<char>(...)`（同时 `dest += c & (~0x40u)` 也加了 `static_cast<char>`，行为不变）。
- `-Wignored-qualifiers` 2 处：`src/DHTGetPeersReplyMessage.cc:95,138` 的 `std::make_unsigned<decltype(clen)>::type` 因 `clen` 是 `const auto` 而使结果带 const → `std::make_unsigned_t<std::remove_cv_t<decltype(clen)>>`。
- `-Wvla` 1 处：`src/InternalDHKeyExchange.cc:72` 的 `unsigned char buf[pbytes]` → `std::vector<unsigned char> buf(pbytes)` + `std::as_writable_bytes(std::span{buf})` + `n(buf.data(), pbytes)`，补 `#include <vector>`。该 TU 本机默认不编译（`USE_INTERNAL_BIGNUM` 未定义），单独 `-fsyntax-only` 验证。
- 验证：`/tmp/wextra5.log`（src）**0 条 warning**、`error:` 0；`/tmp/wextra_test4.log`（test）只剩 `test/IndexedListTest.cc` 的 12 条 `-Wdeprecated-copy`（阶段 5 删除后消失）+ 1 条 `test/CookieBoxTest.cc` 的 `CookieBox.h: No such file`（该文件未被 `test/Makefile.am` 引用，是历史死文件，非本次引入）。
- 就地 `make -j8 -C src` + `make -j8 -C test` 均 rc=0。`clang-format -i` 后 `17 files changed, 62 insertions(+), 58 deletions(-)`。
- 干净 worktree `/tmp/aria2-m11-p2`（base `67757636`）门禁：`# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`OK (915)`、`error:` 0、无 `Failures !!!`、ABI `132c72d9f055756769456c35ca7f8e7a879fbaa8`。

3. [x] 阶段 3：`configure.ac` 加 `-Wextra`、CI 加 `--enable-werror` → 提交 `PLAN-011 阶段3: 打开 -Wextra 警告门禁`

### 阶段 3 实测（2026-11-08）

- `configure.ac:534` 在 `-Wall` 与 `-Werror` 之间插入 `AX_CHECK_COMPILE_FLAG([-Wextra], [CXXFLAGS="$CXXFLAGS -Wextra"])`；`.github/workflows/build.yml:65,69` 两次 `./configure` 各加 `--enable-werror`。
- 配置结果：`src/Makefile` 与 `test/Makefile` 均为 `WARNCXXFLAGS = -Wall -Wextra -Werror -Wformat-security`（`grep -c -- '-Wextra'` 各 2，含 `AM_CXXFLAGS` 行）。
- **重要发现：`-fsyntax-only` 扫不到中端警告**。首次开 `-Werror` 后在 `src/JsonParser.cc:504` 报 `-Werror=implicit-fallthrough`——`-Wimplicit-fallthrough` 在 GIMPLE 上运行，`-fsyntax-only` 不产生。改用「真实编译 + 临时去掉 `-Werror`」（`sed -i 's/-Werror//' src/Makefile test/Makefile` + `make -k`）全量扫一遍，共暴露 **3 处**：`src/BencodeParser.cc:156`、`src/JsonParser.cc:504`、`src/LibsslTLSContext.cc:117`。三处都是**有意旁路**，但注释位置不对（注释与 `case` 标签之间隔着 `}` 或 `#endif`，GCC 不认）——只把注释移到闭括号/`#endif` 之后，不加 `[[fallthrough]]`，比原本更小。
- 另外 `test/IndexedListTest.cc` 的 12 条 `-Wdeprecated-copy` 在 `-Werror` 下变成错误（原计划以为能留到阶段 5），所以在 `src/IndexedList.h:107` 补了一行 `IndexedListIterator& operator=(const IndexedListIterator&) = default;`（该类阶段 5 会被删除，这行随之消失）。
- 干净 worktree `/tmp/aria2-m11-p3`（base `deec0c38`）全量重建（`-Werror` 生效）：`# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`OK (915)`、`error:` 0、C++ `warning:` 0（唯一一条 `warning:` 是 `deps/wslay` 的 libtool `-version-info` 提示）、无 `Failures !!!`、ABI `132c72d9f055756769456c35ca7f8e7a879fbaa8`。

4. [x] 阶段 4：移除 `array_fun.h` 表达式模板 + 62 个死 include → 提交 `PLAN-011 阶段4: 移除 array_fun 表达式模板`

### 阶段 4 实测（2026-11-08）

- 删除 `src/array_fun.h`（134 行）与 `test/array_funTest.cc`（102 行 / 4 个用例），两者的 `src/Makefile.am`、`test/Makefile.am` 条目一并删。
- `src/BitfieldMan.cc` 的 18 个 `expr::array` 站点（39 处 `array()` 调用）换成 3 个匿名 namespace 辅助函数：`makeMissingMask(length, bf, use, peer, filter)`（nullptr 表示该输入不参与）、`makeIgnoreMask(length, ignore, bf, use, filter)`、`makeAndMask(length, bf, filter)`。逐字节循环写出 `std::vector<unsigned char>`，消费者（`bitfield::test`/`getFirstSetBitIndex`/`getFirstNSetBitIndex`/`countSetBitSlow`/`countCompletedLength`）本来就接任意可索引容器，无需改动。`copyBitfield(unsigned char* dst, const Array& src, size_t)` → `copyBitfield(unsigned char* dst, std::span<const unsigned char> src, size_t)`。
- `src/DHTRoutingTableDeserializer.cc` 的 `array_wrapper<unsigned char, 255> buf` → `std::array<unsigned char, 255> buf`（`buf` 作指针处补 `.data()`）。
- `arraySize(arr)` → `std::size(arr)` 共 7 处，分布在 `test/BitfieldManTest.cc`、`test/BtPortMessageTest.cc`（2）、`test/DHTTaskExecutorTest.cc`、`test/LongestSequencePieceSelectorTest.cc`、`test/OptionParserTest.cc`。
- 死 include 删 **62 个**（`#include "array_fun.h"` 原 70 文件，实际用符号的只 8 个）。
- 用例数 **915 → 911**（`array_funTest` 那 4 个用例），`OK (911)` 为新的稳定值。
- 干净 worktree `/tmp/aria2-m11-p4`（base `e8796b3e`）全量重建：`# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`OK (911)`、`error:` 0、C++ `warning:` 0、无 `Failures !!!`、ABI `132c72d9f055756769456c35ca7f8e7a879fbaa8`。
- 教训：**不要对仅改了一行 include 的文件跑 `clang-format`**——仓库并非整体 clang-format-clean，会顺带重排 `test/UtilTest1.cc`（769 行）、`src/Makefile.am`（877 行）等无关内容；且 `clang-format` 会把 `*.am` 当 C++ 处理。只对实质修改的文件格式化。

5. [x] ~~阶段 5：`IndexedList` → `IndexedDeque`（或 `std::deque`）~~ —— **取消**

### 阶段 5 实测（2026-11-08）：前提不成立，取消

- 计划假设「`IndexedList` 598 行里只用到 9 个方法，可缩到约 80 行」。实测推翻：
  - `src/RequestGroupMan.cc` 实际用到 `push_back`(:167,:561,:918)、`push_front`(:437)、`insert(size_t, Key, Value)`(:207)、`insert(iterator, KeyFunc, first, last)`(:586)、`insert(size_t, KeyFunc, first, last)`(:198)、`move`(:227)、`get`(:217,:219,:906)、`remove`(:239,:911)、`remove_if`(:479)、`pop_front`(:548,:937)、`operator[]`(:923)、`begin`(:547)、`size`/`empty`/`clear`。
  - 真正死掉的只有 `erase(iterator)`（`src/IndexedList.h:462`，只有 `test/IndexedListTest.cc` 的 `testErase` 在用）和 `insert(iterator, Key, Value)`（`:387`），合计约 25 行。
  - 自定义双向迭代器（`:59-130`）是活的：`getPaginationRange` 用 `std::advance`、`move()` 用 `std::rotate`。
- 结论：**活代码占 96%**。「重写成 IndexedDeque」只会把 598 行变成约 570 行，净收益为负；`move()` 的 `OFFSET_MODE_CUR/SET/END` 与 `insert` 的索引维护又是 `aria2.changePosition` RPC 的用户可见语义，重写有回归风险。经用户确认，**阶段 5 取消**，`src/IndexedList.h` 与 `test/IndexedListTest.cc` 原样保留。
6. [x] 阶段 6：头文件 `const char*` 形参 → `std::string_view` → 提交 `PLAN-011 阶段6: 头文件 const char* 形参改 string_view`

### 阶段 6 实测（2026-11-08）：只改有真实收益的 6 处

计划写的「约 43 处」是按 grep 计数估的。按「是否真有调用方需要 `.c_str()`」逐个核对后，**只有 6 处形参真能省掉调用方的 `.c_str()`**，其余传的都是字面量（改与不改调用点零差别），或语义上不能改。按最小改动原则只做这 6 处：

| 形参 | 文件 | 省掉的调用方 `.c_str()` |
|---|---|---|
| `GroupId::expandUnique(a2_gid_t&, std::string_view hex)` | `src/GroupId.h:59` / `src/GroupId.cc:71` | `src/RpcMethodImpl.cc:199` |
| `GroupId::toNumericId(a2_gid_t&, std::string_view hex)` | `src/GroupId.h:60` / `src/GroupId.cc:105` | `src/aria2api.cc:181`、`src/download_helper.cc:111`、`test/GroupIdTest.cc:46,49` |
| `Logger::writeLog(..., std::string_view trace)` | `src/Logger.h:73` / `src/Logger.cc:192` | `src/Logger.cc:219`（`ex.stackTrace()`） |
| `HttpHeader::fieldContains(InterestingHeader, std::string_view value)` | `src/HttpHeader.h:163` / `src/HttpHeader.cc:196` | `src/HttpServerCommand.cc:215,217` 的字面量走 string_view |
| `idInterestingHeader(std::string_view hdName)` | `src/HttpHeader.h:171` / `src/HttpHeader.cc:249` | `src/HttpHeaderProcessor.cc:345` |
| `writeStackTrace(Output&, std::string_view)` | `src/Logger.cc:186` | 同上 |

- `src/GroupId.cc` 两个函数原来的 `for (i = 0; hex[i]; ++i)` 改成 `for (auto c0 : hex)`，`i` 改为 `hex.size()`（`hexCharToUInt` 语义不变）。
- `idInterestingHeader` 原来的 `util::strless(const char*, const char*)` 比较器换成 `[](const char* a, std::string_view b) { return a < b; }`，相等判断 `strcmp(*i, hdName) != 0` → `*i != hdName`。
- `src/util.h` 新增一个重载 `template <typename InputIterator> bool strieq(InputIterator first, InputIterator last, std::string_view b)`（长度先行比较 + `std::equal(..., CaseCmp())`）。**不删**原来的 `const char* b` 版本：传字面量时精确匹配仍选它，两者共存不产生歧义（`const char*` → `string_view` 是用户定义转换，劣于精确匹配）。

#### 明确不改（连同理由）

- `src/OptionHandlerImpl.h`(16) + `src/AbstractOptionHandler.h` 的 `const char* description`：链路末端 `virtual const char* getDescription() const`（`src/OptionHandler.h:54`）被 `--help` 与 `aria2.getGlobalOption` 消费，`description_` 成员是 `const char*`；改成 `string_view` 需连带改整个虚函数族 + 15 个子类 + `src/help_tags.cc`，而 `string_view` 不保证 null 结尾、无法直接存 `const char*`。调用方全是字面量，零收益。
- `src/ColorizedStream.h`(4) 的 `operator<<(ColorizedStream&, const char*)`：PLAN-008 阶段 4 已实测，把 `operator<<` 改成 `string_view` 会与 `std::ostream::operator<<(const std::string&)` 产生 `ISO C++ says that these are ambiguous`。
- `src/MSEHandshake.h` 的 `decodeLength16(const char* buffer)` / `verifyPadLength(...)`：形参是**字节缓冲**不是字符串。
- `src/prefs.h` 的 `Pref(const char* k, size_t)`：`k` 被存成成员 `const char* k;`，生命周期依赖调用方传静态字面量，改 `string_view` 会丢掉这个契约。
- `src/bittorrent_helper.h`(3) 的 `const char* msgName`、`src/RequestGroupMan.h`(2) 的 `const char* status`、`src/help_tags.h` 的 `idHelpTag(const char*)`、`src/uri.h:79` 的 `parse(..., const char* base)`：调用方全是字面量；且 `uri::parse` 的 `base` 形参目前已无任何调用方传它。
- `src/util.h` 的 `streq`/`startsWith`/`istartsWith`/`iendsWith`/`strless`/`makeString`：都是「扫到 `\0` 为止」的 C 串语义，`strip`/`stripIter`/`lstripIter`/`splitIterM` 的 `chars`/`delims` 同理（`strchr` 换 `find_first_of` 无行为差异但无调用方受益）。这类保留与 `error_code.h`、`ValueBase` 的选择一致：语义是 C 串就保持 C 串。

- 干净 worktree `/tmp/aria2-m11-p6`（base `b9b7078a`）全量重建：`# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`OK (911)`、`error:` 0、C++ `warning:` 0、无 `Failures !!!`、ABI `132c72d9f055756769456c35ca7f8e7a879fbaa8`。12 文件 +43/−29。
7. [ ] 阶段 7：收尾（勾选、状态行、实测表、偏差）→ 提交 `PLAN-011: 勾选全部阶段、标记计划完成并记录实测门禁结果`

## 预计修改的文件

- 阶段 1：实测 **77 个文件**（`src/` 54 + `test/` 23，含 `test/MockPieceStorage.h` 等 11 个 `test/Mock*.h`、`test/FixedNumberRandomizer.h`）。
- 阶段 2：`src/RpcMethodImpl.{h,cc}`、`src/DefaultExtensionMessageFactory.cc`、`src/HttpInitiateConnectionCommand.cc`、`src/FtpInitiateConnectionCommand.cc`、`src/AuthConfigFactory.cc`、`src/RequestGroup.cc`、`src/DHTMessageReceiver.cc`、`src/DefaultBtMessageFactory.cc`、`src/DownloadEngineFactory.cc`、`src/MultiUrlRequestInfo.cc`、`src/RpcMethod.cc`、`src/util.cc`、`src/DHTRoutingTableDeserializer.cc`、`src/DHTRoutingTableSerializer.cc`、`src/DHTGetPeersReplyMessage.cc`、`src/InternalDHKeyExchange.cc`。
- 阶段 3：`configure.ac`、`.github/workflows/build.yml`。
- 阶段 4：`src/BitfieldMan.cc`、`src/array_fun.h`（删）、`src/DHTRoutingTableDeserializer.cc`、`test/Makefile.am`、`test/array_funTest.cc`（删）、`test/BtPortMessageTest.cc`、`test/BitfieldManTest.cc`、`test/DHTTaskExecutorTest.cc`、`test/LongestSequencePieceSelectorTest.cc`、`test/OptionParserTest.cc`，以及 62 个含死 include 的 `src/*.cc`。
- 阶段 5：`src/IndexedDeque.h`（新增）、`src/IndexedList.h`（删）、`src/RequestGroupMan.{h,cc}`、`src/RpcMethodImpl.h`、`test/IndexedListTest.cc`（改名/重写）、`test/Makefile.am`。
- 阶段 6：`src/OptionHandlerImpl.{h,cc}`、`src/util.{h,cc}`、`src/ColorizedStream.{h,cc}`、`src/GroupId.{h,cc}`、`src/prefs.h`、`src/uri.{h,cc}`、`src/help_tags.{h,cc}`、`src/Logger.{h,cc}`、`src/RequestGroupMan.h`、`src/bittorrent_helper.{h,cc}`、`src/MSEHandshake.{h,cc}`、`src/HttpHeader.{h,cc}`。
- 阶段 7：`plans/PLAN-011-warning-gate-and-stdlib-cleanup.md`。

## 测试与验证

### 每阶段门禁（干净 worktree）

```sh
rm -rf /tmp/aria2-m11-pN && git worktree prune
git worktree add --detach /tmp/aria2-m11-pN HEAD
cd /tmp/aria2-m11-pN && git apply /tmp/p11-stageN.patch
autoreconf -i && ./configure --enable-werror
nohup make -j8 check > /tmp/p11-pN-check.log 2>&1 &
```

轮询日志（**不要用 `pgrep -f`，会匹配 bash 自身命令行死循环**），判据：

| 判据 | 期望 |
| --- | --- |
| `# TOTAL:` / `# PASS:`（取**最后一组**，make 会打印两组） | `1` / `1` |
| `# FAIL` / `# ERROR` | `0` / `0` |
| `grep -c 'OK (' test/aria2c.log` | `1`（阶段 1~6 为 `OK (915)`，阶段 5 若删 `IndexedListTest` 则相应减少并在文档记录新值） |
| `grep -c 'error:' <log>` | `0` |
| `grep -c 'Failures !!!' test/aria2c.log` | `0` |
| `git hash-object src/includes/aria2/aria2.h` | `132c72d9f055756769456c35ca7f8e7a879fbaa8` |
| 阶段 3 起 `grep 'CXXFLAGS' src/Makefile \| head -1` | 含 `-Wextra` |

### 每阶段专项验证

1. **阶段 1**：`-Wextra` 单独扫描必须 `-Wunused-parameter` = 0。
   ```sh
   for f in $(git ls-files 'src/*.cc'); do
     g++ -std=c++23 -DHAVE_CONFIG_H -I. -Isrc -Isrc/includes \
         -Ideps/wslay/lib/includes -Wall -Wextra -fsyntax-only $f
   done 2>&1 | grep -c 'unused parameter'   # 期望 0
   ```
   test 侧用 `/tmp/wxt.sh` 同法。**假绿风险**：本机 `-fsyntax-only` 不实例化所有模板，`src/RpcMethodImpl.h` 的模板站点要靠 `make check` 兜底。
2. **阶段 2**：`-Wextra` 下非 `unused parameter` 的 warning 数 = 0（同上命令 `grep -v 'unused parameter' | grep -c warning:`）。**假绿风险**：`src/InternalDHKeyExchange.cc` 本机不编译，只能 `-fsyntax-only` 语法自查并标「未验证」。
3. **阶段 3**：`./configure --enable-werror` 后 `grep -c -- '-Wextra' src/Makefile` ≥ 1，且 `make check` 全绿 —— 这是**唯一能证明前两阶段真的清零**的步骤；不做这一步，阶段 1/2 的「清零」在 CI 上仍是未验证的。
4. **阶段 4**：`git grep -c 'array_fun.h' -- src test` = 0（除 `src/DHTRoutingTableDeserializer.cc` 保留时核对）；`test/array_funTest.cc` 已删；`test/BitfieldManTest.cc` 的缺块查询用例全绿；附带 `BitfieldMan` 用例计时对比（阶段前后各跑一次 `test/aria2c` 的 `BitfieldManTest` 用例，记录耗时）。
5. **阶段 5**：`git grep -c 'IndexedList' -- src test` = 0；`test/RequestGroupManTest.cc`、`test/RpcMethodTest.cc`（分页方法）全绿；**遍历中删除**路径由 `test/RequestGroupManTest.cc` 的 `removeStoppedGroup` 用例覆盖。
6. **阶段 6**：`git grep -cE 'const char\* [A-Za-z_]+[,)]' -- 'src/*.h'` 从 71 降到 ≤ 28（保留项）。

## 验收标准

1. `./configure --enable-werror && make -j8 check` 在干净 worktree 下全绿，`OK (915)`（或阶段 5 后的新值）且无 `Failures !!!`、`error:` = 0。
2. `-Wextra` 下 src 与 test 的 warning 均为 **0**。
3. `configure.ac` 的 werror 分支含 `-Wextra`；CI 的两次 `./configure` 都带 `--enable-werror`。
4. `src/array_fun.h` 与 `test/array_funTest.cc` 已删除，`git grep 'array_fun.h'` 剩 0 处（或仅保留 `array_wrapper` 的内联定义，需在「实际偏差」说明）。
5. `src/IndexedList.h` 与 `test/IndexedListTest.cc` 已删除/替换，`git grep 'IndexedList'` = 0。
6. 头文件 `const char*` 形参从 71 降到 ≤ 28（保留 C API / `String(const char*, size_t)` / `fopen` / zlib 语义）。
7. `src/includes/aria2/aria2.h` 哈希保持 `132c72d9f055756769456c35ca7f8e7a879fbaa8`。
8. 7 次提交（6 代码 + 1 收尾），每个阶段文档勾选与实际一致。

## 风险与权衡

1. **阶段 1 的 301 个站点里，删形参名可能掩盖「本该使用却没用」的真实缺陷**。缓解：`-Wunused-parameter` 是编译器判定，但仍需人工抽查 `src/RpcMethodImpl.cc`(15)、`src/UnknownLengthPieceStorage.{h,cc}`(36) 这两簇（后者是「未实现」的桩类，形参本就该忽略）。
2. **阶段 1 改 `test/Mock*.h` 会牵连大量 test TU**。本机增量 `make -C test` 会假绿（PLAN-008 阶段 3 实测），必须 `touch test/*.cc test/*.h` 或直接干净 worktree 门禁。
3. **阶段 3 打开门禁后，任何新警告都会红**。这是目标，但也意味着 CI 的 clang-18 侧可能出现 gcc-14 没有的警告 —— **本机无 clang++，无法预验证**，只能在文档标「未验证」并接受 CI 首轮可能失败。
4. **阶段 4 的性能风险**：`BitfieldMan` 的缺块查询在 BT 下载热路径上，把 `expr::array` 的惰性表达式改成「先算临时缓冲」会多一次 `vector` 分配。必须做前后计时对比；退化明显则改成把结果写回已有成员缓冲（`bitfield_`/`useBitfield_` 之外新增一块 reusable scratch）。
5. **阶段 5 的语义风险**：`IndexedDeque` 要替代的是「按 key 查找 + 保序 + 遍历中删除」。`std::unordered_map<K, deque::iterator>` 的 `iterator` 在 `deque::push_back` 后会失效吗？——不会（`deque` 的 `push_back` 不失效已有迭代器），但 `pop_front` 会失效被删元素对应的迭代器，所以 `remove_if` 必须同步从 map 里 erase。**这一点在实现时必须写进注释并有单测**。
6. **阶段 6 的 `src/util.h` 是「被 255 个文件 include」的公共头**，改 `strip`/`split` 会牵连极广。必须逐个调用点核对是否依赖 `const char*` 的 null-terminated 语义（`src/util.h:535 const char* dlast = delims + numDelims;` 就是靠 `numDelims` 计算而非 `strlen`，反而更适合 `string_view`）。
7. **阶段 4/5 会改动 `test/Makefile.am`**，属于构建系统文件；前 10 轮一直声明「不动构建系统」，本轮因删测试源而必须改 —— 这是用户批准的范围内改动，需在文档注明。

## 备注

- 用户决策（轮次 11）：A+B+C+D 全做；`-Wextra` 全部清零（删形参名，不用 `[[maybe_unused]]`）；CI 只加 `--enable-werror`。
- **审查 #1**：初稿把 `-Woverloaded-virtual`（`src/DiskWriter.h:54,68`、`src/DHTConnection.h:48,51`）与 `src/help_tags.h:42` 的 `elaborated-type-specifier` 列为真实警告 —— 带 `-DHAVE_CONFIG_H` 重扫后 0 命中，是早期不带该宏的假象，已从阶段 2 清单删除。
- **审查 #2**：`src/aria2api.cc` 的 10 条 error 是 `ENABLE_LIBARIA2` 未定义的探针假象（本机不编译该 TU），不属于本轮范围。
- **审查 #3**：`src/RpcMethodImpl.h` 的 34 处 `const char*` 是 `getMethodName()` 的**返回类型**，不是形参，已从 D 组 71 处里排除（早前粗算 142 处含了它们）。
- **审查 #4**：`-Wredundant-move` 实测 38 条 but 其中 `src/RpcMethodImpl.h:361` 是同一个模板行被两个实例化各报一次，去重后 46 个站点里算 1 个。
- **审查 #5**：`src/InternalDHKeyExchange.cc` 的 VLA 警告所在 TU 本机不编译（`config.h:438 /* #undef USE_INTERNAL_BIGNUM */`），阶段 2 只能语法自查，需在「实际偏差」登记。
- **审查 #6**：test 侧扫描跑完后数字从「34 条 / 21 文件」校正为「1852 条 / 22 文件」，其中 `-Wunused-parameter` 去重 151 站点；`test/MockPieceStorage.h` 一个文件就占 600 条（22 个站点）。同时发现 `test/IndexedListTest.cc` 有 12 条 `-Wdeprecated-copy`，归入阶段 5 一并消失，不列入阶段 2。
- **审查 #7**：`array_fun.h` 的 include 方是 **70 个文件**（初稿写 37），真正使用符号的只有 8 个 → 死 include **62 个**（初稿写 36）；`arraySize` 用户是 5 个 test 文件（`test/array_funTest.cc` 里的 2 处在删测试文件时一并消失）。
