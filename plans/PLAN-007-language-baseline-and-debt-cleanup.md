# 任务：语言基线升级到 C++23 + 三类剩余债清理

> 状态：⏳ 待执行（计划已确认，等待「开始执行」）

## 目标

PLAN-003~006 已把协议层、引擎层、配置/RPC/CLI 层的主干现代化做完（`typedef`→`using`、`enum class`、`std::span`、`std::from_chars`、`std::ranges`、`std::format`）。本计划处理剩下的三类结构性债：

1. **语言基线**：C++20 → C++23（`configure.ac` + `m4` 宏），解锁 `std::expected`、`std::ispanstream`、`std::to_underlying`、`std::byteswap`。
2. **`.c_str()` 冗余**：只清 `fmt(...)` 实参里多余的 `.c_str()`。
3. **`bitfield.h` span 化**：`countSetBit` / `flipBit` 两个非模板函数收 `std::span`。
4. **`bool` + 引用出参 → `std::optional`**：只改真正的「输出一个值」函数。
5. 附带：5 处 `shortcut:` 标注的 `std::istream&` 链路改 `std::string_view`（阶段 1 切 C++23 后不再需要 `std::ispanstream`）。

## 当前上下文

### 仓库状态

- HEAD `0bfa7120`（PLAN-006 收尾 + libuv 补验），分支 `slim-features`，`git status --porcelain` 为空。
- `plans/` 已有 PLAN-001~006，本计划是 PLAN-007。
- `configure.ac:111` 当前 `AX_CXX_COMPILE_STDCXX([20], [noext], [mandatory])`。
- `--enable-werror` 实际定义在 `configure.ac:555-561` → `WARNCXXFLAGS = -Wall -Werror -Wformat-security`。
- 工具链：g++ 14.2.0、autoconf 2.72、automake 1.17、`nproc` = 24。**本机无 clang++**。

### C++23 可行性（已实测）

- 干净 worktree `/tmp/aria2-c23`（base `0bfa7120`）+ `make -j24 CXX='g++ -std=c++23'`：全量重编 **0 error / 0 warning**，`src/aria2c` 正常生成。
- `make -j24 check CXX='g++ -std=c++23'`：`# TOTAL: 1` / `# PASS: 1` / `# FAIL: 0` / `# ERROR: 0`，`test/aria2c.log` `OK (914)`。
- g++ 14 探针（`-std=c++23 -Wall -Wextra -Werror -Wformat-security`）通过：`std::expected<int,std::string>`、`std::ispanstream{std::span}` + `std::getline`、`std::to_underlying(Enum::A)`（必须传真枚举）、`std::views::split`、`std::print`、`std::byteswap`。
- **`std::ranges::contains` 在 g++14 缺失**（`__cpp_lib_ranges_contains` 有定义但符号不存在）——本计划不使用，已有代码 0 处使用。
- `m4/ax_cxx_compile_stdcxx.m4` 的 `AC_DEFUN` 只接受 11/14/17/20（`:52-56` 的 `m4_if`，传 23 会 `m4_fatal`），`:182-189` 最后一档是 `testbody_20`，`:995-1016` 是 `new_in_20`。**只改 `configure.ac` 会直接失败，必须同时改 m4 宏。**

### 债务实测（剥注释 + 括号配平脚本）

| 项 | 数量 | 说明 |
|---|---|---|
| `fmt(...)` 内含 `.c_str()` | **286 个调用 / 333 处 / 58 文件** | 纯机械；top `src/SocketCore.cc` 42、`src/RequestGroup.cc` 28、`src/UDPTrackerClient.cc` 23、`src/AbstractCommand.cc` 21、`src/AbstractDiskWriter.cc` 21 |
| `.c_str()` 全量 | 约 507 处 / 94 文件 | 多数传给 C API（`getopt_long`/`BufferedFile`/`sockaddr`/`strerror`），**不动** |
| `bitfield.h` 非模板裸指针函数 | 2 个 | `countSetBit(const unsigned char*, size_t)`、`flipBit(unsigned char*, size_t, size_t)` |
| `bitfield.h` 模板函数 | 4 个，**已接受 `std::vector`/表达式模板** | `test`/`countSetBitSlow`/`getFirstSetBitIndex`/`getFirstNSetBitIndex` —— 零改动 |
| `bool` + 引用出参 | 28 个声明，其中**真出参 9 个** | 其余 17 个是可写引用（`Exception&`/`IOFile&`/`BufferedFile&`/累加 vector） |
| `std::istream&` 链路 | 5 处 `shortcut:` | `OptionParser::parse`、`OptionParser::parseArg`、`util::createIndexPaths` |

### 已确认决策（4 项，用户回答）

| 决策点 | 结论 |
|---|---|
| 覆盖范围 | **四项全做 + 5 阶段**：C++23 基线 / `.c_str()` / bitfield span / optional 出参 / 收尾 |
| C++23 | **纳入，作为阶段 1 独立提交** |
| `bool`+出参形态 | **`std::optional`** |
| span 边界 | **只改 `bitfield.h` 两个非模板函数**；`getBitfield()`/`getFilterBitfield()`/`PieceStorage` 纯虚族保持 `const unsigned char*` |

### 待确认决策

无。

## 提议的方法

### 提交顺序

自底向上，每个阶段一次提交 + 一次干净 worktree 门禁：

1. **阶段 1：C++23 基线**（m4 宏 + `configure.ac`）—— 编译系统改动，必须最先，且单独可回滚。
2. **阶段 2：`.c_str()` 清理 + `std::string_view` 链路**（286 处机械替换 + 3 个签名）。
3. **阶段 3：`bitfield.h` span 化**（2 个函数 + 12 个调用点）。
4. **阶段 4：`bool`+出参 → `std::optional`**（9 个函数声明 + 12 个 override + 调用点 + 测试）。
5. **阶段 5：收尾**（勾选、状态行、实测门禁表、实际偏差、债务前后对比）。

阶段 3 与 4 都动 `src/DefaultPieceStorage.cc` 与 `src/Piece.h`，但改的是不同函数（3: `flipBit`/`countSetBit`；4: `select`/`getMissingUnusedBlockIndex`），可以分两次提交而不留半成品；若阶段 3 后 `make check` 因 `select` 未改而失败，则合并 3、4 为一阶段（见「风险与权衡」）。

### 阶段 1 细节：C++23 基线

- `m4/ax_cxx_compile_stdcxx.m4:52-56` 追加一档：
  ```
  [$1], [20], [ax_cxx_compile_alternatives="20"],
  [$1], [23], [ax_cxx_compile_alternatives="23 2b"],
  ```
- 追加 `_AX_CXX_COMPILE_STDCXX_testbody_23`（在 `:182-189` 的 `testbody_20` 之后），内部依次引用 `testbody_new_in_11/14/17/20/23`。
- 追加 `_AX_CXX_COMPILE_STDCXX_testbody_new_in_23`，**只测语言标准，不测库特性**：
  ```
  #elif __cplusplus < 202302L && !defined _MSC_VER
  #error "This is not a C++23 compiler"
  #else
  #include <version>
  #endif
  ```
  理由：`std::expected` 在 clang-18 的 libstdc++/libc++ 组合下支持度不一，写 `__cpp_lib_expected` 断言可能误杀 CI；语言标准就够了，库特性由阶段 2~4 的实际使用在 g++14 门禁里验证。
- `configure.ac:111` → `AX_CXX_COMPILE_STDCXX([23], [noext], [mandatory])`。
- 验证：
  ```
  autoreconf -i
  ./configure --enable-werror
  grep -c 'std=c++23' config.log   # ≥1，确认探测用 -std=c++23
  grep '^CXX = ' Makefile           # 含 -std=c++23
  make -j24 check
  ```

### 阶段 2 细节：`.c_str()` 清理 + `std::string_view`

**2a：机械替换（333 处）**

Python 脚本判定规则（必须精确到深度，避免误删传给 C API 的）：

1. 定位每个 `fmt(`（排除 `fmt.h` 内的定义、`fmtRuntime(`）。
2. 用括号配平算出实参区间。
3. 在区间内找 `.c_str()`；**只有当它后面（跳过空白）紧跟的字符是**
   - `,` 且当前括号深度 == `fmt` 实参深度（即该逗号是 fmt 的实参分隔符），或
   - `)` 且深度 == `fmt` 深度 − 1（即该 `)` 闭合 fmt 调用）
   才删除。
4. `fmt(...)` 内嵌套的 `strstr(a.c_str(), b.c_str())`、`sockaddr`、`getopt_long` 等 C API 实参**保持不动**（它们的 `.c_str()` 后面跟的是嵌套调用的 `,`/`)`，深度不匹配）。

理由：`std::format` 的 `{}` 对 `std::string` 与 `const char*` 输出完全一致，`.c_str()` 在这里纯冗余。

**2b：`std::istream&` 链路改 `std::string_view`**（5 处 `shortcut:` → 0）

| 函数 | 现签名 | 新签名 | 调用点 |
|---|---|---|---|
| `OptionParser::parse` | `parse(Option&, std::istream& ios)` | `parse(Option&, std::string_view text)` | `src/option_processing.cc:191`(经 parseArg 缓冲区)、`:232`、`src/UriListParser.cc:99`、`test/OptionParserTest.cc:201` |
| `OptionParser::parseArg` | `parseArg(std::ostream& out, ...)` | `std::string parseArg(std::vector<std::string>& nonopts, int argc, char* argv[])`（返回值代替 `out <<`） | `src/option_processing.cc:191`、`test/OptionParserTest.cc:181` |
| `util::createIndexPaths` | `createIndexPaths(std::istream& i)` | `createIndexPaths(std::string_view text)` | `src/download_helper.cc:219`、`src/RpcMethodImpl.cc:1494`、`test/UtilTest2.cc:644` |

`parse` 的实现改成按行遍历 `std::string_view`（`util::split`/手写 find，行为与 `getline(is, line)` 等价：跳过空行与 `#` 开头行，按第一个 `=` 切分）。

`parseArg` 返回值替代 `std::ostream& out` 后，`src/option_processing.cc:191` 的 `std::stringstream cmdstream` 消失，改成 `auto cmdtext = oparser->parseArg(uris, argc, argv); oparser->parse(op, cmdtext);`。

`src/option_processing.cc:229-232` 读配置文件：`BufferedFile fp(...)` 需要读全量——用已有的 `IOFile::read(std::span<std::byte>)` 循环读到 `std::string`（`transfer(std::ostream&)` 保留，但这里不再用），或加一个 `IOFile::readAll()` 小辅助。倾向后者：`std::string IOFile::readAll()`（4 行，`src/IOFile.cc`），顺带让 `src/LibsslTLSContext.cc:201` 的 `transfer(ss)` 也能简化——但那个文件不在本阶段范围，留 `transfer` 不动。

`src/UriListParser.cc:73` 的 `std::stringstream ss` → `std::string opts; opts += line_; opts += '\n';`，末尾 `optparser->parse(op, opts);`。

### 阶段 3 细节：`bitfield.h` span 化

`src/bitfield.h`：

```cpp
// 现在
inline size_t countSetBit(const unsigned char* bitfield, size_t nbits);
void flipBit(unsigned char* data, size_t length, size_t bitIndex);
// 改为
inline size_t countSetBit(std::span<const unsigned char> bitfield, size_t nbits);
void flipBit(std::span<unsigned char> data, size_t bitIndex);
```

- **保留 `nbits` 形参**：`nbits` 是位数量，不一定是 `bitfield.size() * 8`（调用点传 `blocks_`，最后一个字节只用高位）。改成从 span 推会在 `nbits % 8 != 0` 时算错。
- `flipBit` 的 `length` 完全等价于 `data.size()`，删掉形参，内部 `assert(byteIndex < data.size())` 比原来的 `<=` 更严格（原 `<=` 允许越界写入 `data[length]`）。
- `src/bitfield.h` 新增 `#include <span>`。

调用点（全部实测）：

| 文件:行 | 改法 |
|---|---|
| `src/BitfieldMan.cc:490,491,495,502,732` | `bitfield::countSetBit(bitfield_.data(), blocks_)` → `bitfield::countSetBit(bitfield_, blocks_)` |
| `src/BitfieldMan.cc:777,781` | 函数指针 `&bitfield::countSetBitSlow<decltype(arr)>` / `&bitfield::countSetBit` 传给 `computeCompletedLength(..., CountFun)` —— `countSetBitSlow` 是模板保持；`&bitfield::countSetBit` 的新签名与 `CountFun` 调用形态 `cntfun(bitfield, nbits)` 兼容（`Array` 是 `unsigned char*`，需改成 `std::span{...}` 或在 `computeCompletedLength` 里加 `std::span` 包装） |
| `src/DHTBucket.cc:181,184` | `flipBit(rMax.data(), rMax.size(), prefixLength_)` → `flipBit(rMax, prefixLength_)`（`DHTID` 是 `std::array<unsigned char, 20>`，隐式转 `std::span<unsigned char>`） |
| `src/DefaultPieceStorage.cc:241` | `flipBit(misbitfield.get(), blocks, index)` → `flipBit(std::span{misbitfield.get(), blocks}, index)` |
| `src/bitfield.cc:41` | 定义同步删 `length` 形参 |
| `test/BitfieldManTest.cc:674,680,693,711,720,737,747` | `countSetBit(misbitfield, nbits)` → `countSetBit(std::span{misbitfield, nbits}, nbits)`（`misbitfield` 是 `unique_ptr<unsigned char[]>`） |
| `test/BitfieldManTest.cc:869` | `flipBit(ignoreBitfield, length, i)` → `flipBit(std::span{ignoreBitfield, length}, i)` |
| `test/bitfieldTest.cc:46,49,52,55,58` | `countSetBit(bitfield, N)` → `countSetBit(std::span{bitfield, N}, N)`（`bitfield` 是 `unsigned char bitfield[8]`） |

**明确不改**：`src/BitfieldMan.h:215 getBitfield()`、`:286 getFilterBitfield()`、`src/BtBitfieldMessage.h:62`、`src/Peer.h:203`/`src/Peer.cc:239`、`src/PeerSessionResource.h:136`、`src/Piece.h:130`、`src/PieceStorage.h:209` 的纯虚、`src/DefaultPieceStorage.h:218`、`src/UnknownLengthPieceStorage.h:198`、`test/MockPieceStorage.h:187` 及 34 个消费点。`src/includes/aria2/aria2.h:749 virtual std::string getBitfield()` 是 `BtProgressInfoFile` 的同名不同类，也不动。

### 阶段 4 细节：`bool` + 出参 → `std::optional`

只改这 9 个声明（12 个 override 随同）：

| # | 现签名 | 新签名 | 实现文件 | 调用点 |
|---|---|---|---|---|
| 1 | `src/uri.h:73 bool parse(UriStruct& result, const std::string& uri)` | `std::optional<UriStruct> parse(const std::string& uri)` | `src/uri.cc` | `src/AbstractCommand.cc:629`、`src/OptionHandlerImpl.cc:513`、`src/Request.cc:153` |
| 2 | `src/Piece.h:89 bool getMissingUnusedBlockIndex(size_t& index) const` | `std::optional<size_t> getMissingUnusedBlockIndex() const` | `src/Piece.cc` | `test/DefaultBtMessageDispatcherTest.cc:213,233,305`（注释明写 only used by unit tests） |
| 3 | `src/Piece.h:98 bool getFirstMissingBlockIndexWithoutLock(size_t& index) const` | `std::optional<size_t> getFirstMissingBlockIndexWithoutLock() const` | `src/Piece.cc` | `src/PiecedSegment.cc:46` |
| 4 | `src/PieceSelector.h:48 bool select(size_t& index, const unsigned char* bitfield, size_t nbits) const` | `std::optional<size_t> select(const unsigned char* bitfield, size_t nbits) const` | 3 个 override：`src/LongestSequencePieceSelector.cc:65`、`src/PriorityPieceSelector.cc:46`、`src/RarestPieceSelector.cc:51`；2 个 test fake：`test/MockPieceSelector.h:10`、`test/InorderPieceSelector.h:11` | grep 后逐个 |
| 5 | `src/StreamPieceSelector.h:61 bool select(size_t& index, size_t minSplitSize, const unsigned char* ignoreBitfield, size_t length)` | `std::optional<size_t> select(size_t minSplitSize, const unsigned char* ignoreBitfield, size_t length)` | 4 个 override：`Default`/`Geom`/`Inorder`/`Random` 的 `.cc:47/48/47/48` | `src/DefaultPieceStorage.cc:239,391` |
| 6 | `src/DHTConnectionImpl.h:66 bool bind(uint16_t& port, const std::string& addr, SegList<int>& sgl)` | `std::optional<uint16_t> bind(const std::string& addr, SegList<int>& sgl)` | `src/DHTConnectionImpl.cc` | `src/DHTSetup.cc:130` |
| 7 | `src/DHTConnectionImpl.h:78 bool bind(uint16_t& port, const std::string& addr)` | `std::optional<uint16_t> bind(const std::string& addr)` | 同上 | `src/DHTSetup.cc:133`（`#7` 由 `#6` 内部调用） |
| 8 | `src/PeerListenCommand.h:66 bool bindPort(uint16_t& port, SegList<int>& seq)` | `std::optional<uint16_t> bindPort(SegList<int>& seq)` | `src/PeerListenCommand.cc:62` | `src/BtSetup.cc:197,203` |
| 9 | `src/File.h:78 bool exists(std::string& err)` | `std::optional<std::string> existsError()`（有值 = 不存在/取状态失败的原因，`nullopt` = 存在） | `src/File.cc:82` | `src/OptionHandlerImpl.cc:549` |

**`#9` 的命名说明**：`std::optional` 无法同时表达「成功/失败 + 错误消息」两个状态，用「有值即错误」的 `existsError()` 保持零分配（`std::string` 只在失败时构造，与现状一致）。`File::exists()`（无参版本，`src/File.h:71`）保持不变。`test/FileTest.cc:50-95` 的既有断言继续走无参版；新增一条 `existsError()` 断言（存在的文件 → `nullopt`；不存在的文件 → 有值且非空）。

**`#1` 的实现注意**：`src/uri.cc` 的 `parse` 现在是「先 `result = UriStruct()`，逐字段填，失败返回 false」。改 `std::optional` 后内部构造局部 `UriStruct result;`，失败 `return std::nullopt;`，成功 `return result;`。`UriStruct` 有自定义拷贝 ctor 与析构（`src/uri.h:60-62`），`std::optional<UriStruct>` 要求可拷贝 —— 已有拷贝 ctor，满足。

**`#4` 的 `bitfield` 形参**：阶段 3 只改了 `bitfield::countSetBit`/`flipBit`，`select` 的 `const unsigned char* bitfield` 形参按用户决策保持不变（`BitfieldMan::getBitfield()` 家族仍是裸指针）。`std::span` 化留给后续计划。

### 阶段 5 细节：收尾

- `plans/PLAN-007-*.md` 状态行、5 个阶段勾选、`## 实测门禁结果` 表格（提交 / worktree / 结果 / ABI 哈希）、`## 实际偏差`、债务前后对比表（333→0、bitfield 2→0、出参 9→0、`shortcut:` 5→0）。
- `MEMORY.md` 追加 PLAN-007 条目。
- 清理 `/tmp/aria2-m7-p*` worktree 与 `git worktree prune`。

## 分步计划

1. [x] **阶段 1：C++23 语言基线** — 改 `m4/ax_cxx_compile_stdcxx.m4`（3 处）+ `configure.ac:111`；干净 worktree 门禁；提交 `PLAN-007 阶段1: 升级语言基线到 C++23`。

   **实测（2026-11-08）**：`m4/ax_cxx_compile_stdcxx.m4` 加 `[$1],[23],[ax_cxx_compile_alternatives="23 2b"]`、`_AX_CXX_COMPILE_STDCXX_testbody_23`（引用 new_in_11/14/17/20/23）、`_AX_CXX_COMPILE_STDCXX_testbody_new_in_23`（只断言 `__cplusplus < 202302L`，不测库特性）；`configure.ac:111` → `AX_CXX_COMPILE_STDCXX([23], [noext], [mandatory])`。干净 worktree `/tmp/aria2-m7-p1`（base `0bfa7120`）：`autoreconf -i` rc=0、`./configure --enable-werror` rc=0、`CXX = g++ -std=c++23`、`config.log` 中 `std=c++20` 0 命中、`make -j24 check` rc=0、`# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`test/aria2c.log` `OK (914)`、`grep -c 'error:'` = 0、ABI 哈希 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。提交 `5e032fc7`。
2. [x] **阶段 2：`.c_str()` 清理 + `std::string_view` 链路** — 333 处机械替换 + `OptionParser::parse`/`parseArg`/`util::createIndexPaths` 三个签名 + `IOFile::readAll()`；干净 worktree 门禁；提交 `PLAN-007 阶段2: 清理 fmt 冗余 .c_str() 并移除 istream 链路`。

   **实测（2026-11-08）**：括号配平脚本（`fmt_call_spans` + `redundant_positions`：仅删「其后紧跟深度 1 的 `,` 或 `)`」的 `.c_str()`，避开 `strstr(a.c_str(), b.c_str())` 这类嵌套 C API 实参）删掉 **333 处 / 58 文件**，`git grep` 复算残留 0。`OptionParser::parseArg(std::ostream&, ...)` → `std::string parseArg(std::vector<std::string>&, int, char*[]) const`（调用方 `src/option_processing.cc` 直接收返回值）；`OptionParser::parse(Option&, std::istream&)` → `parse(Option&, std::string_view)`，循环改用新增的 `util::getline(std::string_view&, std::string&)`（inline，`src/util.h`）；`util::createIndexPaths(std::istream&)` → `(std::string_view)`；新增 `IOFile::readAll()`（`src/IOFile.h`/`.cc`）替掉 `fp.transfer(ss)`；`src/option_processing.cc` 删掉 `cmdstream`/`seekg`/`clear()` 三段缓冲逻辑与 `#include <sstream>`；`src/UriListParser.cc` 的 `std::stringstream ss` → `std::string opts`；`src/download_helper.cc`/`src/RpcMethodImpl.cc` 的 `std::istringstream` → 直接传 `string_view` 并删 `<sstream>`。测试：`test/OptionParserTest.cc` 的 `parseArg(s, ...)`+`s.str()` → `auto s = parseArg(...)`、`std::istringstream in` → `std::string in`；`test/UtilTest2.cc:644` → `std::string_view`。踩坑：第一次编译 `src/option_processing.cc:231` 报 `cannot convert 'std::string' to 'std::ostream&'`（`fp.transfer(conf)` 里 `transfer` 只收 `std::ostream&`），加 `IOFile::readAll()` 后通过。

   干净 worktree `/tmp/aria2-m7-p2`（base `5e032fc7`）：`autoreconf -i` rc=0、`./configure --enable-werror` rc=0、`CXX = g++ -std=c++23`、`make -j24 check` rc=0、`# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`test/aria2c.log` `OK (914)`、`grep -c 'error:'` = 0、ABI 哈希不变。提交 `135b6392`（69 文件，+607/−663）。

   **可解释残余**：`stringstream` 从 16 处降到 **10 处 / 6 文件**（`src/ByteArrayDiskWriter.h` 2、`src/ConsoleStatCalc.cc` 1、`src/LibsslTLSContext.cc` 1、`src/RequestGroupMan.cc` 3、`src/RpcResponse.cc` 2、`src/json.cc` 1）—— 全部喂 `std::ostream`/`std::istream` 以外的流语义（`buf_.seekp/seekg/gcount`、`std::setw`/`std::setfill` 格式化、`GZipEncoder` 共享 `operator<<`），换 `std::format` 要改内部函数签名且收益为零，按 `shortcut:` 保留。
3. [ ] **阶段 3：`bitfield.h` span 化** — `countSetBit`/`flipBit` 两个函数 + 16 个调用点；干净 worktree 门禁；提交 `PLAN-007 阶段3: bitfield 算法族 span 化`。
4. [ ] **阶段 4：`bool`+出参 → `std::optional`** — 9 个声明 + 12 个 override + 调用点 + 测试；干净 worktree 门禁；提交 `PLAN-007 阶段4: 输出参数改为 std::optional 返回值`。
5. [ ] **阶段 5：收尾** — 勾选、状态行、实测门禁表、实际偏差、债务前后对比；提交 `PLAN-007: 勾选全部阶段、标记计划完成并记录实测门禁结果`。

## 预计修改的文件

**阶段 1（2 个）**
- `m4/ax_cxx_compile_stdcxx.m4`
- `configure.ac`

**阶段 2（62 个左右）**
- 58 个含 `fmt(...c_str())` 的文件（清单见 `git grep`，top 见上文债务表）
- `src/OptionParser.h`、`src/OptionParser.cc`
- `src/util.h`、`src/util.cc`（`createIndexPaths`）
- `src/IOFile.h`、`src/IOFile.cc`（新增 `readAll()`）
- `src/option_processing.cc`、`src/UriListParser.cc`、`src/download_helper.cc`、`src/RpcMethodImpl.cc`
- `test/OptionParserTest.cc`、`test/UtilTest2.cc`、`test/UriListParserTest.cc`

**阶段 3（8 个）**
- `src/bitfield.h`、`src/bitfield.cc`
- `src/BitfieldMan.cc`
- `src/DHTBucket.cc`
- `src/DefaultPieceStorage.cc`
- `test/BitfieldManTest.cc`、`test/bitfieldTest.cc`

**阶段 4（20 个左右）**
- `src/uri.h`、`src/uri.cc`、`src/AbstractCommand.cc`、`src/OptionHandlerImpl.cc`、`src/Request.cc`
- `src/Piece.h`、`src/Piece.cc`、`src/PiecedSegment.cc`
- `src/PieceSelector.h`、`src/LongestSequencePieceSelector.{h,cc}`、`src/PriorityPieceSelector.{h,cc}`、`src/RarestPieceSelector.{h,cc}`
- `src/StreamPieceSelector.h`、`src/DefaultStreamPieceSelector.{h,cc}`、`src/GeomStreamPieceSelector.{h,cc}`、`src/InorderStreamPieceSelector.{h,cc}`、`src/RandomStreamPieceSelector.{h,cc}`
- `src/DHTConnectionImpl.{h,cc}`、`src/DHTSetup.cc`
- `src/PeerListenCommand.{h,cc}`、`src/BtSetup.cc`
- `src/File.h`、`src/File.cc`、`test/FileTest.cc`
- `test/MockPieceSelector.h`、`test/InorderPieceSelector.h`、`test/DefaultBtMessageDispatcherTest.cc`
- `src/DefaultPieceStorage.cc`（`select` 调用点）

**阶段 5（1 个）**
- `plans/PLAN-007-language-baseline-and-debt-cleanup.md`

## 测试与验证

每个阶段的统一门禁（**必须干净 worktree，原地增量 `make` 会假绿**）：

```bash
git worktree prune
git worktree add --detach /tmp/aria2-m7-p<N> HEAD
cd /tmp/aria2-m7-p<N>
git apply /tmp/p7-stage<N>.patch        # patch 用 git diff 生成
autoreconf -i
./configure --enable-werror
nohup make -j24 check > /tmp/m7-p<N>.log 2>&1 &   # make check 单次 > 60s，用 bg + 轮询
```

通过标准（全部满足才算绿）：

| 检查 | 期望 |
|---|---|
| `make check` 退出码 | 0 |
| `# TOTAL:` / `# PASS:` | `1` / `1`（顶层 summary） |
| `# FAIL:` / `# ERROR:` | `0` / `0` |
| `test/aria2c.log` | 末行 `OK (914)` |
| `grep -c 'error:'`（build log） | `0` |
| `git hash-object src/includes/aria2/aria2.h` | `2216641161b15acb3c3ddf2600dc19e3decd59d5` |
| `grep '^CXX = ' Makefile` | 阶段 1 起含 `-std=c++23` |

每阶段专项验证：

- **阶段 1**：`grep -c 'std=c++23' config.log` ≥ 1；`grep -c 'std=c++20' config.log` = 0（确认没回退）。**假绿风险**：`make` 若复用旧 `.o` 会绕过新标准 → 干净 worktree 已规避。
- **阶段 2**：重跑括号配平计数脚本，`fmt(...)` 内 `.c_str()` = 0；`git grep -n 'shortcut: .*std::istream'` = 0；`git grep -c 'stringstream' -- src` 从 16 降到 10（消除 `src/UriListParser.cc` 2、`src/download_helper.cc` 1、`src/RpcMethodImpl.cc` 1、`src/option_processing.cc` 2；保留 `src/ByteArrayDiskWriter.h` 2、`src/ConsoleStatCalc.cc` 1、`src/LibsslTLSContext.cc` 1、`src/RequestGroupMan.cc` 3、`src/RpcResponse.cc` 2、`src/json.cc` 1，它们喂 `std::ostream`/`istream` 之外的流语义）。`test/OptionParserTest.cc` 与 `test/UriListParserTest.cc` 全绿证明行为等价。**假绿风险**：`.c_str()` 删多了会让某处 `strstr(a.c_str(), ...)` 变成 `strstr(a, ...)` → 编译失败，`-Werror` 会挡住。
- **阶段 3**：`git grep -n 'countSetBit(\|flipBit('` 逐个核对无裸指针实参；`test/bitfieldTest.cc` 与 `test/BitfieldManTest.cc` 全绿（`countSetBitSlow` 与 `countSetBit` 的双实现对照断言是强校验）。**假绿风险**：`flipBit` 的 `length` 由 `data.size()` 推出，若某调用点传的 span 尺寸与原 `length` 不一致会静默越界 → 调用点已逐个列出并核对。
- **阶段 4**：`git grep -nE 'bool \w+\([^)]*(size_t|uint16_t|std::string)&'` 在目标文件 = 0；`git grep -n 'getMissingUnusedBlockIndex\|getFirstMissingBlockIndexWithoutLock\|::select(\|bindPort('` 全部按新签名；`test/DefaultBtMessageDispatcherTest.cc`、`test/GeomStreamPieceSelectorTest.cc`、`test/LongestSequencePieceSelectorTest.cc`、`test/PriorityPieceSelectorTest.cc`、`test/RarestPieceSelectorTest.cc`、`test/FileTest.cc` 全绿。**假绿风险**：`optional<size_t>` 隐式转 `bool` 仍能写 `if (select(...))`，只看 `if` 会漏掉「值被丢弃」—— 必须读每次调用的返回使用方式。

## 验收标准

1. `configure.ac:111` 为 `AX_CXX_COMPILE_STDCXX([23], ...)`，`autoreconf -i && ./configure --enable-werror` 成功，`Makefile` 的 `CXX` 含 `-std=c++23`。
2. `fmt(...)` 实参内的 `.c_str()` 为 **0 处**（脚本复核）。
3. `bitfield.h` 中 `countSetBit`/`flipBit` 无裸指针形参；`getBitfield()` 家族签名未变（`git diff src/PieceStorage.h src/includes/aria2/aria2.h` 为空）。
4. 9 个 `bool`+出参函数全部改为 `std::optional` 返回；17 处可写引用保持不变。
5. `git grep -n 'shortcut: .*std::istream'` = 0。
6. 4 次代码提交 + 1 次文档收尾，每次都有干净 worktree 门禁记录（`OK (914)`、`# TOTAL: 1`/`# PASS: 1`、ABI 哈希不变）。
7. `src/includes/aria2/aria2.h` 哈希保持 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。
8. `configure.ac` 只改第 111 行的标准参数；`.github/workflows/build.yml`、`src/Makefile.am`、`test/Makefile.am`、7 个 `src/*.c` shim、`README.rst` 不动。

## 风险与权衡

| 风险 | 影响 | 缓解 |
|---|---|---|
| CI 用 clang-18，本机无 clang++ | C++23 在 clang-18 + libstdc++-14 下的行为未本地验证 | `testbody_23` 只断言 `__cplusplus >= 202302L`，不测库特性；阶段 2~4 用到的 `std::expected` 等都在 g++14 门禁覆盖；CI 首跑若失败，最小回退是把阶段 1 的 `[23]` 改回 `[20]`（阶段 2~4 不依赖 C++23 特性，除了 `IOFile::readAll()` 不用新特性） |
| 阶段 2 的自动替换误删 C API 的 `.c_str()` | 编译错误或静默行为变化 | 规则基于括号深度（只有 fmt 实参分隔符/闭合括号前的才删）；`-Werror` 挡住类型错误；`test/` 914 例覆盖 |
| `OptionParser::parse` 改 `std::string_view` 后与 `getline` 行为不一致（如 `\r\n`） | 配置文件解析差异 | `getline(is, line)` 以 `\n` 分隔且保留 `\r`；新实现同样只在 `\n` 处切分并保留 `\r`，与现状一致；`test/OptionParserTest.cc:201` 用 `\n` 分隔 |
| 阶段 3 与 4 都改 `src/DefaultPieceStorage.cc` | 阶段 3 单独提交时若 `select` 未改，`flipBit` 的 span 化仍可编译（`select` 形参未动） | 两个阶段改的是不同函数，互不阻塞；若出现耦合，合并 3、4 为一阶段并在文档记「审查 #N」 |
| `flipBit` 删掉 `length` 形参后 `assert` 变严格 | 原代码若真有 `bitIndex/8 == data.size()` 的越界调用，会在 debug 下 abort | 16 个调用点已逐个核对，`byteIndex` 均 `< length`；`test/bitfieldTest.cc` 覆盖边界 |
| `File::exists(std::string&) → existsError()` 语义反转 | 调用点写错会把「存在」当错误 | 唯一调用点 `src/OptionHandlerImpl.cc:549` 一处 + `test/FileTest.cc` 新增断言 |
| `uri::parse` 返回 `std::optional<UriStruct>` 需要 `UriStruct` 可拷贝 | 编译错误 | `src/uri.h:60-62` 有显式拷贝 ctor，满足 |

## 自查

1. **引用真实**：上表所有 `文件:行` 均已用 `grep -n`/`sed -n` 逐条核对（`src/bitfield.h:100,145`、`src/bitfield.cc:41`、`src/BitfieldMan.cc:490,491,495,502,732,777,781`、`src/DHTBucket.cc:181,184`、`src/DefaultPieceStorage.cc:239,241,391`、`src/uri.h:73`、`src/Piece.h:89,98`、`src/PieceSelector.h:48`、`src/StreamPieceSelector.h:61`、`src/DHTConnectionImpl.h:66,78`、`src/PeerListenCommand.h:66`、`src/File.h:71,78`、`src/File.cc:82`、`src/OptionParser.h:69,72`、`src/util.h:475`、`src/IOFile.h:82`、`m4/ax_cxx_compile_stdcxx.m4:52-56,182-189,995-1016`、`configure.ac:111`；`getBitfield()` 家族 11 处声明（`src/BtBitfieldMessage.h:62`、`src/Peer.h:203`、`src/PeerSessionResource.h:136`、`src/Piece.h:130`、`src/PieceStorage.h:209`、`src/DefaultPieceStorage.h:218`、`src/UnknownLengthPieceStorage.h:198`、`src/BitfieldMan.h:215`、`test/MockPieceStorage.h:187`）逐个确认存在且不改）。「预计修改的文件」全部 `git ls-files` 确认存在。
2. **阶段可编译、测试不假绿**：9 个出参函数的全部调用点（含 `test/`）已 grep 列出并在阶段内闭环；每阶段跑干净 worktree 全量重编译（原地增量会假绿，PLAN-004 阶段 4 已实测踩到）；阶段 2 的脚本规则基于括号深度而非单行正则，避免上一轮「167 vs 333」的计数误差重演。
3. **覆盖所有分支与消费点**：`getBitfield()` 家族 34 个消费点确认**不改**（用户决策）；`bitfield.h` 4 个模板函数确认**已接受容器**（`test/bitfieldTest.cc:47` 就在用 `countSetBitSlow(array)`）；`A2_ERROR` 与 `Logger::LEVEL` 的重名在 PLAN-006 已解决，本计划不涉及；7 个 `src/*.c` shim 与 `src/includes/aria2/aria2.h` 确认不在范围。

## 备注

- 用户授权：「开始执行」尚未给出，本阶段只写计划文档。
- 上一轮建议顺序（`.c_str()` → bitfield → 出参）被调整为「C++23 先行」，理由是 C++23 是编译系统改动，越晚做越会与后续阶段的二进制/`--enable-werror` 行为混淆，且它单独可回滚。
- 上一轮给的「167 处 `.c_str()`」是单行正则低估，本计划实测为 286 个调用 / 333 处（括号配平脚本，已复核 `src/SocketCore.cc` 42、`src/RequestGroup.cc` 28 等 top 文件）。
- 上一轮把 `bitfield.h` 描述为「整族 span 化」不准确：4 个模板函数本来就接受 `std::vector`/表达式模板，实际只需改 2 个非模板函数。
- 「待确认决策」4 项已由用户选定，故本章节记为「无」。
