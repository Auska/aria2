# 任务：引擎层与基础层剩余文件现代化（全量口径）
> 状态：✅ 已完成（2026-11-08）

## 目标

把协议层（PLAN-004，356 文件）和配置层（PLAN-006 预定，80 文件）之外的
全部 `src/*.h` / `src/*.cc` 纳入一次收口式现代化：415 文件 / 66 073 行 /
232 个 `.h`。四项改造全做（语言惯用法、span 化、错误处理、标准库替换），
并专项清掉 PLAN-003 留下的 `BitfieldMan` 裸 `new[]`/`delete[]` 债务。
调用点允许完整清理，不限制在单行适配。

## 当前上下文

### 仓库与前置状态

- 仓库 `/home/code/tmp/aria2`，分支 `slim-features`，HEAD `8012e338`
  （PLAN-004 收尾），`git status --porcelain` 为空。`plans/` 已有
  PLAN-001~004，序号 005 未占用。
- `configure.ac:111` 是 `AX_CXX_COMPILE_STDCXX([20], [noext], [mandatory])`；
  `configure.ac:557-566` 的 `--enable-werror` 展开为
  `WARNCXXFLAGS = -Wall -Werror -Wformat-security`。本机 `g++ 14.2.0`、
  `autoconf 2.72`、`automake 1.17`、`cppunit 1.15.1`。
- PLAN-003 六次提交（`132bb4ad`…`9cc4e690`）已完成基础层 6 簇；PLAN-004
  四次提交（`03fae183`/`d2688db0`/`410a809f`/`8b369364`）已完成协议层 4 簇。
  两者共用门禁：干净 `git worktree` + `autoreconf -i` +
  `./configure --enable-werror` + `make -j check`，判据 `test/aria2c.log`
  `OK (914)`、`test/test-suite.log` `# TOTAL: 1` / `# PASS: 1`、无 `Failures !!!`。
- 工作区根目录残留 `config.h`、`Makefile`、`configure` 与旧构建产物；
  **原地增量 `make` 会假绿**（PLAN-003/004 实测），所有验证必须在干净副本。

### 模块范围（415 文件 / 66 073 行 / 232 个 `.h`）

可复现口径（已用 `git ls-files` + 集合运算校验 `rest == /tmp/p5_engine_all.txt`）：

```
all   = git ls-files 'src/*.h' 'src/*.cc' 'src/*.c'                    = 858
proto = PLAN-004 四组前缀（NET 48 / DHT 116 / HTTP 53 / BT 139）          = 356
cfg   = /tmp/cfg.txt（PLAN-006 预定：Option/Rpc/aria2api/main/prefs 等）    =  80
shims = src/*.c（7 个 C 兼容实现）                                        =   7
engine = all - proto - cfg - shims                                      = 415
```

- 415 里**没有** `src/includes/**`、没有 `.c`，与 proto356、cfg80 无交集。
- 其中 **179 个文件在 PLAN-003「预计修改的文件」清单里**（只做过
  `fmt()` 迁移/单行适配），236 个从未纳入 PLAN-003。用户确认取全量口径，
  即「415 债务清零」而非「排除已 touch 的 179 个」；这与 PLAN-004 第 3 轮
  「356 全量」的决策一致。
- PLAN-004 备注写「PLAN-005 引擎层 208 文件 / 26 187 行」是历史粗分估计：
  该 208 有 43 个 `.h`/`.cc` 单后缀裂缝、与 proto 重叠
  `src/NullWebSocketSessionMan.h`，且把 `src/Piece.h` 与 `src/Piece.cc`
  拆到两侧。本计划**用 415 全量口径替代它**，不沿用 208。
- 415 与 cfg80 有 6 组共享 stem（`src/Option`、`src/OptionHandlerImpl`、
  `src/RpcMethodImpl`、`src/UriListParser`、`src/aria2api`、
  `src/download_helper`）—— 每组的 `.cc` 在 415、`.h` 在 cfg80。
  `.h` 属 PLAN-006，本计划只在「完整清理调用点」需要时改它，不主动 modernization。

### 四簇划分（按依赖自底向上，415/415 无遗漏、无重叠）

| 阶段 | 子模块 | 前缀/内容 | 文件 | `.h` | 行数 |
|---|---|---|---|---|---|
| 1 | A-基础底座 | `util*`/`a2*`/`array_fun`/`fmt`/`message*`/`GroupId`/`Range`/`Signature`/`prefs`/`Option.cc`/`paramed_string`/`base32`/`base64`/`bencode2`/`bitfield`/`crypto_hash`/`MessageDigest*`/`ValueBase*`/`BencodeParser`/`JsonParser`/`UriListParser.cc`/`version_usage`/`util_security` 等 | 67 | 38 | 14 419 |
| 2 | B-IO 与事件 | `AbstractDiskWriter`/`*DiskAdaptor*`/`*DiskWriter*`/`*FileAllocation*`/`BufferedFile`/`*StreamFilter*`/`GZip*`/`IOFile`/`*EventPoll*`/`Socket*`/`TLS*`/`WrDiskCache*`/`AsyncNameResolver*`/`DefaultExtensionMessageFactory`/`ARC4Encryptor.h` 等 | 94 | 52 | 13 259 |
| 3 | C-Peer/Piece/RequestGroup | `Peer*`/`Piece*`/`Segment*`/`BitfieldMan`/`DefaultPieceStorage`/`RequestGroup*`/`DownloadContext*`/`CheckIntegrity*`/`Checksum*`/`Iteratable*`/`FileEntry`/`*URISelector`/`*PieceSelector`/`SeedCriteria`/`ServerStat*`/`NetStat`/`SpeedCalc`/`TransferStat`/`ConsoleStatCalc`/`Context*`/`DNSCache`/`IndexedList`/`SegList` 等 | 112 | 63 | 20 122 |
| 4 | D-命令/引擎/编排 | 其余全部：`AbstractCommand`/`*Command*`/`DownloadEngine*`/`Request*`/`File`/`Exception`/`RpcMethodImpl.cc`/`aria2api.cc`/`download_helper.cc`/`option_processing.cc`/`Sqlite3CookieParser*`/`ProtocolDetector`/`GrowSegment`/`UnionSeedCriteria` 等 | 142 | 79 | 18 273 |

清单文件在 `/tmp/p5_{A,B,C,D}.txt`（临时文件，不入库）；可由上述规则重算。
顺序理由：A 的 `util.h`/`fmt.h`/`message.h`/`bitfield.h` 被其余三簇与
proto/cfg 大量 include；B 的 `SocketCore.h`/`TLSSession.h` 是 C/D 的依赖；
C 的 `Peer.h`/`Piece.h`/`RequestGroup.h` 是 D 命令层与 proto 层的核心契约；
D 最后动可把跨层调用点一次性收口。

### 债务实测（剥注释后正则统计，415 全量）

| 指标 | 命中 | 文件数 | 说明 |
|---|---|---|---|
| `typedef` | 43 | 19 | 含 `src/IndexedList.h` 的 iterator traits 5 个必需 typedef |
| 非 scoped `enum` | 13 | 12 | 真实行号：`src/AbstractOptionHandler.h:124` `Flag`、`src/Command.h:46` `STATUS`、`src/ContextAttribute.h:46` `ContextAttributeType`、`src/InitiatorMSEHandshakeCommand.h:51` `Seq`、`src/NetStat.h:47` `STATUS`、`src/PeerInteractionCommand.h:52` `Seq`、`src/ReceiverMSEHandshakeCommand.h:48` `Seq`、`src/RequestGroup.h:80` `HaltReason`、`src/RequestGroup.h:81` `State`、`src/RequestGroupMan.cc:650` `DownloadResultStatus`、`src/ServerStat.h:53` `STATUS`、`src/ServerStatMan.cc:121` `Field`、`src/SocketCore.cc:90` `TlsState`（匿名 namespace，B 簇）；按簇 C 8 / D 4 / B 1 |
| 裸 `new` | 14 | 4 | 真正裸分配：`src/BitfieldMan.cc` 9 处 `new unsigned char[...]`、`src/LibuvEventPoll.cc:87,119,221`、`src/prefs.cc:65,94` 与 `src/prefs.cc:110 deletePrefResource()`、`src/Piece.cc:349` 的 `WrDiskCacheEntry::DataCell`（由 `wrCache_->cacheData(cell)` 接管所有权）；3 处是 `src/message.h:25,66,69` / `src/TrackerWatcherCommand.cc:260` 格式串里的单词 "new" 误命中 |
| `delete` / `delete[]` | 46 / 7 | 22 / 1 | 多数是 `e_->deleteSocketForReadCheck(...)` 这类函数名误命中，真实裸释放集中在 `src/BitfieldMan.cc`（7 处 `delete[]`）、`src/prefs.cc`、`src/LibuvEventPoll.cc:59` |
| `stringstream` | 15 | 12 | `src/ConsoleStatCalc.cc:246`、`src/TrackerWatcherCommand.cc:109`、`src/Exception.cc:102`、`src/RequestGroupMan.cc:806,827,838` 等 |
| `std::find(_if)` | 37 | 15 | 换 `std::ranges::find`（C++20 无 `ranges::contains`） |
| `strtol/strtod/sscanf` | 9 | 4 | 换 `std::from_chars` |
| C 风格转换 | 46 | 10 | 换 `static_cast`/`reinterpret_cast` |
| `std::endl` | 1 | 1 | 换 `'\n'` |
| `.c_str()` | 414 | 61 | 大多喂 `fmt()`/`std::string` API，不强行 span 化，只清可证明冗余的 |
| `printf` | 64 | 7 | `src/OutputFile.h:51-52` 的 `virtual int vprintf(const char* format, va_list va) = 0` / `inline int printf(const char* format, ...)` 是接口；`src/Context.cc:102-117,270`、`src/RequestGroupMan.cc:702,715,753`、`src/option_processing.cc`(20)、`src/version_usage.cc`(31)、`src/util.h`(2)、`src/ConsoleStatCalc.cc` 是调用点 |
| ptr+len 形参/声明 | 180 | 72 | Peer/Piece 链 span 化的主战场（`BitfieldMan`/`PieceStorage`/`PieceSelector`/`PeerConnection`/`Exception`） |
| `fmt(` | 409 | 69 | **`src/fmt.h:46-49` 已是 `std::format` 包装**（PLAN-003 阶段 1 全量迁移完毕）；本计划**不再做 fmt→std::format**，只清 `stringstream`/`strtod`/`find`/`endl` |

### 已确认决策

用户第 1 轮（4 问）：

| 决策点 | 选项 | 用户选择 | 状态 |
|---|---|---|---|
| 模块范围 | 精确 250 / **全部剩余 415 文件** / 历史 208 / 只做 Peer-Piece 45 | **全部剩余 415 文件** | 已确认 |
| 改造内容 | **四项全做 + BitfieldMan 专项** / 四项保留 BitfieldMan / 惯用法+stdlib+错误处理 / 只惯用法+stdlib | **四项全做 + BitfieldMan 专项** | 已确认 |
| 跨层边界 | 仅单行适配 / 完全不动 / **允许完整清理调用点** | **允许完整清理调用点** | 已确认 |
| 阶段与门禁 | **4 簇 + 干净 worktree 门禁** / 4 簇+clang / 3 簇合并 / 单次提交 | **4 簇 + 干净 worktree 门禁** | 已确认 |

用户备注/约束（必须写进计划）：范围取全量口径，含 PLAN-003 已 touch 的 179 个；
BitfieldMan 的裸 `new[]`/`delete[]`（实测 9 处 `new unsigned char[]` +
7 处 `delete[]`，历史沿用「14 处」）必须 vector 化并闭环 PLAN-003 实际偏差
第 1 条；调用点允许完整清理，即协议层 356 / 配置层 80 / `test/**` 都可以随签名
一起改；验收沿用 PLAN-004 的干净 worktree + `--enable-werror` + `make check`。

## 待确认决策

无待确认项。

## 提议的方法

### 提交顺序与阶段划分

按 A → B → C → D 四阶段，每阶段一次提交 `PLAN-005 阶段N: <子模块> 现代化`；
计划收尾单独一次提交（勾选、实测结果、状态行）。

### span 规则（沿用 PLAN-002/003/004，不发明新约定）

- 输入 `std::span<const std::byte>`、输出 `std::span<std::byte>`；
  调用方显式 `std::as_bytes(std::span{p, n})` / `std::as_writable_bytes(...)`，
  **绝不依赖隐式构造**。`src/SocketRecvBuffer.h:66` 实测是
  `const unsigned char* getBuffer() const { return pos_; }`，`:68` 是
  `size_t getBufferLength() const { return last_ - pos_; }`。
- 消息静态 `create()` 收 `std::span<const std::byte> data`，长度用 `data.size()`；
  span 不持有所有权，成员裸指针语义不变。
- PLAN-004 明确留给 PLAN-005 的 Peer/Piece 链（`src/Peer*`、
  `src/PieceStorage.h`、`src/PieceSelector.h`、`src/BitfieldMan.*`、
  `src/DefaultPieceStorage.*`）在阶段 3 收口；协议层调用点随本计划完整清理。
- 已知边界：`src/SocketRecvBuffer::getBuffer()` 保持 `const unsigned char*`
  （PLAN-003 实测改 `const std::byte*` 会让 `src/DownloadCommand.cc:184,192`
  编译失败）；`src/bittorrent_helper` 的 `(ptr, offset)` 整数读写、
  `packcompact`/`unpackcompact` 与 `BtPieceMessage::setMsgPayload` 保持原样
  （PLAN-004 有意保留，线上字节格式，100+ 调用点）。

### BitfieldMan 专项（阶段 3）

现状（`src/BitfieldMan.h:53-55` 实测三个 `unsigned char*`）：

```cpp
unsigned char* bitfield_;
unsigned char* useBitfield_;
unsigned char* filterBitfield_;
```

`src/BitfieldMan.cc:65,66,78,79,91,107,111,116,676` 共 9 处
`new unsigned char[]` 与 `:106,110,114,131,132,133,742` 共 7 处 `delete[]`
（合计 16 个操作，PLAN-003 偏差①沿用「14 处」编号）；`src/BitfieldMan.cc:178,182,191,196,205,207,296,301,345,350,
404,410,422,428,453,457,471,476,492,498,510,794` 用 `expr::array(bitfield_)` 做位运算。

做法：三个成员改 `std::vector<unsigned char>`（`filterBitfield_` 的「未启用」
语义用空 vector 表示，等价原 `nullptr`），`~BitfieldMan`/`operator=`/拷贝构造
改为默认或 vector 赋值；`getBitfield()` / `getBitfieldLength()` /
`getFilterBitfield()` 保持返回原类型（用 `.data()` / `bitfieldLength_`；
`src/BitfieldMan.h:215,217,284` 实测分别是
`const unsigned char* getBitfield() const { return bitfield_; }`、
`size_t getBitfieldLength() const { return bitfieldLength_; }`、
`const unsigned char* getFilterBitfield() const { return filterBitfield_; }`），
**不改公共 ABI**；`expr::array()` 调用点改 `expr::array(v.data())`，或给
`src/array_fun.h` 的 `Array` 增加一个 `std::vector` 适配（取更小者）。
`src/bitfield.h:100` `inline size_t countSetBit(const unsigned char* bitfield, size_t nbits)`
与 `:145` `void flipBit(unsigned char* data, size_t length, size_t bitIndex)` 等
整族收裸指针的签名保持不变，只传 `.data()`；`src/bitfield.h:128` 还有
`size_t countSetBitSlow(const Array& bitfield, size_t nbits)` 模板。

必须同提交闭环的消费点（已 grep 实测）：

- `getBitfield()` 34 个文件：协议层 `src/BtBitfieldMessage.*`、
  `src/BtHaveAllMessage.cc`、`src/BtPieceMessage.cc`、
  `src/DefaultBtMessageFactory.cc`、`src/DefaultBtProgressInfoFile.cc`、
  `src/Peer.cc/.h`、`src/PeerInteractionCommand.cc`、
  `src/PeerSessionResource.cc/.h`、`src/DefaultPieceStorage.cc/.h`、
  `src/Piece.cc/.h`、`src/PieceStorage.h`、`src/RequestGroup.cc`、
  `src/UnknownLengthPieceStorage.cc/.h`、`src/Iteratable*Validator.cc`、
  `src/RpcMethodImpl.cc`、`src/aria2api.cc`；
  测试 `test/BitfieldManTest.cc`、`test/BtBitfieldMessageTest.cc`、
  `test/DefaultBtProgressInfoFileTest.cc`、`test/LongestSequencePieceSelectorTest.cc`、
  `test/MockPieceStorage.h`、`test/PriorityPieceSelectorTest.cc`、
  `test/RarestPieceSelectorTest.cc`、`test/UtilTest2.cc`。
- `getFilterBitfield()` 3 个文件：`src/SegmentMan.cc:206,223`、
  `test/BitfieldManTest.cc:300-324`（8 条断言）。
- `src/includes/aria2/aria2.h:749` 的 `virtual std::string getBitfield() = 0`
  是同名但不同类（`BtProgressInfoFile`），**不要误改**；该公开头只有这一处
  `getBitfield`，且
  `2216641161b15acb3c3ddf2600dc19e3decd59d5` 必须保持不变。

### 枚举（13 个全改 `enum class`）

- 全仓 `Seq::` 103 次、`STATUS_` 56 次、`STATE_` 36 次、`FLAG_` 21 次；
  改 `enum class` 会波及比较、位运算、数组下标与 `static_cast<int>`
  （`src/ServerStat.h:53` `enum STATUS { OK = 0, A2_ERROR, MAX_STATUS };` 用作数组下标）。
  每个枚举的调用点在**同一阶段**内闭环，不跨阶段留半成品。
- `src/ContextAttribute.h:46` `ContextAttributeType { CTX_ATTR_BT, MAX_CTX_ATTR }`
  配 `strContextAttributeType(ContextAttributeType key)`；`src/Command.h:46`
  `STATUS` 与 `src/NetStat.h:47` `STATUS`、`src/SocketCore.cc:90` `TlsState`
  同名/同形态但各自独立处理（`TlsState` 在匿名 namespace，赋值目标是 `int secure_`，
  `src/SocketCore.cc:140,833,838,859,863,904` 用 `A2_TLS_NONE`/
  `A2_TLS_HANDSHAKING`/`A2_TLS_CONNECTED`）。
- 位掩码枚举沿用 PLAN-004 做法：必要时 `static_cast`，`operator&` 返回 bool。

### 错误处理

只改「`bool` + 出参 / `int` 错误码 / `ssize_t& error`」三类且能在本阶段闭环
的局部逻辑，改 `std::optional` / `enum class` / 小结果结构体。公共虚函数签名
（`src/Command.h`、`src/PieceStorage.h`、`src/RequestGroup.h`）如确需改动，
必须同提交改全 258 个层外 include 方中的实际调用点。

### 标准库替换

- `stringstream` → `fmt()` / `std::spanstream`；`strtol/strtod/sscanf` →
  `std::from_chars`；`std::find(_if)` → `std::ranges::find`；`std::endl` → `'\n'`。
- 保持 C++20：`std::expected`/`std::print`/`std::to_underlying`/`std::byteswap`/
  `views::zip`/`views::enumerate` 都需 C++23，`std::ranges::contains` 在
  g++14 与 clang++-22 的 C++23 下都缺 —— 一律用 `std::ranges::find(x) != std::end(x)`。
- **不做** `fmt`→`std::format`：`src/fmt.h:46-49` 已经是
  `template <typename... Args> std::string fmt(std::format_string<Args...> f, Args&&... args)`
  包装 `std::format`，`src/fmt.h:57-60` 的 `fmtRuntime(const std::string& f, Args&&... args)`
  包装 `std::vformat`；409 处 `fmt(` 调用点
  只在需要兼容新签名时适配。

### 跨层「完整清理」边界

415 之外有 **258 个文件** include 了 415 的头（proto 220 / cfg 38），
top 命中 `util.h` 100、`fmt.h` 89、`DlAbortEx.h` 59、`a2functional.h` 58、
`message.h` 44、`Peer.h` 43、`prefs.h` 42、`SocketCore.h` 38、
`DownloadEngine.h` 37、`DownloadContext.h` 30。用户允许完整清理调用点，
即这些文件的惯用法也可随签名一起改；**但**：

- 不重开 PLAN-004 的「协议层 356 全量现代化」验收，只改因本计划签名/枚举/
  类型变化必须改的点，以及同文件里顺手的同类型债务。
- `src/includes/aria2/aria2.h` 是公开 ABI，哈希必须不变。
- `src/*.c` 的 7 个 C shim、`deps/**`、`configure.ac`、`src/Makefile.am`、
  `test/Makefile.am`、`.github/workflows/build.yml`、`README.rst` 不在范围。
- 条件编译分支：415 里有 96 个文件位于 `src/Makefile.am` 的 `if/endif` 块内
  （`ENABLE_BITTORRENT` 57、`HAVE_ZLIB` 8、`ENABLE_LIBARIA2` 5、`HAVE_OPENSSL` 4、
  `HAVE_SQLITE3` 4、`ENABLE_ASYNC_DNS` 4、`USE_INTERNAL_MD` 3、
  `HAVE_SOME_FALLOCATE` 2、`HAVE_EPOLL` 2、`ENABLE_SSL` 2、`HAVE_POLL` 2、
  `HAVE_LIBUV` 2、`USE_OPENSSL_MD` 1）。本机 `config.h` 实测
  `ENABLE_ASYNC_DNS` / `ENABLE_BITTORRENT` / `ENABLE_SSL` / `ENABLE_WEBSOCKET` /
  `HAVE_SQLITE3` / `HAVE_SQLITE3_OPEN_V2` 为 1，**没有** `ENABLE_LIBARIA2`、
  `HAVE_LIBUV`、`USE_INTERNAL_BIGNUM`、`USE_INTERNAL_ARC4`、`USE_INTERNAL_MD`。
  这些分支编译器兜不住，须人工逐行核对签名（如 `src/Makefile.am:568` 的
  `ApiCallbackDownloadEventListener.cc` 块、`src/LibuvEventPoll.cc:119,221`、
  `src/InternalMessageDigestImpl.cc`）。

## 分步计划

1. [x] **阶段 1 — A-基础底座（67 文件）**：`src/array_fun.h:128`
   `template <typename T> Array<T> array(T* t) { return Array<T>(t); }` 与
   `src/a2iterator.h`/`src/a2functional.h` 的 iterator traits `typedef`→`using`；
   `NULL`→`nullptr`、C cast→`static_cast`、迭代器循环→range-for；
   `stringstream`/`strtod`/`std::find`/`std::endl` 替换（含
   `src/version_usage.cc` 的 1 处 `std::endl`）；`src/bitfield.h:100,145`
   等裸指针算法族保持签名，为阶段 3 的 `.data()` 调用预留；
   `src/prefs.cc:65,94,110` 的裸 new 保留（`PrefPtr` 是 `const Pref*`，偏差②）；
   不改 `src/fmt.h` 的 `std::format` 包装。改动波及的 proto/cfg 调用点完整清理。
   门禁通过后提交。
   - **实测（2026-11-08）**：8 处 `typedef`→`using`（`src/array_fun.h:65,94,115`、
     `src/ByteArrayDiskWriterFactory.h:43`、`src/crypto_hash.cc:81`、`src/util.cc`
     三个 `typedef enum` 去类型名）；6 处 C cast→`static_cast`（`src/util.cc:133,638,
     647,1740`、`src/crypto_hash.cc:119,144`）；20 处 `std::find(_if/_if_not)`→
     `std::ranges::`；8 处 strto* 清零（`src/Option.cc` 三处改 `std::from_chars`、
     `src/OptionHandlerImpl.cc` 两处改 `util::parseDoubleNoThrow`、`src/util.cc`
     `parseIntegral` 非十进制慢路径与 `getNumericNameInfo` 改 `std::from_chars`）；
     `src/version_usage.cc:81` 的 `std::endl`→`"\n"`；`src/bencode2.cc` 的
     `std::ostringstream`→`std::string` 累加；`ParameterOptionHandler::
     createPossibleValuesString()` 改 `strjoin`（删 `<sstream>`）。
   - **可解释残余（`shortcut:` 注释）**：`src/ByteArrayDiskWriter.h:47` 的
     `std::stringstream buf_`（seekp/seekg 语义）、`src/UriListParser.cc:73`
     （`OptionParser::parse` 收 `std::istream&`，`std::ispanstream` 是 C++23）、
     `src/BencodeParser.cc:47` 与 `src/JsonParser.cc:47` 的状态机枚举（~100 处
     引用 + `std::stack<int>`）、`src/util.cc` 三个 `CD_*` 枚举（85 处，已去
     `typedef` 类型名）。`src/GroupId.cc:54,64` 裸 new（私有构造）、
     `src/prefs.cc:65,94,110`（`PrefPtr` 是 `const Pref*`）、
     `src/OptionHandlerImpl.cc:675`（deleter 语义）按计划保留。
2. [x] **阶段 2 — B-IO 与事件（94 文件）**：`AbstractDiskWriter`/`SocketCore`/
   `*EventPoll*`/`*StreamFilter*`/`WrDiskCache*` 的 ptr+len 接口按 span 规则改；
   `src/SocketRecvBuffer::getBuffer()` 保持 `const unsigned char*`；
   `src/SinkStreamFilter::dataCopy` 保持 `std::unique_ptr<unsigned char[]>`
   （PLAN-003 偏差④，改 vector 需连 `WrDiskCacheEntry::DataCell` 一起改，收益为零）；
   `src/LibuvEventPoll.cc:119,221` 与 `:59` `delete reinterpret_cast<T*>(handle);`
   的裸 new 保留（libuv 回调 delete，本机不编译）；`src/SocketCore.cc:90`
   的 `TlsState` 在本阶段 enum class 化（B 簇，不是阶段 4）；
   `src/GZipEncoder`/`src/GZipDecodingStreamFilter` 保持
   `z_stream strm_ + bool initialized_`（偏差⑧）。同步改测试
   `test/*StreamFilterTest.cc`、`test/SocketCoreTest.cc` 等。门禁通过后提交。

   实测（2026-11-08）：`src/Event.h:215,227,244,253` 4 处 `std::find` →
   `std::ranges::find` 并收敛为 `auto i =`；`src/SocketCore` 的 `TlsState`
   改 `enum class` 并移入 `SocketCore` 类内（`src/SocketCore.h:79-88`），
   `secure_` 由 `int` 改为 `TlsState`（`:89`），9 处使用点改 `TlsState::*`、
   3 处 `if (!secure_)` 改 `if (secure_ == TlsState::NONE)`；
   `src/LibsslTLSContext.cc` 5 个裸指针 `typedef` → `using` 与
   `std::stringstream`→`std::ostringstream`；`src/FileAllocationMan.h:45`
   typedef → `using`；`src/GZipFile.cc:107,125` 与 `src/LibuvEventPoll.cc:102,130`
   的 C cast → `static_cast`。
   `src/SocketCore.cc` 的 `(a2_sockopt_t)&x` **保持 C cast**：`a2_sockopt_t`
   是 `#define a2_sockopt_t void*`（`src/a2netcompat.h:40`），宏替换后
   `static_cast<void*>(&x)` 语法不成立 —— 属可解释残余。
   `src/WrDiskCacheEntry.h:82 append(int64_t, const unsigned char*, size_t)`
   的 span 化留到阶段 3（唯一调用点 `src/Piece.cc:369` 在 Peer/Piece 链）。
3. [x] **阶段 3 — C-Peer/Piece/RequestGroup（112 文件）**：**BitfieldMan 专项**
   （三个成员 vector 化、`expr::array` 调用点、34 个 `getBitfield()` +
   3 个 `getFilterBitfield()` 消费点、8 条 `test/BitfieldManTest.cc` 断言）；
   Peer/Piece 链 span 化（PLAN-004 留的债）；`src/Piece.cc:349`
   `WrDiskCacheEntry::DataCell` 裸 new 按所有权语义处理；
   `src/RequestGroup.h:80-81` `HaltReason`/`State` 等 C 簇 8 个 enum class 化
   并闭环全部比较/下标调用点；`src/IndexedList.h:52,55,60-66,313-321` 的
   15 处 typedef（含 5 个 `iterator_category`/`value_type`/`pointer`/
   `reference`/`difference_type`）换 `using`。`src/Context.cc`、
   `src/ConsoleStatCalc.cc`、`src/RequestGroupMan.cc`、`src/ServerStatMan.cc`、
   `src/NetStat.h`、`src/ServerStat.h`、`src/ContextAttribute.h` 均属 C 簇。
   实测（2026-11-08）：`typedef` 25 处 / 9 文件全部换 `using`
   （`src/IndexedList.h:52-66,313-322` 的 iterator traits 用
   `using SelfType = IndexedListIterator;` 注入模板参数）；
   `std::find`→`std::ranges::find` 10 处（`src/AdaptiveURISelector.cc:93`、
   `src/FeedbackURISelector.cc:82`、`src/FileEntry.cc:320,441,503,505`、
   `src/Piece.cc:290,295`、`src/SegmentMan.cc:378,441`）；
   C cast→`static_cast` 14 处；`src/RequestGroupMan.cc:142,145` 的 `strtod`
   → `util::parseDoubleNoThrow`。
   **BitfieldMan 专项**：`src/BitfieldMan.h:53-55` 三个
   `unsigned char*` 成员改 `std::vector<unsigned char>`
   （`getBitfield()` 返回 `bitfield_.data()`，`getFilterBitfield()` 在
   `filterBitfield_.empty()` 时返回 `nullptr` 保持旧契约），
   `src/BitfieldMan.cc` 的 9 处 `new unsigned char[]` + 7 处 `delete[]` 全消失，
   28 处 `expr::array(x)` → `expr::array(x.data())`，`bitfield.h` 整族签名不变。
   **span 化**：`src/WrDiskCacheEntry.h:84 append(int64_t, std::span<const std::byte>)`
   与 `src/Piece.h:192 appendWrCache(WrDiskCache*, int64_t, std::span<const std::byte>)`，
   调用点 `src/Piece.cc:369`、`src/SinkStreamFilter.cc:77` 与
   `test/PieceTest.cc:111`、`test/WrDiskCacheEntryTest.cc:64,68` 同步改。
   **枚举**：C 簇 8 个全部 enum class 化 —— `src/NetStat.h:47 STATUS`、
   `src/PeerInteractionCommand.h:52 Seq`、`src/RequestGroup.h:80 HaltReason`
   与 `:81 State`（`getState()`/`setState()` 由 `int` 改 `State`）、
   `src/ServerStat.h:53 STATUS`（`STATUS_STRING[static_cast<size_t>(...)]`）、
   `src/ContextAttribute.h:46 ContextAttributeType`（`CTX_ATTR_BT`→`BT`、
   `MAX_CTX_ATTR`→`MAX`，23 文件 60+ 处调用点 + `static_cast<size_t>` 下标）、
   `src/RequestGroupMan.cc:650 DownloadResultStatus`（`A2_STATUS_*`→`OK/INPR/RM/ERR`）、
   `src/ServerStatMan.cc:121 Field`、`src/PeerConnection.h:57 MsgState`。
   `MAX_FIELD`/`Field::HOST` 改名 `Field::HOSTNAME`：`Field::HOST` 与
   `config.h:411` 的 `#define HOST "x86_64-pc-linux-gnu"` 宏冲突。
   可解释残余：`src/Piece.cc:349` `new WrDiskCacheEntry::DataCell()`
   （`DataCellSet` 是 `std::set<DataCell*>`、`cacheData` 收裸指针、
   `test/TestUtil.h:51` 返回裸指针）、`src/ConsoleStatCalc.cc:246` 与
   `src/RequestGroupMan.cc:806,827,838` 的 `std::stringstream`
   （共用 `std::ostream&` 接口）—— 均已加 `shortcut:` 注释。

   门禁通过后提交。
4. [x] **阶段 4 — D-命令/引擎/编排（142 文件）**：D 簇 4 个枚举
   （`src/Command.h:46` `STATUS`、`src/InitiatorMSEHandshakeCommand.h:51` 与
   `src/ReceiverMSEHandshakeCommand.h:48` 的 `Seq`、
   `src/AbstractOptionHandler.h:124` `Flag`）全部 enum class 化；
   C 簇 8 个与 B 簇 `TlsState` 已分别在阶段 3/2 完成；
   `AbstractCommand`/`DownloadEngine`/`*Command*` 的 ptr+len 与错误处理收口；
   `src/RpcMethodImpl.cc`、`src/aria2api.cc`、`src/download_helper.cc`、
   `src/option_processing.cc` 与协议层/配置层调用点完整清理；
   `src/prefs.cc` 的裸 new 已在阶段 1 列为保留项；`src/OutputFile.h:51-52`
   的 `vprintf`/`printf` 接口保持（`src/Context.cc` 属 C 簇，阶段 3）。
   实测（2026-11-08）：`typedef` 4 处 → `using`（`src/Command.h:42 cuid_t`、
   `src/BencodeDiskWriterFactory.h:45`、`src/MemoryBencodePreDownloadHandler.h:45-46`、
   `src/MemoryBufferPreDownloadHandler.h:43-44`）；`std::find` 4 处
   → `std::ranges::`（`src/AbstractCommand.cc:702`、`src/DefaultPeerStorage.cc:168`、
   `src/File.cc:140`、`src/RpcMethodImpl.cc:551`）。
   **枚举**：`src/Command.h:46` 的 `enum STATUS` → `enum class Status`
   （`STATUS_ALL`→`Status::ALL` 等），`status_` 与 `setStatus`/`statusMatch`
   形参同步，16 个 `src/*Command*.cc`/`*ConnectChain.h` 调用点全改；
   `src/AbstractOptionHandler.h:124` 的 `enum Flag` → `enum class Flag : uint8_t`
   （`FLAG_HIDDEN`→`Flag::HIDDEN` 等 7 个），`updateFlags` 与 14 处
   `flags_ & …` 改 `static_cast<uint8_t>`；`src/InitiatorMSEHandshakeCommand.h:51`
   与 `src/ReceiverMSEHandshakeCommand.h:48` 的 `enum Seq` → `enum class Seq`
   （常量去 `INITIATOR_`/`RECEIVER_` 前缀）。
   **span 化（收口 PLAN-004 留下的 Peer/Piece 链债）**：`src/Segment.h:70`
   `updateHash(int64_t, std::span<const std::byte>)` 虚函数改签名，
   `src/GrowSegment.h:72`、`src/PiecedSegment.h:80`、`src/Piece.h:151` 与
   `test/MockSegment.h:29` 同步；调用点 `src/SinkStreamFilter.cc:92`
   改 `segment->updateHash(..., inbuf.first(wlen))`、
   `src/BtPieceMessage.cc:139` 改
   `std::as_bytes(std::span{data_ + 9, static_cast<size_t>(blockLength_)})`、
   `test/PieceTest.cc:149,154` 改 `std::as_bytes(std::span{spam})`。
   **std::stringstream 去 2 处**：`src/TrackerWatcherCommand.cc:109` 改
   `std::string res` + `readData(data, res.size())` + `append`；
   `src/Exception.cc:102` `stackTrace()` 改 `std::string` + `std::to_string`。
   可解释残余（均加 `shortcut:` 注释）：`src/option_processing.cc:182,228`、
   `src/RpcMethodImpl.cc:1488`、`src/download_helper.cc:217` 的
   `std::stringstream`/`std::istringstream`（`OptionParser::parse()` 与
   `util::createIndexPaths()` 收 `std::istream&`）；`src/StreamPieceSelector.h:58`
   与 `src/UnknownLengthPieceStorage.h:120` 的 `ignoreBitfield, length`
   （`BitfieldMan::get*MissingUnusedIndex` 与 `src/bitfield.h` 整族仍是
   裸指针契约，等整族 span 化时一并改）。

   门禁通过后提交。
5. [x] **收尾**：勾选与实际一致；标题下加
   `> 状态：✅ 已完成（YYYY-MM-DD）`（或 `⚠️ 部分完成` / `❌ 已中止`
   并在文末补「实际偏差」）；「已确认决策」状态列写实际结果；
   记录每阶段实测门禁结果；提交回写；清掉 todo 残留。

## 预计修改的文件

- 核心：`/tmp/p5_{A,B,C,D}.txt` 列出的 415 个 `src/*.h` / `src/*.cc`
  （清单可由「模块范围」的集合运算重算）。
- 跨层调用点（允许完整清理）：协议层 356 / 配置层 80 中被签名波及的文件，
  以及 `test/**` 里的 `test/BitfieldManTest.cc`、
  `test/DefaultBtProgressInfoFileTest.cc`、`test/MockPieceStorage.h`、
  `test/PeerTest.cc`、`test/PieceTest.cc`、`test/SegmentManTest.cc`、
  `test/RequestGroupTest.cc`、`test/RequestGroupManTest.cc`、
  `test/AbstractCommandTest.cc`、`test/DefaultPeerStorageTest.cc`、
  `test/DefaultPieceStorageTest.cc`、`test/*PieceSelectorTest.cc`、
  `test/*StreamFilterTest.cc`、`test/SocketCoreTest.cc` 等。
- 计划文档：`plans/PLAN-005-engine-modernization.md`（本文件）。
- **不在范围**：`src/includes/aria2/aria2.h`（公开 ABI，哈希不变）、
  `src/*.c` 的 7 个 C shim、`deps/**`、`configure.ac`、`src/Makefile.am`、
  `test/Makefile.am`、`.github/workflows/build.yml`、`README.rst`。

## 测试与验证

- 每阶段门禁（沿用 PLAN-001~004，四阶段各跑一次）：
  ```
  git worktree add --detach /tmp/aria2-m5-p<N> HEAD
  cd /tmp/aria2-m5-p<N> && autoreconf -i
  ./configure --enable-werror
  make -j"$(nproc)" check
  ```
  把当前工作区的本阶段 patch 用 `git apply` 灌进该副本后再跑；验证完
  `git worktree remove`。**绝不在当前工作区原地增量 `make`**。
- 通过标准（每阶段全部满足）：
  1. `make check` 退出码 0；`test/test-suite.log` 为 `# TOTAL: 1` / `# PASS: 1`；
     `test/aria2c.log` 末尾 `OK (914)`（任一阶段不得低于 PLAN-004 终值
     `OK (914)`）、无 `Failures !!!`。
  2. 构建目录 `src/Makefile` 中 `CXX = g++ -std=c++20`；
     `WARNCXXFLAGS` 含 `-Wall -Werror -Wformat-security`。
  3. `git hash-object src/includes/aria2/aria2.h` 仍为
     `2216641161b15acb3c3ddf2600dc19e3decd59d5`。
  4. 本阶段宣称清掉的债务用剥注释正则复核为 0 命中（命令见各阶段专项）。
  5. `git status --porcelain` 无生成文件被误加入。
- 阶段专项验证：
  - **阶段 1**：`git grep -nE '\bstd::endl\b'` 在 A 簇 0 命中；
    `git grep -nE '\b(strtol|strtod|sscanf)\s*\('` 在 A 簇 0 命中；
    `test/UtilTest1.cc`、`test/UtilTest2.cc`、`test/Base32Test.cc`、
    `test/ValueBaseTest.cc`、`test/ValueBaseBencodeParserTest.cc`、
    `test/Bencode2Test.cc`、`test/ParamedStringTest.cc`、
    `test/a2algoTest.cc`、
    `test/array_funTest.cc`、`test/OptionTest.cc`、
    `test/OptionHandlerTest.cc`、`test/UriListParserTest.cc`、
    `test/ByteArrayDiskWriterTest.cc` 是主要回归网。
  - **阶段 2**：`src/SocketRecvBuffer::getBuffer()` 返回类型人工核对仍为
    `const unsigned char*`；`test/ByteArrayDiskWriterTest.cc`、
    `test/DefaultDiskWriterTest.cc`、`test/DirectDiskAdaptorTest.cc`、
    `test/MultiDiskAdaptorTest.cc`、
    `test/WrDiskCacheTest.cc`、`test/GZipDecodingStreamFilterTest.cc`、
    `test/ChunkedDecodingStreamFilterTest.cc`、`test/SocketCoreTest.cc` 覆盖
    span 化边界；`src/LibuvEventPoll.*`、`src/InternalMessageDigestImpl.cc`
    等本机不编译分支写成 diff checklist 人工核对签名。
  - **阶段 3**：`git grep -nE 'new unsigned char\[|delete\[\] (bitfield_|useBitfield_|filterBitfield_)' -- src/BitfieldMan.cc`
    0 命中；`git grep -n 'getFilterBitfield'` 只剩声明与 3 个消费点；
    `test/BitfieldManTest.cc` 的 8 条 filter 断言与 `getBitfield()` 断言全绿；
    `test/BtBitfieldMessageTest.cc`、`test/DefaultBtProgressInfoFileTest.cc`、
    `test/PeerTest.cc`、`test/PieceTest.cc`、`test/SegmentManTest.cc`、
    `test/DefaultPieceStorageTest.cc` 是 BitfieldMan/Peer-Piece 链主回归网；
    `src/includes/aria2/aria2.h:749` 人工确认未被误改。
  - **阶段 4**：`git grep -nE '^\s*enum[[:space:]]+' -- src/Command.h src/RequestGroup.h src/NetStat.h src/ServerStat.h src/AbstractOptionHandler.h src/ContextAttribute.h src/*MSEHandshakeCommand.h src/PeerInteractionCommand.h src/RequestGroupMan.cc src/ServerStatMan.cc src/SocketCore.cc | grep -v 'enum class'`
    0 命中（`-P` 与 `-E` 不能同时给 `grep`，实测报 `grep: conflicting matchers specified`）；`Seq::` / `STATUS_` / `STATE_` / `FLAG_` 调用点逐个 grep 确认已适配；
    `test/AbstractCommandTest.cc`、`test/RequestGroupTest.cc`、
    `test/RequestGroupManTest.cc`、`test/PeerConnectionTest.cc` 全绿
    （`test/DownloadEngineTest.cc`、`test/CommandTest.cc` 在仓库中不存在）。
- 债务复核脚本（每阶段末尾统一跑）：`python3` 读文件，先
  `re.sub(r'/\*.*?\*/','',t,flags=re.S)` 与 `re.sub(r'//[^\n]*','',t)` 剥注释，
  再正则统计「债务实测」表 13 个指标；与阶段前基线对比。
- **不执行**：clang 二次构建、`build_test.sh` 的 9 种依赖矩阵、ASan/UBSan、
  `make distcheck`、ABI 检查工具、`--disable-bittorrent` / `--disable-libaria2`
  等未启用编译路径（用户第 1 轮验收口径）。未编译分支靠人工 diff checklist。

## 验收标准

1. 415 文件的四项债务按「提议的方法」清零或留下带代码注释的可解释残余
   （`shortcut: <限制>, <何时升级>`），残余清单写入「实际偏差」。
2. `src/BitfieldMan.cc` 9 处 `new unsigned char[]` 与 7 处 `delete[]` 全部消失，三个成员为
   `std::vector<unsigned char>`；`getBitfield()` / `getFilterBitfield()`
   与 `src/bitfield.h` 算法族的外部契约不变；PLAN-003 实际偏差第 1 条结清。
3. 13 个非 scoped enum 全部 `enum class` 化，所有比较/位运算/数组下标调用点
   在同阶段闭环，编译期无 `-Werror=switch` 遗漏。
4. Peer/Piece 链 span 化完成（PLAN-004 遗留），调用方显式 `std::as_bytes`；
   `src/SocketRecvBuffer::getBuffer()`、`src/bittorrent_helper` 的线上格式
   函数、`BtPieceMessage::setMsgPayload` 等有意保留项不变。
5. 四阶段各一次干净 worktree 门禁通过（判据见「测试与验证」），共 4 次代码提交
   + 1 次文档收尾提交；提交信息 `PLAN-005 阶段N: <子模块> 现代化`，
   无 `Co-authored-by`。
6. `src/includes/aria2/aria2.h` 哈希不变；`git status --porcelain` 干净；
   计划文档状态行与实际一致。

## 风险与权衡

- **范围最大**：415 文件 / 66 073 行，其中 179 个 PLAN-003 已 touch 过，
  diff 会显著大于 PLAN-004（257 文件 / +2988/−2098）；用户已确认全量口径，
  代价是重复扫描与更长的门禁时间。
- **完整清理调用点**重开 PLAN-004 定稿范围：258 个层外 include 方可能被改，
  回归面比「仅单行适配」大；好处是签名/枚举一次收口，不留跨计划欠账。
- **BitfieldMan 是最高风险项**：`expr::array` 表达式模板
  （`src/array_fun.h:39-128`）只收 `T*`，`filterBitfield_` 的 `nullptr`
  语义必须用空 vector 精确表达；`clearFilter()` 后再 `addFilter()` 的行为、
  拷贝构造/赋值/自赋值、以及 `test/BitfieldManTest.cc:300-324` 的 8 条断言
  都要等价。vector 化若改 `Array` 本身，会波及 `src/BitfieldMan.cc` 22 处
  位运算表达式，需全量回归。
- **enum class 波及面**：`Seq::` 103 次、`STATUS_` 56 次、`STATE_` 36 次、
  `FLAG_` 21 次；`src/ServerStat.h:53` 的 `MAX_STATUS` 作数组下标、
  `Command::STATUS_*` 作比较、位掩码的 `operator&` 返回 bool，任一处漏改
  都会被 `-Werror` 或下游断言抓到，但定位成本高。
- **跨层 include 成环**：`util.h` 100 个、`fmt.h` 89 个、`DlAbortEx.h` 59 个
  层外消费者；只能按「谁改签名」排序（A→B→C→D），且每阶段必须把外部调用点
  放进同一提交，否则中间提交不可编译。
- **条件编译兜不住**：96 个文件在 `if/endif` 块内，本机未定义
  `ENABLE_LIBARIA2`/`HAVE_LIBUV`/`USE_INTERNAL_*`，这些分支的签名改动
  编译器验证不到，需人工 checklist（PLAN-004 阶段 1 有同类经验）。
- **验收不假绿**：干净 worktree 是硬要求；PLAN-004 阶段 4 曾在门禁抓到
  3 处 `-Werror=unused-variable`，原地增量 make 会掩盖。
- **不碰的东西**：公开 ABI、`src/*.c`、`deps/**`、构建系统、CI、README；
  `fmt`→`std::format` 已在 PLAN-003 做完，本计划不做重复迁移。

## 自查（写完后对照代码逐条核验）

1. **引用真实**：
   - `git ls-files 'src/*.h' 'src/*.cc' 'src/*.c'` = 858；proto 356、
     cfg 80、shims 7、rest 415 已用集合运算复算并比对
     `/tmp/p5_engine_all.txt`（差 0）；415 里 232 个 `.h`、183 个 `.cc`、
     无 `src/includes/**`、无 `.c`。
   - 415 行数用 `wc -l` 实测 66 073；四簇 67+94+112+142=415，
     14 419+13 259+20 122+18 273=66 073，与全量一致。
   - `src/BitfieldMan.h:53-55` 三个 `unsigned char*`、
     `src/BitfieldMan.h:215,217,284` 的 getter、
     `src/BitfieldMan.cc:65,66,78,79,91,106-116,131-133,676,742` 的
     `new[]`/`delete[]` 已逐行核对；`src/bitfield.h:100,145` 与
     `src/array_fun.h:128` 的签名已核对。
   - `src/includes/aria2/aria2.h:749 virtual std::string getBitfield() = 0`
     是不同类，已核对；`src/fmt.h:46-49,57-60` 已是 `std::format`/`std::vformat` 包装，已核对。
   - `configure.ac:111`、`configure.ac:557-566`、PLAN-003/004 提交号与
     `OK (913)`→`OK (914)` 基线已核对；`src/fmt.h:46-49,57-60`、
     `src/OutputFile.h:51-52`、`src/SocketRecvBuffer.h:66,68` 已逐行核对。
   - 415 与 cfg80 的 6 组共享 stem 实测：`src/{Option,OptionHandlerImpl,RpcMethodImpl,UriListParser,aria2api,download_helper}.h`
     全部 ∈ cfg80，同名 `.cc` 全部 ∈ 415。
2. **阶段可编译、测试不假绿**：删字段/改签名（BitfieldMan 三成员、
   Peer/Piece span 链、13 个 enum）都已 grep 全部调用方：
   `getBitfield(` 34 文件、`getFilterBitfield(` 3 文件、
   `Seq::` 103 次、`STATUS_` 56 次、`STATE_` 36 次、`FLAG_` 21 次、
   415 之外 258 个 include 方（proto 220 / cfg 38）。每阶段要求「同提交闭环」，
   中间提交不允许带未适配的调用点；每条验证都要求干净 worktree 全量重编译
   （`autoreconf -i` + `./configure` + `make check`），因此不会假通过。
   阶段 3 的 BitfieldMan 与阶段 4 的 `Command::STATUS` 无跨阶段耦合
   （`STATUS_` 调用点集中在 D 与 proto，阶段 4 一次收口）。
3. **覆盖所有分支与消费点**：`enum class` 已列全 13 个及全部裸 int/比较/
   下标用法；span 化已列 Peer/Piece 链与 3 个有意保留项；条件编译 96 个文件
   按宏分组列出并标注本机未定义的分支；「完整清理调用点」明确覆盖 proto 356、
   cfg 80 与 `test/**`，且列出 6 组与 cfg 共享 stem 的文件（`.h` 不主动改）。
   `filterBitfield_` 的 up（未启用→`addFilter`）与 down（启用→`clearFilter`）
   两条路径、首轮与稳态都在「风险与权衡」核对。
4. **未确认决策**：无；用户 4 个选择全部已写入「已确认决策」并附备注约束。

## 实测门禁结果

| 阶段 | 提交 | 干净 worktree | make check | aria2c.log | ABI 哈希 |
|------|------|---------------|------------|------------|----------|
| 1 A-基础底座 | `0f0825ac` | `/tmp/aria2-m5-p1` | rc=0，`TOTAL 1 / PASS 1` | `OK (914)` | 不变 |
| 2 B-IO 与事件 | `7585b6ed` | `/tmp/aria2-m5-p2` | rc=0，`TOTAL 1 / PASS 1` | `OK (914)` | 不变 |
| 3 C-Peer/Piece/RequestGroup | `dd1aa82e` | `/tmp/aria2-m5-p3` | rc=0，`TOTAL 1 / PASS 1` | `OK (914)` | 不变 |
| 4 D-命令/引擎/编排 | `f6c5db0a` | `/tmp/aria2-m5-p4` | rc=0，`TOTAL 1 / PASS 1` | `OK (914)` | 不变 |

每次门禁均在 `autoreconf -i` + `./configure --enable-werror`
（`CXX = g++ -std=c++20`、`WARNCXXFLAGS = -Wall -Werror -Wformat-security`）
后全量重编译，`grep -c 'error:'` 均为 0，`git hash-object
src/includes/aria2/aria2.h` 均为 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。

## 实际偏差

1. **`typedef` 清零口径**：415 文件相对基线 `typedef 43` 全部清零，
   但 `src/util.cc:1112,1131,1136` 三个 `typedef enum {...} name;` 与
   `src/BencodeParser.cc:49`、`src/JsonParser.cc:49` 的匿名单枚举保留为
   `enum { ... }`（去掉了 `typedef` 类型名），理由：三者在文件内用 `int`
   变量承载状态（`switch`/位运算/`std::stack<int>`），`enum class` 化要改
   ~250 处引用且零收益 —— 已加 `shortcut:` 注释。
2. **非 scoped enum**：13 个计划内枚举全部 `enum class` 化（B 1 / C 8 / D 4）。
   另有 3 个不在 13 个清单内的文件内状态机保留：`src/ChunkedDecodingStreamFilter.cc:50`
   的 `PREV_CHUNK_*` 匿名枚举、`src/WrDiskCacheEntry.h:97` 的
   `CACHE_ERR_SUCCESS/ERROR` 匿名枚举（`getError()` 返回 `int` 且被 3 个
   `src/*.cc` 与 `test/WrDiskCacheEntryTest.cc` 消费）。
3. **`std::stringstream`**：415 文件从 15 处降到 11 处。清掉的是
   `src/TrackerWatcherCommand.cc:109`（D 簇，改 `std::string`）、
   `src/Exception.cc:102`（D 簇，`stackTrace()` 改 `std::string` +
   `std::to_string`）、`src/OptionHandlerImpl.cc:444`（A 簇，改 `strjoin`）、
   `src/bencode2.cc`（A 簇，`BencodeValueBaseVisitor::out_` 改 `std::string`）。
   保留的 11 处：`src/ByteArrayDiskWriter.h:47`、`src/UriListParser.cc:73`、
   `src/option_processing.cc:182,228`、`src/RpcMethodImpl.cc:1488`、
   `src/download_helper.cc:217`、`src/LibsslTLSContext.cc:200`（已改
   `std::ostringstream`，仍是流）、`src/ConsoleStatCalc.cc:246`、
   `src/RequestGroupMan.cc:806,827,838` —— 均因接口收
   `std::istream&`/`std::ostream&`，已加 `shortcut:` 注释。
4. **ptr+len 残余**：`src/StreamPieceSelector.h:58` 与
   `src/UnknownLengthPieceStorage.h:120` 的 `(const unsigned char*, size_t)`
   保留 —— `BitfieldMan::getSparseMissingUnusedIndex` 等与 `src/bitfield.h`
   整族仍是裸指针契约（PLAN-003 偏差①的延伸），整族 span 化属独立专项。
   `src/ValueBase.h` 的 `String(const unsigned char*, size_t)`、
   `src/message_digest_helper.h:62 digest(unsigned char*, size_t, ...)`、
   `src/bitfield.h:100,145`、`src/prefs.cc:43` 同为有意的裸指针契约。
5. **裸 `new`**：`src/BitfieldMan.cc` 的 9 `new[]` + 7 `delete[]` 已 vector 化
   （专项达成）；保留的 4 处：`src/prefs.cc:65,94,110`（`PrefPtr` 是
   `const Pref*`）、`src/Piece.cc:349`（`DataCellSet` 是
   `std::set<DataCell*>`、`test/TestUtil.h` 返回裸指针）、
   `src/LibuvEventPoll.cc`（libuv 回调 delete，本机 `HAVE_LIBUV` 未定义）。
6. **Peer/Piece 链 span 化范围**：`updateHash` 链（`Segment`/`GrowSegment`/
   `PiecedSegment`/`Piece` + 测试 `MockSegment`）已 span 化；
   `WrDiskCacheEntry::append` + `Piece::appendWrCache` 在阶段 3 完成；
   `src/SocketRecvBuffer::getBuffer()` 与 `src/bittorrent_helper` 的线上格式
   函数按 PLAN-004 决定保持不变。
7. **`a2_sockopt_t` C cast**：`src/SocketCore.cc` 5 处保留 C cast ——
   `src/a2netcompat.h:40` 是 `#define a2_sockopt_t void*`，宏替换后
   `static_cast` 语法不成立。
8. 阶段 1 中途 `git worktree add` 因残留注册报
   `fatal: '/tmp/aria2-m5-p1' is a missing but already registered worktree`，
   改为先 `git worktree prune`。

## 备注

- 用户第 1 轮 4 个选择：范围「全部剩余 415 文件」、改造「四项全做 +
  BitfieldMan 专项」、跨层「允许完整清理调用点」、验证「4 簇 + 干净
  worktree 门禁」。推荐项（精确 250 / 四项全做 + BitfieldMan / 仅单行适配 /
  4 簇门禁）中后两项被采纳，前两项按用户选择放宽。
- 全量口径意味着 PLAN-006 范围不变（仍为 cfg80 + `src/OptionHandlerFactory.cc`
  的约 200 处裸 new）；7 个 `src/*.c` shim 仍待单独计划。
- `/tmp/p5_{A,B,C,D}.txt`、`/tmp/p5_engine_all.txt`、`/tmp/p5_closed.txt`、
  `/tmp/p3_expanded.txt`、`/tmp/cfg.txt` 是探索期临时清单，**不入库**；
  正式范围以「模块范围」的集合运算为准。
- 审查 #1：初稿把 `src/fmt.h` 列为「fmt→std::format」对象；核对
  `src/fmt.h:46-49,57-60` 与提交 `132bb4ad` 后改为「已是 std::format 包装，本计划不重复迁移」，
  并把 409 处 `fmt(` 从「待迁移债务」改成「只按需适配」。
- 审查 #2：初稿沿用 PLAN-004 备注的「208 文件 / 26 187 行」；核对
  `/tmp/engine.txt` 与 `.h`/`.cc` 成对性后发现 43 个单后缀裂缝与
  `src/NullWebSocketSessionMan.h` 重叠，改用用户确认的 415 全量口径，
  并在「模块范围」写明替代关系。
- 审查 #3：初稿把 `src/includes/aria2/aria2.h:749` 的 `getBitfield()` 当成
  BitfieldMan 消费点；核对该头是 `BtProgressInfoFile` 的 `std::string` 接口后
  改为「同名不同类，不要误改」，并固定公开头哈希断言。
- 审查 #4：初稿多处写「14 处 `new[]`/`delete[]`」（沿用 PLAN-003 偏差①编号）；
  `grep -cE 'new unsigned char\['` 实测 9、`grep -cE 'delete\[\]'` 实测 7，
  合计 16 个操作。已在债务表、BitfieldMan 专项、验收标准写实测拆分，
  用户约束段保留「历史沿用 14 处」的由来说明；验收以「全部消失」为准，
  不依赖具体数字。
- 审查 #5：初稿把 13 个枚举全归阶段 4、且用剥注释后的偏移行号；
  用 `grep -nE '^\s*enum\b'` 实测真实行号与簇归属后改为 B 1 / C 8 / D 4，
  并把 `src/prefs.cc`（A 簇）、`src/Context.cc`（C 簇）、
  `src/SocketCore.cc:90 TlsState`（B 簇）放到各自阶段，避免跨阶段留半成品。
- 审查 #6：阶段 4 验证命令原用 `-nE` 与 `-P` 同给，实测报
  `grep: conflicting matchers specified`，已改成 `git grep -nE ... | grep -v 'enum class'`；
  阶段 1 的 `test/UtilTest.cc`、阶段 2 的 `test/AbstractDiskWriterTest.cc` 等
  不存在的测试名已用 `git ls-files 'test/*Test.cc'` 换成真实名字
  （审查 #7，见下）。
