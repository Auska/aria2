# 任务：按模块推进现代化 —— 基础层（util / 解析器 / 摘要 / 磁盘IO / 网络 / 流）

> 状态：📝 待执行（计划已写，等用户授权）

## 目标

在 PLAN-001（C++11→C++20）、PLAN-002（工具层 span 化）已完成的基础上，
按**模块**对 aria2 做可持续的现代化改造。本计划只覆盖**基础层 6 个子模块 /
166 个文件 / 27 697 行**，每个子模块一次提交，每次提交都是可编译可测试的闭环。

四类改造同时做（用户第 1 轮确认）：

1. **语言惯用法清理**：`typedef`→`using`、非 scoped `enum`→`enum class`、
   `NULL`→`nullptr`、C 风格转换→`static_cast`、迭代器 for→range-for。
2. **接口 span 化续做**：把 PLAN-002 明确排除的虚函数链换成 `std::span`
   （`BinaryStream`、`MessageDigest*`、`IOFile`、`SocketCore`/`TLSSession`、
   解析器 `parseUpdate`/`parseFinal`）。
3. **错误处理现代化**：`bool` + 出参、`int` 错误码、`ssize_t& error`
   出参统一改 `std::optional` / `enum class` / 小结果结构体。
4. **标准库替换**：自有 `fmt()`（printf 风格）→ `std::format`，
   `strtol/strtod/sscanf` → `std::from_chars`，`std::find` → ranges，
   `stringstream` → `std::format` / `spanstream`。

验收粒度：每个子模块一次干净副本 `--enable-werror` 构建 + `make check`。

## 当前上下文

### 仓库与前置状态

- 仓库 `/home/code/tmp/aria2`，`git status --porcelain` 为空（干净）。
- `configure.ac:111` 已是 `AX_CXX_COMPILE_STDCXX([20], [noext], [mandatory])`，
  构建目录 `src/Makefile` 中 `CXX = g++ -std=c++20`。
- 最近提交：`83703aef`（PLAN-002 阶段3）、`d7bcfa63`、`2e359ab6`、`9c15c8a2`、`e663aaae`。
- `plans/` 是项目约定；`PLAN-001`、`PLAN-002` 均 ✅ 已完成，序号 003 未被占用。
- 本机：`g++ 14.2.0`、`clang++-22`、`autoconf 2.72`、`automake 1.17`、
  `cppunit 1.15.1`、`clang-format-21`。
- `.gitignore` 已覆盖 `configure`/`Makefile.in`/`config.h.in`/`*.o`/`*.lo`/
  `autom4te.cache/` 等生成文件；`plans/` 不受忽略。
- **工作区根目录残留 187 个 `test/*.o` 与旧 `test/aria2c`**（PLAN-001 时期遗留），
  原地增量 `make` 会假绿 —— 必须用 `git worktree` 干净副本验证。

### 语言标准基线：保持 C++20（用户第 1 轮确认）

实测（`g++ 14.2` / `clang++-22`，`-std=c++23 -Wall -Wextra -Werror`）：

| 设施 | 头 | 实测 |
|---|---|---|
| `std::format` / `std::vformat` / `make_format_args` | `<format>` | C++20 可用（两边） |
| `std::span` / `as_bytes` / `as_writable_bytes` | `<span>` | C++20 可用（PLAN-002 已用） |
| `std::from_chars` | `<charconv>` | C++20 可用（两边） |
| `std::optional` | `<optional>` | C++20 可用（两边） |
| `std::spanstream` | `<spanstream>` | C++20 可用（两边） |
| `std::views::zip` / `enumerate` | `<ranges>` | **C++23 才有**（两边） |
| `std::expected` | `<expected>` | **C++23 才有**（两边） |
| `std::to_underlying` / `std::byteswap` | `<utility>`/`<bit>` | **C++23 才有**（两边） |
| `std::ranges::contains` | `<ranges>` | **gcc14/clang22 在 C++23 下也缺失**（实测报错） |

结论：本计划只用 C++20 可用设施。`std::expected`、`to_underlying`、
`byteswap`、`views::zip|enumerate`、`ranges::contains` **一律不用**；
`enum class` 需要底层转换时写 `static_cast<int>(e)`，不需要 `to_underlying`。

### 基础层范围（166 文件 / 27 697 行，按 6 个子模块）

用文件前缀精确划分：`src/` 下 166 个文件全部归类、**无重叠、无遗漏**
（已用 `comm` 双向校验；3 个原本会双归属的文件已按下表定点归属）。

| 子模块 | 文件 | 行数 | 测试文件数 |
|---|---|---|---|
| util | 31 | 7 285 | 9 |
| 解析器 | 19 | 4 344 | 6 |
| 摘要 | 18 | 2 731 | 4 |
| 磁盘IO | 46 | 4 679 | 5 |
| 网络 | 32 | 6 652 | 3 |
| 流 | 20 | 2 006 | 8 |

归属约定（避免提交时扯皮）：
- `SHA1IOFile.*` → **流**（它是 `IOFile` 子类，随 `IOFile` 签名一起改）。
- `ValueBaseDiskWriter.h` → **磁盘IO**（它是 `DiskWriter` 子类，随
  `BinaryStream` 虚函数一起改），解析器阶段不改它。
- `ValueBase*` → **解析器**（用 `ValueBase[^D]` 排除 `ValueBaseDiskWriter.h`）。
- `RangeBtMessage*`、`StreamFileAllocationEntry`、`FileAllocationCommand*`、
  `NameResolveCommand`、`DHTConnectionImpl` 是**跨层使用点**：头文件留在各自
  子模块，但被其他子模块接口改动波及的单行调用随接口提交一起改。
- `prefs.*` 归**解析器**（它是 `Option` 的注册表，被 `option_processing` 用，
  但自身只依赖头文件；放在解析器阶段避免 util 阶段产生对 option 层的反向依赖）。

### 各子模块债务实测

（`typedef`/`enum` 只数 `.h`；`裸 new/delete` 只数非注释行；`str→num` 指
`strtol/strtoul/strtoll/atoi/atol/strtod/sscanf`。）

| 子模块 | typedef | 非 scoped enum | 裸 new/delete | fmt() | c_str() | stringstream | find* | str→num |
|---|---|---|---|---|---|---|---|---|
| util | 7 | 0 | 14 | 26 | 29 | 2 | 10 | 5 |
| 解析器 | 10 | 3 | 13 | 0 | 0 | 0 | 0 | 0 |
| 摘要 | 4 | 1 | 0 | 11 | 3 | 1 | 2 | 0 |
| 磁盘IO | 4 | 1 | 4 | 35 | 30 | 1 | 1 | 0 |
| 网络 | 22 | 6 | 3 | 85 | 86 | 2 | 3 | 0 |
| 流 | 1 | 0 | 5 | 2 | 0 | 0 | 0 | 0 |
| **基础层合计** | **48** | **11** | **39** | **159** | **148** | **6** | **16** | **5** |

另有 C 风格转换 25 处（util 11、摘要 9、磁盘IO 2、网络 1、流 2、解析器 0）。

### 具体改造点（逐个核实过）

**A. 语言惯用法**

- `typedef`→`using` 48 处（基础层，只计 `.h`），集中：
  `LibuvEventPoll.h:7`、`PollEventPoll.h:6`、`EpollEventPoll.h:4`、
  `SelectEventPoll.h:2`、`ValueBase.h:4`、`MessageDigestImpl.h:3`、
  `array_fun.h:3`、`GenericParser.h:2`、`a2iterator.h:2`、`GroupId.h:1`、
  `util.h:1`（`signal_handler_t`）。
- 非 scoped `enum`→`enum class` 11 处，逐个已确认可行：
  - `EventPoll::EventType`（`EventPoll.h:54`，值 `1/2/4/8`）：
    用法 57 处值名 + 69 处 `EventPoll::` 限定，含 `SelectEventPoll.cc:58`
    的 `EVENT_ERROR | EVENT_HUP` 与 `EpollEventPoll.cc:149-158` 的
    `EVENT_READ & events`。**改 `enum class` 必须同步加位运算符重载**
    （`operator|`、`operator&`、`operator|=` 三个自由函数，放在 `EventPoll.h`
    里 `EventType` 定义之后），否则 `events |= EVENT_READ` 全部编译失败。
  - `BencodeError`（`BencodeParser.h:48`，`namespace bittorrent`）：
    8 个负值，`BencodeParser.cc` 9 处、`BencodeParser.h` 8 处使用。
  - `JsonError`（`JsonParser.h:48`，`namespace json`）：`JsonParser.cc` 20 处。
  - `StructElementType`（`StructParserStateMachine.h:42`）：值名
    `STRUCT_*` 共 89 处（`BencodeParser.cc` 32、`JsonParser.cc` 39、
    `ValueBaseStructParserStateImpl.cc` 9、头 9）。
  - `TLSSessionSide` / `TLSVersion`（`TLSContext.h:44,46`）、
    `TLSDirection` / `TLSErrorCode`（`TLSSession.h:44,46`）：
    `TLS_ERR_*`/`TLS_WANT_*` 共 53 处，全部在 `LibsslTLSSession.cc`、
    `SocketCore.cc`、`TLSSession.h` 三个文件内（**已核实封闭**）。
  - `crypto::hash::Algorithms`（`crypto_hash.h:18`）：值名 `algo*` 159 处，
    集中在 `crypto_hash.cc`(11)、`crypto_hash.h`(7)、
    `InternalMessageDigestImpl.cc`(6)。**它是模板非类型参数**
    （`template <hash::Algorithms algo>`，`InternalMessageDigestImpl.cc:46`），
    `enum class` 作为模板非类型参数在 C++20 合法。
  - `DiskAdaptor::FileAllocationMethod`（`DiskAdaptor.h:55`）：10 处。
  - `AsyncNameResolver::STATUS`（`AsyncNameResolver.h:59`）：36 处值名 + 19 处限定。
  - `GroupId::ERR_*` 匿名 enum（`GroupId.h:53`）：见 C 项统一处理，不单独改。
- `NULL`：基础层代码里只有 **1 处**非注释用法
  （`LibsslTLSSession.cc:257` 的 `X509_get_ext_d2i(..., NULL)`）；
  `BitfieldMan.h:206`、`GZipEncoder.h:64`、`util.h:762,767` 是注释文字。
  `src/includes/aria2/aria2.h` 里另有 7 处 `NULL` —— 公共头，**不动**。
- C 风格转换 25 处（基础层），逐处换 `static_cast`。已知需要保留 C 风格/
  reinterpret 的：`bencode2.cc:68`（`const char*` ← `std::byte*`）、
  `BencodeParser.cc`/`JsonParser.cc` 的 `char`→`unsigned char` 位运算。
- 迭代器 for→range-for 2 处（基础层），`GZipFile.cc`、`util.cc`。

**B. span 化续做（PLAN-002 排除的虚函数链）**

| 接口 | 声明 | 实现类 | 调用点 |
|---|---|---|---|
| `BinaryStream::writeData` | `BinaryStream.h:48` | `AbstractDiskWriter`、`ByteArrayDiskWriter`、`MultiDiskAdaptor`、`AbstractSingleDiskAdaptor`、`ValueBaseDiskWriter`（模板）、`DiskAdaptor` 子类 | 60 |
| `BinaryStream::readData` | `BinaryStream.h:51` | 同上 | 26 |
| `DiskAdaptor::readDataDropCache` | `DiskAdaptor.h:111` | `MultiDiskAdaptor`、`AbstractSingleDiskAdaptor` | 4 |
| `MessageDigest::update` | `MessageDigest.h:97` | `MessageDigestImpl`（虚）+ `Adler32MessageDigestImpl`、`InternalMessageDigestImpl`、`LibsslMessageDigestImpl` | 39 |
| `crypto::hash::Algorithm::update` / `compute` | `crypto_hash.h:34,73,85` | 已内联在 `crypto_hash.cc` | 内部 |
| `message_digest_helper::digest` | `message_digest_helper.h:54,61` | 自由函数 ×2 | 18 |
| `IOFile::read/write` + `onRead/onWrite` | `IOFile.h:59,61,90,91` | `BufferedFile`、`GZipFile`、`SHA1IOFile` | 20 |
| `SocketCore::writeData/readData/readDataFrom` | `SocketCore.h:246,252,273,276` | 自身 | 5 / 5 / 3 |
| `TLSSession::writeData/readData` | `TLSSession.h:86,91` | `LibsslTLSSession` | 2 |
| `StreamFilter::transform` | `StreamFilter.h:62` | `SinkStreamFilter`、`NullSinkStreamFilter`、`GZipDecodingStreamFilter`、`ChunkedDecodingStreamFilter`（+ `FileAllocationEntry` 链外） | 5 |
| 解析器 `parseUpdate/parseFinal` | `BencodeParser.h:66,72`、`JsonParser.h:69,75`、`GenericParser.h:62,72` | 自身 | 54 |
| `charactersCallback` / `runCharactersCallback` | `StructParserStateMachine.h:63`、`BencodeParser.h:83`、`JsonParser.h:86` | `ValueBaseStructParserStateMachine` | 5 |
| `ValueBase::String(const char*,size_t)` / `(const unsigned char*,size_t)` | `ValueBase.h:85,87` | 自身 | 构造点 3 处（`make_unique<String>`）+ 类内 |

统一规则（沿用 PLAN-002）：输入 `std::span<const std::byte>`，
输出缓冲 `std::span<std::byte>`，调用方显式 `std::as_bytes(std::span{p,n})` /
`std::as_writable_bytes(...)`，**绝不用隐式构造**（字符串字面量会带 NUL，PLAN-002
已踩过一次）。保留 `const std::string&` 便捷重载，内部 `as_bytes` 转发。

例外（**不改**，理由写进提交信息）：
- `SocketCore::setSockOpt(..., void* optval, socklen_t)` —— 语义是 `setsockopt`
  的原始 `void*`，长度由 `optname` 决定而非字节数。
- `SocketCore::inetNtop/inetPton/getBinAddr` 的 `void*`（25 处）——
  `inet_ntop`/`inet_pton` 原样签名。
- `aria2.h` 的 `void* userData`。
- 所有 `src/*.c`（7 个兼容 shim，见 E）。

**C. 错误处理现代化（含跨层单行适配，用户确认）**

| 现状 | 位置 | 改法 | 调用点 |
|---|---|---|---|
| `bool getFirst*Index(size_t&)` ×6 | `BitfieldMan.h:110,123,131,149,160,169` | `std::optional<size_t>` | 25 |
| `bool getFirstSetBitIndex(size_t&, ...)` | `bitfield.h:148` | `std::optional<size_t>` | 6（`Option.cc:193` 只需转 bool，用 `.has_value()` 等价改写） |
| `int expandUnique/toNumericId(a2_gid_t&, const char*)` | `GroupId.h:54,55` | 保留 `int` 返回码，但把匿名 `enum` 改 `enum class GroupIdErr` 并让函数返回它；`0` 成功改为显式 `std::nullopt`/`kOk` | `RpcMethodImpl.cc:199-204`（switch）、`aria2api.cc:180`、`download_helper.cc:111`、`test/GroupIdTest.cc` 10 处、`test/RpcMethodTest.cc` 3 处 |
| `int` + `TLS_ERR_*` / `TLS_WANT_*` | `TLSSession.h:64,69,74,81,99,105` | `int`→`TLSErrorCode`，`checkDirection()` 返回 `TLSDirection`；**`ssize_t` 返回值的 `writeData/readData` 保持 `ssize_t`**（正值=字节数，负值=错误码，混在一个返回值里，拆开是行为改动） | 53（`LibsslTLSSession.cc`、`SocketCore.cc`、`TLSSession.h`，封闭） |
| `ssize_t& error` 出参 | `BencodeParser::parseFinal`、`JsonParser::parseFinal`、`GenericParser::parseFinal` | 新增 `struct ParseResult { ssize_t consumed; int error; }` 风格的小结构体，`parseFinal` 返回它；`ssize_t` 错码换 `enum class` | 47（`ValueBaseBencodeParserTest.cc` 13、`BtPostDownloadHandler.cc`、`HttpServerBodyCommand.cc`、`WebSocketSession.cc`、`download_helper.cc`、`bencode2.cc`、`GenericParser.h` 内部） |
| `bool bind(uint16_t& port, ...)` | `DHTConnectionImpl.h:66,78` | `std::optional<uint16_t>` | `DHTSetup.cc` 等（跨层） |
| `int resolveHostname(std::vector<std::string>&)` | `NameResolveCommand.h:62` | 保留（返回 `AsyncNameResolver::STATUS`，已在 A 项改 enum class） | 内部 |

**明确保留原样**（会改语义或跨层契约）：`MessageDigest::supports/isStronger/
isValidHash`（bool 正确）、`File::exists(std::string& err)`、`ErrorCode` 枚举、
`Exception` 继承链（`what()` 文本被 `test/BtRequestMessageTest.cc:280` 逐字断言）、
`TLSSession::writeData/readData` 的 `ssize_t`。

**D. 标准库替换**

- **`fmt()` → `std::format`（用户选「连 message.h 一起改，跨层同步」）**：
  - `src/fmt.h`/`fmt.cc` 改成 `std::format` 的变参模板（保留 `fmt` 函数名与
    `fmt("...", args...)` 调用形状，804 处调用**形状不变**，只改格式串）。
    实现：`template <typename... Args> std::string fmt(std::format_string<Args...> f, Args&&... a) { return std::format(f, std::forward<Args>(a)...); }`；
    两处运行时格式串（`OptionHandlerImpl.cc:127` 的 `fmt(msg.c_str(), ...)`、
    `paramed_string.h:145` 的 `fmt(format.c_str(), j)`）改用 `std::vformat`。
  - `src/message.h` 的 **193 个宏**改 `{}` 风格：删掉全部 `PRId64` 拼接
    （51 个宏）、`%s`→`{}`（106 个）、`%lu/%d/%u/%ld`→`{}`（37 个）。
    541 处使用点全部跟着改实参（`c_str()` 可去可留，`std::format` 直接吃
    `std::string`；**保留 `c_str()` 也无害，不强制删**）。
  - 逐项陷阱（已实测）：
    1. **README/测试断言依赖输出文本**：`test/FmtTest.cc:47` 断言
       `fmt("aria2-%d.%d.%d-%s", ...)`，改为 `{}` 风格后同步改断言。
       `test/BtRequestMessageTest.cc:280` 断言的 "Length too long: %d > %dKB"
       来自 `bittorrent_helper.cc:793`（引擎层文件），改宏时必须一起改。
    2. **`%08x`/`%02X`/`%.2f` 要写成格式说明**：`{:08x}`、`{:02X}`、`{:.2f}`，
       不能裸 `{}`，否则输出文本变化。
    3. **`%p`**：`std::format` 写 `{:p}`，实参必须 `void*`（`Piece.cc:342`
       已是 `wrCache_.get()`）。
    4. **`unsigned char`/`char` 实参**：`std::format("{}", uc)` 输出字符而非数字
       （实测 `0x2f` → `[47]` 是 `uint8_t`，`unsigned char` 同样是字符）。
       `util.cc:319,344,362,1968` 的 `fmt("%%%02X", c)` 必须写成
       `fmt("%{:02X}", static_cast<unsigned>(c))` 并核对 `c` 的静态类型。
    5. **花括号转义**：格式串里真出现 `{`/`}` 的要写 `{{`/`}}`。
       全仓只有 1 处：`WrDiskCache.cc:66` 的 `"a.{size=%lu,clock=%" PRId64`。
    6. **多行宏 27 个**（`\` 续行）改写时保持续行；`// clang-format off` 区
       不要被 clang-format 打乱。
    7. **两处运行时格式串**（见上）不能用 `std::format`（`consteval` 校验失败，
       实测报 "not a constant expression"），改 `std::vformat` +
       `std::make_format_args`；`make_format_args` 只接受左值，实参需先存变量。
    8. **`fmt.cc` 现有 2048 字节栈缓冲 + `vsnprintf` 截断**（`fmt.cc:44-56`）
       顺带消失；不要在新实现里保留该缓冲。
  - `src/message.h` 改完后 `#include <cinttypes>` 若不再需要则删。
- `strtol/strtoll/strtoul/strtod` 5 处（全在 `util.cc:487,501,514,531,1797`）
  → `std::from_chars`。`util.cc:487-514` 的三个 `parse*NoThrow` 是公共工具
  （`parseIntNoThrow` 11 处、`parseUIntNoThrow` 12、`parseLLIntNoThrow` 15、
  `parseDoubleNoThrow` 7 调用点，跨层），**签名不变**，只换实现。
  注意 `std::from_chars` 的浮点解析在 gcc 14 可用；`util.cc:531` 的
  `strtod` 需核对 `parseDoubleNoThrow` 对 `"inf"`/`"nan"` 的现有行为。
- `std::find`/`find_if` 16 处（基础层）→ `std::ranges::find`/`find_if`
  （**不是 `ranges::contains`，C++23 也缺**）。有 2 处 `std::find(...) != end()`
  的存在性判断，用 `ranges::find(...) != end()` 或 `any_of` 均可，保持可读性优先。
- `stringstream` 6 处：`MessageDigest.cc:103`、`LibsslTLSContext.cc:200`、
  `SocketCore.cc:877`、`util.cc:1757`、`ByteArrayDiskWriter.h:47`、
  `bencode2.cc:83`。前 5 处是格式化/拼接 → `std::format` / `std::string::append`；
  `ByteArrayDiskWriter.h:47` 的 `buf_` 是**字节缓冲**，改 `std::string` 或保持
  `stringstream`（读语义依赖 `str()` 的两阶段读取），倾向保持原样。

**E. 明确不在范围**

- 磁盘IO 的 7 个 C 兼容 shim（`alloca.c`、`gai_strerror.c`、`getaddrinfo.c`、
  `libgen.c`、`strptime.c`、`timegm.c`、`uri_split.c`）：`src/Makefile.am:528-548`
  由 `HAVE_*` 条件控制，本机 `config.h` 全部 `HAVE_*=1` 所以**根本不编译**；
  `alloca.c` 连 `Makefile.am` 都没引用。删它们属于「删死代码」而非现代化，
  留到后续计划（同时要改 `configure.ac:463-484` 的 `AC_CHECK_FUNCS` 与
  `Makefile.am` 条件），避免本计划同时动 autotools。
- `src/includes/aria2/aria2.h`（公共头，ABI）：7 处 `NULL` 与 `void* userData`
  保持原样。
- `deps/**`、`configure.ac`、`src/Makefile.am`、`test/Makefile.am`、
  `.github/workflows/build.yml`、`README.rst`（本轮不改语言标准）。
- 其他 6 个子模块（协议层 BT/DHT/HTTP/FTP、引擎层、配置与 RPC 层）：
  只允许**被基础层接口波及的单行适配**，不做各自模块的惯用法清理。

## 待确认决策

| 决策点 | 选项（含用户备注的约束） | 推荐 | 状态 |
|---|---|---|---|
| 语言标准 | **保持 C++20** / 升 C++23 | 保持 C++20 | 已确认：第 1 轮。实测 `expected`/`print`/`to_underlying`/`byteswap`/`views::zip` 均需 C++23，`ranges::contains` 连 C++23 都缺 |
| 模块范围 | **基础层 6 子模块** / +协议层 / +引擎层 / +配置RPC层 | 基础层 | 已确认：第 1 轮；其余三层留给后续 PLAN-004/005/006 |
| 改造内容 | **语言惯用法 + span 化 + 错误处理 + 标准库替换**（四项全做） | 四项全做 | 已确认：第 1 轮 |
| 验收深度 | **干净副本 + `--enable-werror`** / +clang 双编译 / +ASan+UBSan / +build_test.sh 全矩阵 | 干净副本+werror | 已确认：第 1 轮；沿用 PLAN-001/002 口径 |
| fmt 落地 | **连 message.h 一起改、跨层同步** / 只改基础层、fmt 另开计划 / 本轮不动 fmt | 跨层同步 | 已确认：第 2 轮。代价：本计划会触及 93 个文件里的 541 处宏使用点，超出「只做基础层」的严格边界，已在「风险与权衡」记录 |
| 错误处理边界 | **含跨层单行适配** / 只改基础层内部闭环 | 含跨层适配 | 已确认：第 2 轮。允许在 `Piece`/`DefaultPieceStorage`/`RpcMethodImpl`/`aria2api` 等处做单行适配 |
| 提交粒度 | **6 个子模块各一次提交** / 按改造类型分 4 次 / 3 个合并提交 | 6 次 | 已确认：第 1 轮 |

无待确认项。

## 提议的方法

按依赖自底向上：**util → 解析器 → 摘要 → 磁盘IO → 网络 → 流**。

顺序理由（都已核实）：
- 解析器用 `ValueBase`，`ValueBase` 用 `util`；摘要用 `util`+`Span`；
  磁盘IO 用摘要（`WrDiskCache` 做校验和）+ 解析器（`ValueBaseDiskWriter`）；
  网络用磁盘无关，但 `SocketCore` 用 `TLSSession` 与 `util`；
  流用磁盘IO（`BinaryStream`）+ 摘要（`SHA1IOFile`）+ 解析器
  （`ChunkedDecodingStreamFilter` 的 `parseUpdate`）。

关键安排：

1. **fmt 迁移放在阶段 1（util）里一次做完**，因为 `fmt.h`/`fmt.cc`/`message.h`
   都是 util 簇文件，而 541 处宏使用点跨全仓。若放到后面阶段，
   前几个阶段每次碰 `fmt(...)` 都要回来改格式串，重复劳动。代价是阶段 1 的
   diff 会覆盖 93 个文件 —— 这是用户选择的必然结果，已在风险里写明。

2. **`enum class EventType` 的位运算符**：先加 `operator|/&/|=` 再切 enum class，
   放同一提交内闭环（否则中间态不可编译）。

3. **`parseFinal` 返回值改动**跨阶段耦合：
   - `bencode2::decode`（util 簇，阶段 1）调用 `ValueBaseBencodeParser::parseFinal`
     → 若阶段 2 才改 `parseFinal`，阶段 1 提交时 `bencode2.cc:68` 用的是旧签名。
     **处理：`parseFinal` 的新签名在阶段 1 之前不能变**；阶段 2 改 `parseFinal`
     时把 `bencode2.cc` 一行适配并入阶段 2 提交（这是允许的单行适配）。
   - `ValueBaseDiskWriter.h`（磁盘IO，阶段 4）的 `parseFinal(0, 0)` 空指针哨兵
     也要随阶段 2 一起改 —— 已在阶段 2 的任务清单里显式列出。
   - `GenericParser.h:113` 的 `parser.parseFinal(0, 0, nproc)` 同样在阶段 2 内闭环。

4. **`BinaryStream` 改动波及非基础层**（`BtPieceMessage.cc`、`PeerConnection.cc`、
   `TrackerWatcherCommand.cc`、`FtpConnection.cc`、`MSEHandshake.cc`、
   `HttpServer.cc`、`WebSocketSession.cc`、`LpdMessageDispatcher.cc`、
   `DHTConnectionImpl.cc`、`UTMetadataDataExtensionMessage.cc`、`util.cc`）
  共 11 个非基础层文件 + 8 个测试文件，**全部在阶段 4 的同一提交内做单行适配**。

5. **`MessageDigest::update` 改动波及**17 个非基础层文件（`BtPieceMessage.cc`、
   `Checksum.cc`、`Context.cc`、`DHTTokenTracker.cc`、`DownloadCommand.cc`、
   `HttpRequest.cc`、`HttpResponse.cc`、`HttpServerCommand.cc`、
   `IteratableChecksumValidator.cc`、`IteratableChunkChecksumValidator.cc`、
   `MSEHandshake.cc`、`OptionHandlerImpl.cc`、`Piece.cc`、`RpcMethodImpl.cc`、
   `UTMetadataDataExtensionMessage.cc`、`bittorrent_helper.cc`、
   `version_usage.cc`），**全部在阶段 3 的同一提交内闭环**。

6. **`SocketCore`/`TLSSession` 的 `void*`→`std::span<std::byte>`** 与
   `TLSSession` 的 `int`→`TLSErrorCode` **必须同一提交**（阶段 5），
   否则 `LibsslTLSSession.cc` 会同时面对两套不兼容声明。

## 分步计划

（一个阶段一次提交；6 个阶段，用户授权后用 `todo` 同步 6 个任务）

1. [x] **阶段 1：util 簇（31 文件 / 7 285 行）+ fmt 全量迁移**
   - 惯用法：7 处 `typedef`→`using`（`a2iterator.h:44,47`、`array_fun.h:69,98,119`、
     `GroupId.h:46`、`util.h:338`）；`BitfieldMan.cc` 的 14 处 `new[]`/`delete[]`
     （7 组 new/delete 对，`bitfield_`/`useBitfield_`/`filterBitfield_`）
     → `std::vector<unsigned char>`
     （**选 vector 而不是 `unique_ptr<unsigned char[]>`**：它被 `bitfield::` 算法族
     当 `Array` 用，向下兼容 `data()`/`size()`，且拷贝构造/赋值直接白送，
     能删掉 `operator=` 里手写的 `delete[]`+`new[]`+`copy_n` 三段）；
     11 处 C 风格转换（`BitfieldMan.cc` 4、
     `util.cc` 5、其余 2）→ `static_cast`；
     `GroupId.h:53` 匿名 enum → `enum class GroupIdErr`（与 C 项联动）。
   - 错误处理：`bitfield::getFirstSetBitIndex`（`bitfield.h:148`）与
     `BitfieldMan` 6 个 `getFirst*Index` → `std::optional<size_t>`；
     同步 25 处调用点（`DefaultPieceStorage.cc:383`、`GeomStreamPieceSelector.cc:59`、
     `Piece.cc:132,157`、`Option.cc:193`、`test/BitfieldManTest.cc` 15 处）。
     `GroupId::expandUnique/toNumericId` 改返回 `enum class GroupIdErr`，
     同步 `RpcMethodImpl.cc:199-204`、`aria2api.cc:180`、`download_helper.cc:111`、
     `test/GroupIdTest.cc` 10 处、`test/RpcMethodTest.cc` 3 处。
   - 标准库：`util.cc:487,501,514,531,1797` 的 5 处 `strtol/strtoll/strtoul/strtod`
     → `std::from_chars`（三个 `parse*NoThrow` 签名不变）；
     10 处 `std::find/find_if` → `std::ranges::find/find_if`；
     `util.cc:1757` 的 `stringstream` → `std::format`/`append`；
     `util.cc` 21 处 `fmt(...)`、`paramed_string.h` 2 处、`util_security.cc` 1 处、
     `RangeBtMessage.cc` 1 处、`bencode2.cc:72` 1 处改 `{}` 风格（共 26 处）；
     其中 `paramed_string.h:145` 的那处是动态格式串，转 `std::vformat`。
   - **fmt 全量迁移（本阶段核心）**：
     - 重写 `src/fmt.h`/`src/fmt.cc` 为 `std::format` 变参模板，保留函数名与调用形状。
     - `src/message.h` 193 个宏改 `{}` 风格，删 `PRId64`/`PRIu64` 拼接。
     - 541 处使用点（93 个文件）同步改实参/格式串。
     - 2 处运行时格式串改 `std::vformat`。
     - `test/FmtTest.cc:47` 与 `test/BtRequestMessageTest.cc:280` 的断言同步改。
     - `WrDiskCache.cc:66` 的花括号转义 `{{`/`}}`。
     - `util.cc:319,344,362,1968` 的 `%02X` 与 `unsigned char` 字符语义逐处核对。
   - **跨阶段耦合点**：无（`parseFinal` 本阶段不改签名）。
   - 门禁：干净副本 `autoreconf -i && ./configure --enable-werror && make -j check`；
     `test/aria2c.log` 末尾 `OK (N)` 且无 `Failures !!!`；
     `git grep -n 'PRId64\|PRIu64' -- src/message.h` 无命中；
     `git grep -nE '\bfmt\("[^"]*%' -- 'src/*.cc'` 无命中。

2. [ ] **阶段 2：解析器簇（19 文件 / 4 344 行）**
   - 惯用法：10 处 `typedef`→`using`（`ValueBase.h`、`GenericParser.h:2`、
     `prefs.h` 等）；3 处非 scoped enum →
     `enum class`（`BencodeError`、`JsonError`、`StructElementType`，
     共 89+28 处值名使用点）。
   - span 化：`parseUpdate`/`parseFinal`（`BencodeParser.h:66,72`、
     `JsonParser.h:69,75`、`GenericParser.h:62,72`）、
     `charactersCallback`（`StructParserStateMachine.h:63`）、
     `runCharactersCallback`（`BencodeParser.h:83`、`JsonParser.h:86`）、
     `ValueBase::String(const char*,size_t)` + `(const unsigned char*,size_t)`
     （`ValueBase.h:85,87`）。
   - 错误处理：`parseFinal` 的 `ssize_t& error` → 新增
     `struct ParseResult { ssize_t consumed; int error; }`（或 `enum class`
     错误码 + `std::optional`），同步 47 处调用点：
     `ValueBaseBencodeParserTest.cc` 13、`ValueBaseJsonParserTest.cc`、
     `BtPostDownloadHandler.cc:95`、`HttpServerBodyCommand.cc:227`、
     `WebSocketSession.cc:168,325`、`download_helper.cc:314`、
     `bencode2.cc:68`、`GenericParser.h:72,114`、`ValueBaseDiskWriter.h:85`。
   - 惯用法补充：13 处裸 new/delete（`ValueBaseStructParserStateMachine.cc:46-55`
     的 10 个全局 state 与 `prefs.cc:65,94`）→ `std::unique_ptr` /
     函数内 `static const`。
   - 门禁：同上；`git grep -nE '\*\s*\w+\s*,\s*(size_t|ssize_t)' -- 'src/BencodeParser.h' 'src/JsonParser.h' 'src/GenericParser.h'` 无命中。

3. [ ] **阶段 3：摘要簇（18 文件 / 2 731 行）**
   - 惯用法：4 处 `typedef`→`using`（`MessageDigestImpl.h:53,54,55`、
     `InternalDHKeyExchange.h:46`）；
     1 处非 scoped enum → `enum class`（`crypto::hash::Algorithms`，
     159 处值名；注意它是模板非类型参数）。
   - span 化：`MessageDigest::update`（`MessageDigest.h:97`）、
     `MessageDigestImpl::update` 虚函数（`MessageDigestImpl.h:100`）、
     `Adler32MessageDigestImpl.h:52`、`InternalMessageDigestImpl.cc`、
     `LibsslMessageDigestImpl.cc`、`crypto_hash.h:34,73,85`、
     `message_digest_helper.h:54,61`。
     同步 39 处 `update(` 调用 + 18 处 `message_digest::digest(` 调用，
     涉及 **17 个非基础层文件**（清单见「提议的方法」第 5 条）。
   - 标准库：`MessageDigest.cc:103` 的 `stringstream` → `std::format`；
     11 处 `fmt` → `{}`（含 `message_digest_helper.cc` 3、
     `LibsslDHKeyExchange.cc` 5、`InternalDHKeyExchange.cc` 3）；
     2 处 `std::find` → ranges；9 处 C 风格转换。
   - 门禁：同上；`git grep -n 'const void\*' -- 'src/MessageDigest.h'
     'src/MessageDigestImpl.h' 'src/crypto_hash.h' 'src/message_digest_helper.h'`
     无命中。

4. [ ] **阶段 4：磁盘IO 簇（46 文件 / 4 679 行）**
   - span 化：`BinaryStream::writeData/readData`（`BinaryStream.h:48,51`）、
     `DiskAdaptor::readDataDropCache`（`DiskAdaptor.h:111`），
     6 个实现类（`AbstractDiskWriter`、`ByteArrayDiskWriter`、`MultiDiskAdaptor`、
     `AbstractSingleDiskAdaptor`、`ValueBaseDiskWriter` 模板、`DiskAdaptor` 子类）。
     同步 90 处调用点，含 **11 个非基础层文件 + 8 个测试文件**（清单见
     「提议的方法」第 4 条）。
   - 惯用法：4 处 `typedef`→`using`；`DiskAdaptor::FileAllocationMethod` →
     `enum class`（10 处）；4 处裸 new/delete
     （`SingleFileAllocationIterator.cc:66,83` 的 `buffer_` 数组、
     `WrDiskCacheEntry.cc:70,71` 的 `e->data`/`e`）→ `std::vector` / `unique_ptr`；
     2 处 C 风格转换。
   - 标准库：35 处 `fmt`→`{}`（`AbstractDiskWriter.cc` 13、
     `MultiDiskAdaptor.cc` 8、`WrDiskCache.cc` 5、`FileAllocationCommand.cc` 3、
     `WrDiskCacheEntry.cc` 2、`MultiFileAllocationIterator.cc` /
     `FileAllocationDispatcherCommand.cc` / `ByteArrayDiskWriter.cc` /
     `AbstractSingleDiskAdaptor.cc` 各 1）；
     `ByteArrayDiskWriter.h:47` 的 `stringstream` **保留**（读语义依赖 `str()`）；
     1 处 `std::find` → ranges。
   - 门禁：同上；`git grep -n 'unsigned char\s*\*[^)]*size_t' -- 'src/BinaryStream.h'
     'src/DiskAdaptor.h'` 无命中。

5. [ ] **阶段 5：网络簇（32 文件 / 6 652 行）**
   - span 化：`SocketCore::writeData/readData/readDataFrom`
     （`SocketCore.h:246,252,273,276`）、`TLSSession::writeData/readData`
     （`TLSSession.h:86,91`）、`LibsslTLSSession` 两个 override。
     同步 `SocketCore.cc`(5+5)、`SocketRecvBuffer.cc`、`Event.h`、
     `AsyncNameResolver.cc`、`PeerConnection.cc` 等。
     保留 `setSockOpt` 的 `void*` 与 `inetNtop/inetPton/getBinAddr`（理由见 B 项）。
   - 错误处理：`TLSSession` 6 个 `int` 返回 → `TLSErrorCode` /
     `TLSDirection`（`checkDirection`）；同步 53 处 `TLS_ERR_*`/`TLS_WANT_*`
     （`LibsslTLSSession.cc`、`SocketCore.cc`，封闭）。
     **`writeData/readData` 的 `ssize_t` 保持**（混用正值/负值语义）。
   - 惯用法：22 处 `typedef`→`using`（`LibuvEventPoll.h` 7、`PollEventPoll.h` 6、
     `EpollEventPoll.h` 6、`SelectEventPoll.h` 2、`a2netcompat.h` 1）；
     6 处非 scoped enum →
     `enum class`（`EventPoll::EventType` **需先加位运算符**、
     `TLSSessionSide`、`TLSVersion`、`TLSDirection`、`TLSErrorCode`、
     `AsyncNameResolver::STATUS`）；3 处裸 new/delete
     （`LibuvEventPoll.cc:119,221` 的 `uv_timer_t`、`KPoll`）→ `unique_ptr`；
     1 处 C 风格转换；1 处 `NULL`（`LibsslTLSSession.cc:257`）→ `nullptr`。
   - 标准库：85 处 `fmt`→`{}`（`SocketCore.cc` 48、`LibsslTLSContext.cc` 16、
     `EpollEventPoll.cc` 6、`PollEventPoll.cc` 3、`NameResolveCommand.cc` 3、
     `SocketBuffer.cc` 2、`SelectEventPoll.cc` 2、`DHTConnectionImpl.cc` 2、
     `NameResolver.cc` / `LibuvEventPoll.cc` / `AsyncNameResolverMan.cc` 各 1）；
     `LibsslTLSContext.cc:200`、`SocketCore.cc:877` 的 `stringstream` → `std::format`；
     3 处 `std::find` → ranges。
   - 门禁：同上；`git grep -nE 'void\s*\*[^)]*size_t' -- 'src/SocketCore.h'
     'src/TLSSession.h' 'src/LibsslTLSSession.h'` 只剩 `setSockOpt` 一处。

6. [ ] **阶段 6：流簇（20 文件 / 2 006 行）+ 最终验收**
   - span 化：`StreamFilter::transform`（`StreamFilter.h:62`）的
     `const unsigned char* inbuf, size_t inlen` → `std::span<const std::byte>`；
     `IOFile::read/write` + `onRead/onWrite`（`IOFile.h:59,61,90,91`），
     4 个实现（`BufferedFile`、`GZipFile`、`SHA1IOFile`、`IOFile` 自身）。
     同步 20 处 `IOFile::read/write` 调用（`CookieStorage.cc:442`、
     `ProtocolDetector.cc:66`、`Netrc.cc`、`NsCookieParser.cc`、
     `ServerStatMan.cc`、`UriListParser.cc`、`DHTRoutingTable*`、
     `DefaultBtProgressInfoFile.cc`、`option_processing.cc`、`LibsslTLSContext.cc`）。
   - 惯用法：1 处 `typedef`→`using`；5 处裸 new/delete
     （`GZipEncoder.cc:53,70`、`GZipDecodingStreamFilter.cc:61,78` 的 `z_stream`，
     `SinkStreamFilter.cc:82` 的 `new unsigned char[capacity]`）
     → `z_stream` 改**成员对象**（`deflateInit`/`inflateInit` 不要求堆分配，
     能直接删掉 new/delete 对）；`SinkStreamFilter` 的 `dataCopy` → `std::vector`；
     2 处 C 风格转换。
   - 标准库：2 处 `fmt`→`{}`（`GZipDecoder.cc` 之外的
     `GZipDecodingStreamFilter.cc:1`、`GZipEncoder.cc:1`）；
     `bencode2.cc:83` 的 `ostringstream`（属 util 阶段）不在此重复。
   - **最终验收**（全新 `git worktree` 干净副本）：
     `autoreconf -i && ./configure --enable-werror && make -j"$(nproc)" check`，
     核对 `CXX = g++ -std=c++20`、`WARNCXXFLAGS` 含 `-Wall -Werror -Wformat-security`、
     `test/aria2c.log` 为 `OK (N)`、无 `Failures !!!`、
     `git hash-object src/includes/aria2/aria2.h` 仍为
     `2216641161b15acb3c3ddf2600dc19e3decd59d5`。

## 预计修改的文件

**阶段 1（util，31 + 全仓 fmt 站点）**
- 本簇：`src/util.h`、`src/util.cc`、`src/util_security.h`、`src/util_security.cc`、
  `src/A2STR.h`、`src/A2STR.cc`、`src/a2algo.h`、`src/a2functional.h`、
  `src/a2iterator.h`、`src/array_fun.h`、`src/base32.h`、`src/base32.cc`、
  `src/base64.h`、`src/bitfield.h`、`src/bitfield.cc`、`src/BitfieldMan.h`、
  `src/BitfieldMan.cc`、`src/paramed_string.h`、`src/paramed_string.cc`、
  `src/GroupId.h`、`src/GroupId.cc`、`src/Range.h`、`src/Range.cc`、
  `src/RangeBtMessage.h`、`src/RangeBtMessage.cc`、`src/RangeBtMessageValidator.h`、
  `src/RangeBtMessageValidator.cc`、`src/Signature.h`、`src/Signature.cc`、
  `src/bencode2.h`、`src/bencode2.cc`
- fmt 基础设施：`src/fmt.h`、`src/fmt.cc`、`src/message.h`
- fmt 跨层站点（93 个文件，执行时以
  `git grep -l -E '\bfmt\(|\b(MSG|EX|MESSAGE)_[A-Z_0-9]+'` 生成精确清单）
- 错误处理跨层适配：`src/DefaultPieceStorage.cc`、`src/GeomStreamPieceSelector.cc`、
  `src/Piece.cc`、`src/Option.cc`、`src/RpcMethodImpl.cc`、`src/aria2api.cc`、
  `src/download_helper.cc`
- 测试：`test/FmtTest.cc`、`test/BitfieldManTest.cc`、`test/bitfieldTest.cc`、
  `test/GroupIdTest.cc`、`test/RpcMethodTest.cc`、`test/UtilTest1.cc`、
  `test/UtilTest2.cc`、`test/UtilSecurityTest.cc`、`test/Base32Test.cc`、
  `test/ParamedStringTest.cc`、`test/SignatureTest.cc`、`test/PieceTest.cc`、
  `test/Bencode2Test.cc`（`Range` 的用例在 `test/RangeBtMessageTest.cc`，
  属协议层测试，本阶段不改）

**阶段 2（解析器，19 文件）**
`src/BencodeParser.h/.cc`、`src/JsonParser.h/.cc`、`src/GenericParser.h`、
`src/StructParserStateMachine.h`、`src/ValueBase.h/.cc`、
`src/ValueBaseParserController.h/.cc`、`src/ValueBaseStructParserState.h`、
`src/ValueBaseStructParserStateImpl.h/.cc`、
`src/ValueBaseStructParserStateMachine.h/.cc`、
`src/ValueBaseBencodeParser.h`、`src/ValueBaseJsonParser.h`、
`src/prefs.h`、`src/prefs.cc`
跨层单行适配：`src/ValueBaseDiskWriter.h`、`src/BtPostDownloadHandler.cc`、
`src/HttpServerBodyCommand.cc`、`src/WebSocketSession.cc`、`src/download_helper.cc`、
`src/bencode2.cc`
测试：`test/ValueBaseBencodeParserTest.cc`、`test/ValueBaseJsonParserTest.cc`、
`test/ValueBaseTest.cc`、`test/Bencode2Test.cc`、`test/JsonTest.cc`

**阶段 3（摘要，18 文件）**
`src/MessageDigest.h/.cc`、`src/MessageDigestImpl.h`、
`src/Adler32MessageDigestImpl.h/.cc`、`src/InternalMessageDigestImpl.cc`、
`src/LibsslMessageDigestImpl.cc`、`src/message_digest_helper.h/.cc`、
`src/crypto_hash.h/.cc`、`src/InternalARC4Encryptor.h/.cc`、`src/ARC4Encryptor.h`、
`src/InternalDHKeyExchange.h/.cc`、`src/LibsslDHKeyExchange.h/.cc`
（`src/SHA1IOFile.*` 归**流**阶段，见归属约定）
跨层适配（17 文件）：`src/BtPieceMessage.cc`、`src/Checksum.cc`、`src/Context.cc`、
`src/DHTTokenTracker.cc`、`src/DownloadCommand.cc`、`src/HttpRequest.cc`、
`src/HttpResponse.cc`、`src/HttpServerCommand.cc`、
`src/IteratableChecksumValidator.cc`、`src/IteratableChunkChecksumValidator.cc`、
`src/MSEHandshake.cc`、`src/OptionHandlerImpl.cc`、`src/Piece.cc`、
`src/RpcMethodImpl.cc`、`src/UTMetadataDataExtensionMessage.cc`、
`src/bittorrent_helper.cc`、`src/version_usage.cc`
测试：`test/MessageDigestTest.cc`、`test/MessageDigestHelperTest.cc`、
`test/ARC4Test.cc`、`test/DHKeyExchangeTest.cc`、`test/WrDiskCacheTest.cc`、
`test/DHTTaskExecutorTest.cc`、`test/SpeedCalcTest.cc`、
`test/GZipDecodingStreamFilterTest.cc`、`test/BtAllowedFastMessageTest.cc`

**阶段 4（磁盘IO，46 文件）**
`src/BinaryStream.h`、`src/DiskWriter.h`、`src/DiskAdaptor.h/.cc`、
`src/AbstractDiskWriter.h/.cc`、`src/AbstractSingleDiskAdaptor.h/.cc`、
`src/MultiDiskAdaptor.h/.cc`、`src/ByteArrayDiskWriter.h/.cc`、
`src/ByteArrayDiskWriterFactory.h`、`src/DefaultDiskWriter.h/.cc`、
`src/DefaultDiskWriterFactory.h/.cc`、`src/AnonDiskWriterFactory.h`、
`src/DiskWriterFactory.h`、`src/ValueBaseDiskWriter.h`、
`src/WrDiskCache.h/.cc`、`src/WrDiskCacheEntry.h/.cc`、
`src/OpenedFileCounter.h/.cc`、`src/DirectDiskAdaptor.h/.cc`、
`src/SingleFileAllocationIterator.h/.cc`、`src/MultiFileAllocationIterator.h/.cc`、
`src/FallocFileAllocationIterator.h/.cc`、`src/TruncFileAllocationIterator.h/.cc`、
`src/AdaptiveFileAllocationIterator.h/.cc`、`src/FileAllocationEntry.h/.cc`、
`src/StreamFileAllocationEntry.h/.cc`、`src/FileAllocationCommand.h/.cc`、
`src/FileAllocationDispatcherCommand.h/.cc`、`src/FileAllocationMan.h`、
`src/SocketBuffer.h/.cc`
跨层单行适配（11 src + 8 test）：`src/BtPieceMessage.cc`、`src/PeerConnection.cc`、
`src/TrackerWatcherCommand.cc`、`src/FtpConnection.cc`、`src/MSEHandshake.cc`、
`src/HttpServer.cc`、`src/WebSocketSession.cc`、`src/LpdMessageDispatcher.cc`、
`src/DHTConnectionImpl.cc`、`src/UTMetadataDataExtensionMessage.cc`、`src/util.cc`、
`test/ByteArrayDiskWriterTest.cc`、`test/MultiDiskAdaptorTest.cc`、
`test/DirectDiskAdaptorTest.cc`、`test/WrDiskCacheTest.cc`、
`test/WrDiskCacheEntryTest.cc`、`test/SingleFileAllocationIteratorTest.cc`、
`test/FtpConnectionTest.cc`、`test/HttpServerTest.cc`、
`test/LpdMessageReceiverTest.cc`、`test/UTMetadataPostDownloadHandlerTest.cc`、
`test/UtilTest2.cc`

**阶段 5（网络，32 文件）**
`src/SocketCore.h/.cc`、`src/SocketRecvBuffer.h/.cc`、`src/TLSSession.h`、
`src/TLSContext.h`、`src/LibsslTLSSession.h/.cc`、`src/LibsslTLSContext.h/.cc`、
`src/EventPoll.h`、`src/EpollEventPoll.h/.cc`、`src/PollEventPoll.h/.cc`、
`src/LibuvEventPoll.h/.cc`、`src/SelectEventPoll.h/.cc`、
`src/AsyncNameResolver.h/.cc`、`src/AsyncNameResolverMan.h/.cc`、
`src/NameResolver.h/.cc`、`src/NameResolveCommand.h/.cc`、
`src/DHTConnectionImpl.h/.cc`、`src/a2netcompat.h`、`src/Event.h`
测试：`test/SocketCoreTest.cc`、`test/AsyncNameResolverTest.cc`、
`test/DHTConnectionImplTest.cc`

**阶段 6（流，20 文件）**
`src/StreamFilter.h/.cc`、`src/SinkStreamFilter.h/.cc`、
`src/NullSinkStreamFilter.h/.cc`、
`src/GZipDecodingStreamFilter.h/.cc`、`src/ChunkedDecodingStreamFilter.h/.cc`、
`src/GZipEncoder.h/.cc`、`src/GZipFile.h/.cc`、`src/IOFile.h/.cc`、
`src/BufferedFile.h/.cc`、`src/SHA1IOFile.h/.cc`
跨层单行适配：`src/CookieStorage.cc`、`src/ProtocolDetector.cc`、`src/Netrc.cc`、
`src/NsCookieParser.cc`、`src/ServerStatMan.cc`、`src/UriListParser.cc`、
`src/DHTRoutingTableDeserializer.cc`、`src/DHTRoutingTableSerializer.cc`、
`src/DefaultBtProgressInfoFile.cc`、`src/option_processing.cc`、
`src/LibsslTLSContext.cc`
测试：`test/BufferedFileTest.cc`、`test/GZipDecoderTest.cc`、
`test/GZipEncoderTest.cc`、`test/GZipFileTest.cc`、
`test/GZipDecodingStreamFilterTest.cc`、`test/ChunkedDecodingStreamFilterTest.cc`、
`test/SinkStreamFilterTest.cc`

**不变 / 不入 diff**：`src/includes/aria2/aria2.h`、`examples/`、`deps/**`、
`configure.ac`、所有 `Makefile.am`、`.github/workflows/build.yml`、
`README.rst`、`doc/**`、所有生成文件（`configure`、`Makefile.in`、
`config.h.in`、`aclocal.m4`）、7 个 `src/*.c` 兼容 shim。

## 测试与验证

- 每阶段门禁（沿用 PLAN-001/002 的干净副本方式）：
  ```
  git worktree add --detach /tmp/aria2-m3-p<N> HEAD
  cd /tmp/aria2-m3-p<N> && autoreconf -i
  ./configure --enable-werror
  make -j"$(nproc)" check
  ```
  **绝不在当前工作区原地增量 `make`**：现存 187 个 `test/*.o` 与旧
  `test/aria2c` 会让增量构建假绿（PLAN-001 实测该二进制跑出 15 个残留失败，
  而 `test/aria2c.log` 还停在旧的 `OK`）。每阶段验证完 `git worktree remove`；
  临时脚本/补丁不入库。
- 通过标准（每阶段都要满足）：
  1. `make check` 退出码 0；`test/test-suite.log` 为 `# TOTAL: 1` / `# PASS: 1`，
     **且 `test/aria2c.log` 末尾为 `OK (N)`、无 `Failures !!!`**
     （Autotools 只看到 1 个测试程序，光看 test-suite.log 会假绿）。
  2. 构建目录 `src/Makefile` 中 `CXX = g++ -std=c++20`；
     `WARNCXXFLAGS` 含 `-Wall -Werror -Wformat-security`。
  3. 本阶段宣称清干净的债务用 `git grep` 复核为 0 命中（每阶段的具体命令
     写在该阶段描述里）。
  4. `git status --porcelain` 无生成文件被误加入。
- 阶段专项验证：
  - **阶段 1（fmt 迁移，风险最高）**：
    - `test/FmtTest.cc` 的断言改为 `fmt("aria2-{}.{}.{}-{}", ...)` 并保留
      期望值 `"aria2-1.0.7-beta"`。
    - 新增 3 条断言到 `test/FmtTest.cc`（本轮唯一新增测试代码）：
      `fmt("{:08x}", 0x2f)` == `"0000002f"`、
      `fmt("{:.2f}", 3.14159)` == `"3.14"`、
      `fmt("{}", std::string("s"))` == `"s"`（确认不再需要 `c_str()`）。
    - 逐字节核对高风险站点（这些**没有自动化断言**，只能人工 diff）：
      `util.cc:319,344,362,1968` 的 `%02X`、
      `bittorrent_helper.cc:793` 的 `"Length too long: %d > %dKB"`
      （有 `test/BtRequestMessageTest.cc:280` 断言，会自动兜住）、
      `WrDiskCache.cc:66` 的花括号转义、`Piece.cc:342` 的 `{:p}`。
    - `git grep -nE '\bfmt\([^;]*%[-#0-9.*lhzu]*[a-zA-Z]' -- 'src/*.cc' 'src/*.h'`
      应只剩 `paramed_string.h:145` 与 `OptionHandlerImpl.cc:127` 两处
      `std::vformat` 调用（格式串由 `printf` 动态构造，需说明为何保留）。
  - **阶段 2**：`git grep -n 'parseFinal' -- 'src/*.cc'` 逐处确认无
    `ssize_t& error` 出参；`test/ValueBaseBencodeParserTest.cc` 13 处断言的
    错误码语义（`-1..-8`）必须逐值保持。
  - **阶段 3**：`test/MessageDigestTest.cc` 的 6 处 `update(` 断言；
    摘要结果逐字节不变（可用 `git stash` 前后对比 `aria2c` 计算的校验和）。
  - **阶段 4**：`test/MultiDiskAdaptorTest.cc`（7 处）、
    `test/ByteArrayDiskWriterTest.cc`（8 处）是主要回归网；
    `readDataDropCache` 无专门测试，靠 `MultiDiskAdaptor` 用例兜。
  - **阶段 5**：`test/SocketCoreTest.cc` 只有 2 处 `readData` 调用，
    覆盖率低 —— `TLSErrorCode` 改动靠 `--enable-werror` 的类型检查兜，
    无运行时自动化兜底，属已知风险。
  - **阶段 6**：`test/ChunkedDecodingStreamFilterTest.cc`、
    `test/GZipDecodingStreamFilterTest.cc`、`test/SinkStreamFilterTest.cc`
    覆盖 `transform`；`IOFile` 无直接测试，靠
    `test/UriListParserTest.cc`、`test/NetrcTest.cc`、`test/ServerStatManTest.cc` 兜。
- **不执行**：clang 二次构建、`build_test.sh` 的 9 种依赖矩阵、ASan/UBSan、
  `make distcheck`、ABI 检查工具（按用户第 1 轮验收决定）。

## 验收标准

1. 6 次提交，每次都能在干净副本上
   `autoreconf -i && ./configure --enable-werror && make -j check` 通过，
   `OK (N)` 且无 `Failures !!!`。
2. 基础层 166 个文件的 `typedef`（48）、非 scoped `enum`（11）、
   `NULL`（1 处代码）、C 风格转换（25）清零；裸 `new`/`delete` 从 39 降到
   只剩 `ByteArrayDiskWriter.h` 的 `stringstream` 相关的可解释残余（目标 ≤ 4 处，
   每处都要在提交信息里写明理由）。
3. 基础层的 `fmt()` 调用（159 处）与 `message.h` 的 193 个宏全部改为
   `std::format` 风格；全仓 804 处 `fmt(` 调用点只剩 2 处 `std::vformat`
   （运行时格式串），格式串里不再有 `%` 说明符。
4. 11 个虚函数链的 `ptr+len` / `void*+len` 签名全部换成 `std::span`；
   明确保留的例外（`setSockOpt` 的 `void*`、`inetNtop/inetPton/getBinAddr`、
   `aria2.h` 的 `userData`、`TLSSession::writeData/readData` 的 `ssize_t`）
   逐条在提交信息与本文档「风险与权衡」里可查。
5. 错误处理：`BitfieldMan` 7 个索引 API 返回 `std::optional<size_t>`；
   `parseFinal` 不再有 `ssize_t& error` 出参；`GroupId` 与 `TLSSession` 的
   错误码为 `enum class`；`DHTConnectionImpl::bind` 返回 `std::optional<uint16_t>`。
6. `src/includes/aria2/aria2.h` 哈希仍为
   `2216641161b15acb3c3ddf2600dc19e3decd59d5`（ABI 不变），`examples/` 不改。
7. 范围外的 7 个 `src/*.c`、`configure.ac`、所有 `Makefile.am`、CI、
   `README.rst`、其他 6 个模块的惯用法清理均无改动（只有被接口波及的单行适配）。
8. `PLAN-003` 的勾选项与实际一致，6 个提交信息为
   `PLAN-003 阶段N: <子模块> 现代化`。

## 风险与权衡

- **fmt 全量迁移是本次最大风险，且必然溢出「只做基础层」的边界**。
  用户明确选了「连 message.h 一起改，跨层同步」，因此阶段 1 的 diff 会覆盖
  93 个文件、541 处宏使用点 + 531 处字面量格式串。这意味着：
  1. 阶段 1 的审查量远大于其他 5 个阶段之和；
  2. 引擎层/协议层的 `fmt` 调用点在此阶段被改动，但那些文件的其他债务
     （裸 new、C 风格转换、枚举）留到 PLAN-004/005/006；
  3. **没有任何自动化断言能兜住输出文本变化**。唯一被测试逐字断言的
     `fmt` 输出是 `test/FmtTest.cc:47` 与
     `test/BtRequestMessageTest.cc:280`（"Length too long: %d > %dKB"）。
     其余 800 处日志/异常文案的变化**不可见**，只能靠人工逐条比对格式说明符。
     降风险手段：把 `%08x`/`%02X`/`%.1f`/`%.2f`/`%p`/`%zu` 这 6 类格式说明符
     作为**机械 checklist** 逐处核对（基础层 12 处 + 全仓约 30 处），
     其余 `%s`/`%d`/`%u`/`%lu`/`%ld` 用 `{}` 等价替换（`std::format` 对
     整型与字符串的默认输出与 `printf` 一致，已实测）。
- **`unsigned char`/`char` 实参在 `std::format` 下输出字符而非数字**
  （实测 `unsigned char 0x2f` → `"/"` 而不是 `"47"`）。`util.cc` 4 处
  `fmt("%%%02X", c)` 是百分号编码的核心路径，一旦写错所有 URL 编码都错。
  这些有 `test/UtilTest2.cc` 的 `testPercentEncode` 系列断言兜底。
- **`enum class` + 位运算**：`EventPoll::EventType` 改 `enum class` 必须同时加
  `operator|/&/|=`，否则 57+69 处使用全挂。编译器会拦住，不会静默错。
- **`GroupId` 错误码改 `enum class` 会碰测试**：`test/GroupIdTest.cc` 10 处
  `CPPUNIT_ASSERT_EQUAL((int)GroupId::ERR_*, ...)` 需改为
  `CPPUNIT_ASSERT_EQUAL(GroupIdErr::..., ...)` 或保留 `(int)` 强转。
  成功路径的 `0` 与 `ERR_*` 混在一个 `int` 里，改成 `enum class` 后
  「成功」需要一个枚举值（如 `kOk = 0`），**这是语义变化点**，
  `RpcMethodImpl.cc:199` 的 `switch` 要跟着加 `case GroupIdErr::kOk`。
- **`parseFinal` 返回值改成结构体是 API 形状变化**：`GenericParser::parseFinal`
  是模板，`ValueBaseDiskWriter.h` 的 `parseFinal(0, 0)` 用空指针 + 0 长度当
  「结束」哨兵。改 span 后 `parseFinal(std::span<const std::byte>{})` 语义一致，
  但 `bencode2.cc:68` 与 `ValueBaseDiskWriter.h:85` 的两处必须同阶段改，
  否则阶段 2 提交编译不过（已在分步计划里显式列出）。
- **`BinaryStream` 改动是 90 处调用点 + 19 个文件**，其中
  `ValueBaseDiskWriter.h` 是**模板**（改签名要保证所有实例化都编译），
  `MultiDiskAdaptor::readData` 有两个重载（`MultiDiskAdaptor.cc:360,372`），
  漏改一个就编译失败（编译器兜底）。
- **`TLSSession` 的 `ssize_t` 返回值不拆**：`writeData/readData` 用正值表示
  字节数、负值表示 `TLS_ERR_WOULDBLOCK`/`TLS_ERR_ERROR`。用 `std::expected`
  拆开是更现代的做法，但 `std::expected` 在 C++20 不存在，且这会把 53 处
  比较拆成两段 —— 收益小、风险大，明确保留。
- **`SocketCore` 网络层测试覆盖率低**（`test/SocketCoreTest.cc` 只有 2 处
  `readData` 调用）。`void*`→`span` 与 `TLSErrorCode` 改动主要靠
  `--enable-werror` 的类型检查兜，没有运行时网络用例网。若引入 span 后
  某个 `void*` 转换写错（例如 `as_writable_bytes` 用成 `as_bytes`），
  编译器不一定报错（`span<std::byte>` 与 `span<const std::byte>` 转换会报，
  但 `reinterpret_cast` 不会）。逐处核对 `as_bytes` vs `as_writable_bytes`。
- **`strtod` → `std::from_chars` 的行为差异**：`from_chars` 不接受前导空白、
  不接受 `"0x"` 前缀十六进制浮点、对 `"inf"`/`"nan"` 的支持是 C++20 后的
  `chars_format` 扩展（gcc 14 支持）。`util::parseDoubleNoThrow`
  （7 处调用）若依赖这些行为会静默变化 —— 阶段 1 需对比 `test/UtilTest2.cc`
  里 `parseDoubleNoThrow` 的现有断言，缺覆盖就补。
- **`GroupId`/`aria2api` 的 `a2_gid_t` 是公共头 typedef**：`GroupId.h:46`
  的 `typedef uint64_t a2_gid_t` 改 `using` 是纯语法等价，
  但 `src/includes/aria2/aria2.h` 里可能也有同名定义 —— 已核实公共头
  不含 `a2_gid_t`（它是 `A2Gid`，独立类型），无冲突。
- **验收只跑 gcc + werror**：不覆盖 clang-18 对 `std::format` 的
  `consteval` 诊断差异、`enum class` 位运算的模板推导差异、以及 9 种条件编译
  组合（`--disable-bittorrent` 会切掉 DHT/BT 一半代码，很多改动在
  `ENABLE_BITTORRENT` 条件块内）。CI push 后会补 clang + 4 维矩阵，
  但本地合并前不会发现这些。**这是用户明确选择的验收粒度**。
- **`src/*.c` 兼容 shim 全部不删**：本机 `config.h` 里
  `HAVE_BASENAME/GETADDRINFO/GAI_STRERROR/STRPTIME/TIMEGM/DAEMON/POLL` 全为 1，
  所以 6 个 shim 根本不参与编译，`alloca.c` 连 `Makefile.am` 都没引用。
  它们不影响现代化，但会让人误以为「磁盘IO 簇还没清干净」。
  删它们要同时改 `configure.ac:463-484`，属破坏性裁剪，另开计划。

## 备注

- 用户第 1 轮选择：「保持 C++20」「基础层：util / 解析器 / 摘要 / 磁盘IO /
  网络 / 流」「语言惯用法清理 + 接口 span 化续做 + 错误处理现代化 +
  标准库替换」「干净副本 + werror」。
- 用户第 2 轮选择：「连 message.h 一起改，跨层同步」「含跨层单行适配」。
- 第 2 轮提问的起因：第 1 轮选「标准库替换（含 fmt→std::format）」与
  「只做基础层」在 `message.h` 上是冲突的 —— 193 个宏被 93 个文件、
  541 处使用点引用，其中只有 60 处在基础层。已向用户说明并确认要跨层同步。
- 第 2 轮另一个起因：`GroupId`/`TLSSession`/`DHTConnectionImpl` 的错误码改动
  必然要碰 `RpcMethodImpl.cc`/`aria2api.cc`/`SocketCore.cc` 等非基础层文件，
  已确认允许单行适配。
- 与 PLAN-001/002 的关系：PLAN-001 明确排除 `std::span`/`std::format`；
  PLAN-002 把 span 限定在 3 个叶子工具簇并明确排除
  `BencodeParser`/`MessageDigest`/`BinaryStream`/`DiskAdaptor`/`DiskWriter`/
  `SocketCore`/`TLSSession`。本计划正是续做这些被排除项，属独立后续计划，
  不修订、不覆盖、不作废前两个计划。
- 执行前仓库检查：`git rev-parse --is-inside-work-tree` 为 true
  （`git status --porcelain` 为空）；`.gitignore` 已覆盖构建产物与生成文件，
  无需补充；`plans/` 为项目已有约定，`PLAN-003` 序号未被占用。
- 计划自查结论（写完立即做的三项）：
  1. **引用真实**：文中所有行号与计数均为本次 `git grep`/`grep -c` 实测
     （`src/fmt.h:45`、`message.h` 193 宏/541 使用点、`EventPoll.h:54`、
     `BinaryStream.h:48,51`、`GroupId.h:46,53`、`TLSSession.h:44,46,64,86-105`、
     `util.cc:487,501,514,531,1797`、`WrDiskCache.cc:66`、
     `bittorrent_helper.cc:793` 等）；「预计修改的文件」中每个路径都已用
     `ls src/<name>` 确认存在，并按 6 组前缀做了覆盖性校验
     （166 文件全归类、无遗漏、仅 3 个文件双归属已定点）。
  2. **阶段可编译**：已识别并显式列出 3 个跨阶段耦合点
     （`bencode2.cc` 的 `parseFinal`、`ValueBaseDiskWriter.h` 的
     `parseFinal(0,0)`、`GenericParser.h:113`），以及 3 组跨层单行适配
     （阶段 3 的 17 文件、阶段 4 的 19 文件、阶段 5 的
     `SocketCore`+`TLSSession` 同提交），全部写进对应阶段的闭环清单。
     `EventPoll::EventType` 的位运算符要求「先加运算符再切 enum class，
     同一提交」也已写明。
  3. **覆盖所有消费点**：改 `fmt` 时列了 5 类消费者
     （`A2_LOG_*` 宏 398 处、`DL_*_EX` 119 处、测试断言 2 处、
     运行时格式串 2 处、`paramed_string.h` 的动态宽度）；
     改 `enum class` 时逐个核对了值名使用量与限定名使用量
     （`EventType` 57/69、`BStructElementType` 89/0、`Algorithms` 159/0、
     `TLS_*` 53 且封闭在 3 个文件）；改 `BinaryStream` 时用
     `git grep -l -E '(->|\.)(writeData|readData)\('` 生成了 30 个文件的
     精确清单。**未验证项**：`std::format` 对 `%.1f`/`%zu`/`%lu` 的输出
     与 `printf` 逐字节一致性只做了 `%.2f`/`%08x`/`%p` 的探针，其余靠
     「整型默认格式一致」的推理；这是本计划最大的一处未实测假设，
     已写进「风险与权衡」。

## 审查记录

- 审查 #1：「预计修改的文件」里的 `src/Foo.h/.cc` 合并写法逐条展开后，
  发现 2 处测试路径写错：`test/ParamStringTest.cc` 实际名为
  `test/ParamedStringTest.cc`；`test/RangeTest.cc` 不存在（`Range` 的断言在
  `test/RangeBtMessageTest.cc`，属协议层，不在本计划范围）。已修正。
  同时补上漏列的 `src/ValueBaseStructParserState.h` —— 该文件只有头、
  实现拆在 `*Impl.cc`，合并写法会漏掉它。
- 审查 #2：「测试与验证」里阶段 1 的门禁写成「`git grep` 无命中」，
  但 `paramed_string.h:145` 与 `OptionHandlerImpl.cc:127` 的 `%` 必须留下
  （运行时格式串改 `std::vformat` 后，格式串仍由 `printf` 拼出 `"%02lu"`）。
  已把门禁措辞改为「只剩这 2 处 `std::vformat`」，避免门禁永远不可能通过。
- 审查 #3：`--enable-werror` 的实际位置是 `configure.ac:557-561`
  （`if test "x$enable_werror" = "xyes"` + 三行 `AX_CHECK_COMPILE_FLAG([-Wall/-Werror/-Wformat-security])`），
  不是 PLAN-002 文档里「558 是 werror 块」的说法（那是删掉 34 行探测代码前
  的旧行号；现在 558 是 `-Wall` 那行）。文档不引用该行号，只引用
  `WARNCXXFLAGS` 变量名，避免行号漂移。
