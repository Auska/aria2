# 任务：按模块推进现代化 —— 协议层（BT / DHT / HTTP / 网络）

> 状态：⏳ 执行中

## 目标

在 PLAN-001（C++11→C++20）、PLAN-002（工具层 span 化）、PLAN-003（基础层 6 子模块）
已完成的基础上，对**协议层 4 个子模块 / 356 个文件 / 44 957 行**做同一口径的现代化，
每个子模块一次提交，每次提交都是可编译可测试的闭环。

四类改造同时做（用户第 1 轮确认）：

1. **语言惯用法清理**：`typedef`→`using`、非 scoped `enum`→`enum class`、
   裸 `new`/`delete`→RAII 容器、迭代器/`std::find`→ranges、`std::endl`→`'\n'`。
2. **接口 span 化**：协议层内部的 `ptr + len` 签名换成 `std::span`
   （73 处头文件声明），DHT 的定长 ID 参数换 `std::array<unsigned char, DHT_ID_LENGTH>`。
3. **错误处理现代化**：`bool` + 出参改 `std::optional`（只改 2 处，用户第 2 轮确认）。
4. **标准库替换**：`stringstream`→`std::format`/手写切分、`std::find`→
   `std::ranges::find`、`c_str()` 冗余消除。

验收粒度：每个子模块一次干净副本 `--enable-werror` 构建 + `make check`。

## 当前上下文

### 仓库与前置状态

- 仓库 `/home/code/tmp/aria2`，`git status --porcelain` 为空（干净）。
- 最近提交 `465e3cc2`（PLAN-003 收尾）。`plans/` 是项目约定，序号 004 未占用。
- `configure.ac:111` 已是 `AX_CXX_COMPILE_STDCXX([20], [noext], [mandatory])`。
- 本机：`g++ 14.2.0`、`clang++-22`、`autoconf 2.72`、`automake 1.17`、
  `cppunit 1.15.1`。`config.h` 中 `HAVE_OPENSSL 1`，
  `USE_INTERNAL_ARC4`/`USE_INTERNAL_BIGNUM`/`USE_INTERNAL_MD` 均未定义。
- **工作区根目录残留 187 个 `test/*.o` 与旧 `test/aria2c`**，原地增量 `make`
  会假绿 —— 必须用 `git worktree` 干净副本验证（PLAN-001 实测过假绿）。

### 模块范围（356 文件 / 44 957 行 / 187 个 `.h`）

按文件名前缀规则从 `git ls-files 'src/*.h' 'src/*.cc'` 精确划分，四组互斥（已校验无重叠、无遗漏）：

| 子模块 | 前缀正则 | 文件 | `.h` | 行数 | PLAN-003 已 touch |
|---|---|---|---|---|---|
| NET | `Ftp.*\|UDPTracker.*\|Lpd.*\|MSEHandshake.*\|AuthConfig.*\|AuthResolver.*\|Netrc.*\|InternalARC4.*\|LibsslARC4.*\|SimpleRandomizer.*\|XORCloser.*` | 48 | 26 | 7 191 | 10 |
| DHT | `DHT.*\|DHKeyExchange.*\|InternalDHKeyExchange.*\|LibsslDHKeyExchange.*` | 116 | 64 | 12 230 | 29 |
| HTTP | `Http.*\|WebSocket.*\|Cookie.*\|NsCookieParser.*\|cookie_helper.*\|NullWebSocket.*` | 53 | 28 | 9 107 | 12 |
| BT | `DefaultBt.*\|Bt.*\|AbstractBt.*\|SimpleBt.*\|IndexBt.*\|ZeroBt.*\|RangeBt.*\|Extension.*\|Handshake.*\|UTPex.*\|UTMetadata.*\|Announce.*\|MetadataInfo.*\|TorrentAttribute.*\|bittorrent_helper.*\|magnet.*` | 139 | 76 | 16 429 | 31 |
| **合计** | | **356** | 187 | **44 957** | 82 |

范围含 PLAN-003 阶段 1 只改过 `fmt()` 调用点的 82 个文件（用户确认取全量口径，
否则「协议层债务清零」的验收断言不完整）。

**明确不在范围**（引擎层 / 配置层，仅按需单行适配）：
`src/Peer*.cc/.h`、`src/PeerSessionResource.*`、`src/TrackerWatcherCommand.*`、
`src/NameResolveCommand.cc`、`src/InitiatorMSEHandshakeCommand.*`、
`src/ReceiverMSEHandshakeCommand.*`、`src/AbstractAuthResolver.*`、`src/DefaultAuthResolver.*`、
`src/Sqlite3CookieParser*`、`src/AbstractHttpServerResponseCommand.*`、`src/BitfieldMan.*`、
`src/Piece*`、`src/SegmentMan.*`、`src/RequestGroup*`、`src/bignum.h`。

### 子模块依赖（决定提交顺序）

用 `#include` 交叉扫描实测，四组**互相引用**（成环）：

- BT → NET(`SimpleRandomizer.h`)、BT → DHT(`DHTNode.h`)
- DHT → BT(`bittorrent_helper.h`)、DHT → NET(`SimpleRandomizer.h`)
- HTTP → NET(`AuthConfigFactory.h`)、HTTP → BT(`DefaultBtProgressInfoFile.h`)
- NET → HTTP(`HttpHeaderProcessor.h`、`HttpConnection.h`)、NET → BT(`bittorrent_helper.h`、
  `BtHandshakeMessage.h`)、NET → DHT(`DHKeyExchange.h`)

因此**不能按「编译闭包」拆提交**，只能按「改谁的签名」排序，让每个阶段的签名变更
只影响本组文件，跨组调用点适配算该阶段的正常成本。选定顺序：

**NET → DHT → HTTP → BT**

- NET 先行：它对外暴露的签名（ARC4、`SimpleRandomizer::getRandomBytes`、
  `UDPTrackerClient`、`FtpConnection`、`MSEHandshake` 枚举）只有 2 个外部调用点，
  阶段 1 的跨组成本最低。
- DHT 次之：阶段 2 实测无跨组适配（`src/BtPortMessage.cc:94` 的
  `getID()→createNodeLookupTask()` 两侧同步变 `std::array`，源码不用改）。
- HTTP 再次：`HttpHeaderProcessor`/`HttpHeader` 的变更波及 NET 的
  `src/LpdMessageReceiver.cc`（1 处）。
- BT 最后：`bittorrent_helper.h` 的辅助函数被 DHT 7 个文件、NET 2 个文件、
  引擎层 3 个文件、`src/download_helper.cc` 调用（脚本实测），
  放最后可把所有跨组适配集中在一个提交里。

### 债务实测（剥注释后统计，脚本口径见「测试与验证」）

| 项 | BT | DHT | HTTP | NET | 合计 |
|---|---|---|---|---|---|
| `typedef` | 1 | 1 | 2 | 2 | **6** |
| 非 scoped `enum` | 2 | 0 | 3+1(匿名) | 9 | **15** |
| `new` 表达式 | 0 | 1 | 0 | 1 | **2** |
| 裸 `delete` | 0 | 1 | 0 | 0 | **1** |
| `NULL` | 0 | 0 | 0 | 0 | **0 已清零** |
| C 风格转换 | 0 | 0 | 0 | 0 | **0 已清零** |
| `stringstream` | 0 | 0 | 2 | 0 | **2** |
| `std::endl` | 0 | 0 | 0 | 2 | **2** |
| `std::find(_if)` | 5 | 9 | 11 | 1 | **26** |
| `.c_str()` | 73 | 92 | 41 | 74 | **280** |
| `printf(` | 13 | 0 | 0 | 0 | **13**（全在 `src/bittorrent_helper.h`，不改） |
| `ptr+len` 头文件声明 | 40 | 22 | 3 | 8 | **73** |

两处 `new`/一处 `delete` 的真实归属：

- `src/DHTUnknownMessage.cc:62` `data_ = new unsigned char[length];` 与
  `:67` `DHTUnknownMessage::~DHTUnknownMessage() { delete[] data_; }`，
  成员 `src/DHTUnknownMessage.h:44` `unsigned char* data_;`，唯一使用点
  `src/DHTUnknownMessage.cc:85` `util::toHex(std::as_bytes(std::span{data_, sampleLength}))`
  → 改 `std::vector<unsigned char> data_`，使用点写 `util::toHex(std::as_bytes(std::span{data_}))`。
- `src/SimpleRandomizer.cc:60` `randomizer_.reset(new SimpleRandomizer());`
  —— `SimpleRandomizer` 构造函数私有（只能经静态 `getInstance()` 取得），
  **`std::make_unique` 在此处不合法**。保留 `new` 并加
  `// shortcut: 私有构造阻止 make_unique，若放宽可见性则改为 make_unique`。

### 语言标准基线（沿用 PLAN-003，实测 g++ 14.2.0 / clang++-22）

- C++20 可用：`std::format`、`std::span`、`as_bytes`/`as_writable_bytes`、
  `std::from_chars`、`std::optional`、`std::ranges::find`/`find_if`/`any_of`、
  `std::views::split`、`std::spanstream`。
- 仅 C++23：`std::expected`、`std::to_underlying`、`std::byteswap`、`views::zip`、
  `views::enumerate`、`std::print`。
- **`std::ranges::contains` 在 g++14 / clang++-22 的 C++23 下都缺失**，
  继续写 `std::ranges::find(x) != std::end(x)`（仓库既有先例：
  `src/util.cc:190,197,204,211`、`src/SelectEventPoll.cc:83,95`）。
- 结论：**只用 C++20 设施**，`enum class` 转整数写 `static_cast<int>(e)`。

### 已确认决策

用户第 1 轮（4 问）：模块范围 = 协议层 4 子模块；改造内容 = 四项全做；
验收深度 = 干净 worktree + `--enable-werror` + `make check`；提交粒度 = 每子模块一次。

用户第 2 轮（3 问）：DHT ID 签名改 `std::array` **本计划一并做**；
15 个非 scoped enum **全改**（含位掩码与 `UDPTrackerRequest` 的裸 int 字段）；
错误处理**只改 2 个**（`FtpConnection::bulkReceiveResponse`、`cookie_helper::parseDate`）。

用户第 3 轮（4 问）：范围取 **356 文件全量**；span **只改协议层内部签名**
（Peer/Piece 链留 PLAN-005）；`TorrentAttribute::infoHash` **保持 `std::string`**，
只在边界适配；`InterestingExtension`/`InterestingHeader` 的 `int` 存储与 `int` 形参
API **一并换成枚举类型**（引擎层调用点做单行适配）。

## 待确认决策

| 决策点 | 选项（含用户备注的约束） | 推荐 | 状态 |
|---|---|---|---|
| 模块范围 | 仅协议层 4 子模块 / +引擎层 / +配置RPC层 / 全做 | 协议层 | 已确认（第 1 轮） |
| 改造内容 | 惯用法 / span / 错误处理 / 标准库 四项全做 | 四项全做 | 已确认（第 1 轮） |
| 验收深度 | 干净 worktree + `--enable-werror` + `make check` | 该口径 | 已确认（第 1 轮） |
| 提交粒度 | 每子模块一次提交（4 次） | 4 次 | 已确认（第 1 轮） |
| DHT ID 类型 | 改 `std::array` / 留裸指针 | 改 | 已确认（第 2 轮） |
| enum class 范围 | 15 个全改 / 只改 3 个稳妥的 | 全改 | 已确认（第 2 轮） |
| 错误处理范围 | 只改 2 个 / 扩到所有 bool+出参 | 只改 2 个 | 已确认（第 2 轮） |
| 范围口径 | 356 全量 / 排除已 fmt 的 82 个 | 356 全量 | 已确认（第 3 轮） |
| span 边界 | 只改协议层 / 连 Peer-Piece 链 | 只改协议层 | 已确认（第 3 轮） |
| `TorrentAttribute::infoHash` | 保持 `std::string` / 改 `std::array` | 保持 `std::string` | 已确认（第 3 轮） |
| 枚举的 `int` 形参 API | 一并换枚举类型 / 保留 `int` | 一并换 | 已确认（第 3 轮） |

## 提议的方法

### 提交顺序与阶段划分

一个阶段 = 一个子模块 = 一次提交，顺序 **NET → DHT → HTTP → BT**（依赖理由见上）。

### span 规则（沿用 PLAN-002/003，不发明新约定）

- 输入缓冲 `std::span<const std::byte>`，输出缓冲 `std::span<std::byte>`。
- 调用方显式构造：`std::as_bytes(std::span{p, n})`、`std::as_writable_bytes(...)`，
  **绝不依赖隐式构造**（字符串字面量带 NUL，PLAN-002 已踩过）。
- 消息类静态 `create()` 保留原式样，签名改 `std::span<const std::byte> data`，
  入参长度用 `data.size()` 替代 `dataLength`。
- `std::span` 不持有所有权：现有 `const unsigned char*` 成员指针语义不变，
  只在签名层换视图。

### 定长 ID / InfoHash 的表示

- DHT ID：`std::array<unsigned char, DHT_ID_LENGTH>`（`src/DHTConstants.h:48`）。
  形参写 `const std::array<unsigned char, DHT_ID_LENGTH>&`；成员数组同样换
  （`src/DHTNode.h:49`、`src/DHTBucket.h:57,59`、`src/DHTBucketTree.h:91,92`、
  `src/DHTAbstractNodeLookupTask.h:68`、`src/DHTAnnouncePeerMessage.h:51`、
  `src/DHTFindNodeMessage.h:45`、`src/DHTGetPeersMessage.h:54`、
  `src/DHTPeerAnnounceEntry.h:53`、`src/DHTPingReplyMessage.h:45`、`test/MockDHTTask.h:17`）。
- `src/DHTNode.cc:69-80` 的 `operator<` 现状是「逐字节比较，全相同时返回 true」
  （即 `!(a > b)`）。改 `std::array` 后用 `id_ < node.id_` 会变成严格序，
  **这是行为变化点**，必须保留原语义：保留手写循环，只把下标访问换成 `std::array`。
- `bittorrent::getInfoHash()` / `getStaticPeerId()` 改返回
  `std::array<unsigned char, INFO_HASH_LENGTH>`（`src/BtConstants.h` 里
  `INFO_HASH_LENGTH`/`PEER_ID_LENGTH`），`TorrentAttribute::infoHash` 仍是
  `std::string`，由 `getInfoHash()` 内部 `memcpy`/`std::copy_n` 桥接
  （用户第 3 轮确认：只在边界适配）。
- `src/XORCloser.h:49-66` 改成只接受 `std::array<unsigned char, N>`（模板或固定
  `DHT_ID_LENGTH`），删掉 `length_` 成员；`src/DHTIDCloser.h:51` 随之简化。
  **XORCloser 虽然文件归 NET 组，但只被 DHT 的 ID 比较使用**，因此移到阶段 2
  （DHT）与 ID 改造同一提交，避免阶段 1 改一次、阶段 2 再改一次。

### 枚举（15 个全改）

| 文件:行 | 现名 | 处置要点 |
|---|---|---|
| `src/AnnounceTier.h:47` | `AnnounceEvent` | 引用点已全部带 `AnnounceTier::`（`src/AnnounceList.cc:143,158-163,180-183,198-199`、`src/DefaultBtAnnounce.cc:125,131,138,139,228-235`、`test/AnnounceListTest.cc:188`），只改声明 |
| `src/ExtensionMessageRegistry.h:54` | `InterestingExtension` | `MAX_EXTENSION` 提为 `static constexpr size_t`（现被 `src/ExtensionMessageRegistry.cc:43,55,65,75,81,92,103`、`src/HandshakeExtensionMessage.cc:69,88,103,199` 当整数用）；`enum class InterestingExtension { UT_METADATA, UT_PEX }`；`getExtensionMessageID/setExtensionMessageID/removeExtension`、`strBtExtension/keyBtExtension` 的 `int key` 改 `InterestingExtension` |
| `src/HttpHeader.h:74` | `InterestingHeader` | 23 真值 + `MAX_INTERESTING_HEADER`；`MAX_INTERESTING_HEADER` 提为 `static constexpr size_t`；`table_`（`std::multimap<int,std::string>`，`src/HttpHeader.h:50`）改 `std::multimap<InterestingHeader, std::string>`；`put/defined/find/findAll/equalRange/remove/fieldContains` 的 `int hdKey` 全部换枚举；`idInterestingHeader`（`src/HttpHeader.h:164`，实现 `src/HttpHeader.cc:250`）返回 `std::optional<InterestingHeader>`，未命中返回 `std::nullopt`（替代 `MAX_INTERESTING_HEADER` 哨兵） |
| `src/HttpHeaderProcessor.cc:50` | 匿名 `enum {`（20 个状态） | 文件内匿名命名空间 → `enum class ParseState`，约 40 处使用仅本 TU |
| `src/HttpHeaderProcessor.h:50` | `ParserMode` | `src/HttpHeaderProcessor.cc:79` 成员初始化列表 `state_(mode == CLIENT_PARSER ? PREV_RES_VERSION : PREV_METHOD)` 与 `:479` **未限定**，必须同步改全；外部 `src/HttpConnection.cc:67,73`、`src/HttpServer.cc:65`、`src/LpdMessageReceiver.cc:95`、`test/HttpHeaderProcessorTest.cc` |
| `src/HttpServer.h:63` | `RequestType` | **在全局命名空间**（`} // namespace aria2` 之后）→ 移入 `namespace aria2` 并 `enum class`；同步 `src/HttpServer.cc:68,372,379,380,386`、`src/HttpServer.h:80,111`、`src/HttpServerBodyCommand.cc:219-220` 的 `switch/case` |
| `src/FtpNegotiationCommand.h:50` | `Seq` | 类内，引用全在本类 |
| `src/MSEHandshake.h:58` | `HANDSHAKE_TYPE` | 同步 `src/MSEHandshake.cc:99,103,109,113`、`:131`、`src/ReceiverMSEHandshakeCommand.cc:87,90,93,97`（后者不在 356 内，单行适配） |
| `src/MSEHandshake.h:64` | `CRYPTO_TYPE` | **位掩码语义**，照 `src/EventPoll.h:54-90` 先例手写 `constexpr operator|` 与返回 `bool` 的 `operator&`；使用点 `src/MSEHandshake.cc:295,297,354,360,452,458,520` 与 `unsigned char` 互操作，需 `static_cast<unsigned char>` |
| `src/Netrc.cc:137` | 函数内 `enum STATE` | 只本 TU |
| `src/UDPTrackerClient.h:53` | `UDPTrackerConnectionState` | `struct UDPTrackerConnection`（`:56-62`）的 `int state` 与构造 `state(UDPT_CST_CONNECTING)`、`:65` `UDPTrackerConnection(int state, ...)` 一并换 |
| `src/UDPTrackerRequest.h:48/55/63/65` | `UDPTrackerAction`/`UDPTrackerError`/`UDPTrackerState`/`UDPTrackerEvent` | `struct UDPTrackerRequest` 的 `int32_t action`/`int32_t event`/`int state`/`int error` 与 `struct UDPTrackerReply::action` 全换枚举；**线上字节布局不变**，`setIntParam(data+8, req->action)` 处用 `static_cast<uint32_t>`；外部 `src/NameResolveCommand.cc:130,131,136,137`、`src/TrackerWatcherCommand.cc:146,151,152`（引擎层，单行适配）、`src/DefaultBtAnnounce.cc:220,230,233,236,239`（BT 组，阶段 1 适配）、`src/DHTInteractionCommand.cc:133`、`src/UDPTrackerClient.h:161,163` 的 `getUDPTrackerActionStr(int)`/`getUDPTrackerEventStr(int)` |

### 标准库替换

- `std::find(_if)` 26 处 → `std::ranges::find(_if)`（`!= std::end(...)` 写法）。
  位置：BT 5（`src/UTMetadataRequestTracker.cc:54,60` 等）、DHT 9
  （`src/DHTBucket.cc:109,145,156,165,228,264,270`、`src/DHTGetPeersMessage.cc:101`、
  `src/DHTPeerAnnounceEntry.cc:54`）、HTTP 11（`src/HttpHeader.cc:116,119,134,143`、
  `src/cookie_helper.cc:260,261` 等）、NET 1。
- `std::endl` 2 处（`src/SimpleRandomizer.cc:95,110`）→ `'\n'`。
- `stringstream` 2 处：
  `src/Cookie.cc:116` 的 `toNsCookieFormat()` `std::stringstream ss` → 直接 `std::string`
  追加（`+=` / `std::format`）；
  `src/HttpConnection.cc:101` 的 `eraseConfidentialInfo` `std::istringstream` +
  `getline` → `std::views::split(request, '\n')` 逐行处理（保持末尾换行语义）。
- `.c_str()` 280 处：`fmt()` 已是 `std::format` 变参模板（PLAN-003 完成），
  `std::string` 实参可直接传 → **顺手删掉纯为 `fmt` 服务的 `c_str()`**，
  但**不作为验收断言**（删不删都不影响正确性）；涉及
  `exception` 构造、`strcmp` 等仍需 `c_str()` 的保留。
- `src/FtpConnection.cc:222,271,417,490` 的 4 处 `sscanf` **保留**：
  `%*s`、`%14s` 与 `std::from_chars` 不等价，改写的风险大于收益。
- `src/bittorrent_helper.h:345-392` 的 13 处 `o.printf("%s\n", ...)` **不改**：
  那是 `Output` 抽象接口自身的 printf 方法，属 CLI/基础层问题（用户第 1 轮口径）。

### 错误处理（只 2 处）

- `src/FtpConnection.h:76` `bool bulkReceiveResponse(std::pair<int, std::string>& response);`
  → `std::optional<std::pair<int, std::string>> bulkReceiveResponse();`
  调用点全在 `src/FtpConnection.cc`（7 处）与 `test/FtpConnectionTest.cc`。
- `src/cookie_helper.h:51` `bool parseDate(time_t& time, ...)` → `std::optional<time_t> parseDate(...)`，
  实现 `src/cookie_helper.cc:83`，调用点 `src/cookie_helper.cc`、`src/NsCookieParser.cc`、
  `test/CookieHelperTest.cc`。
- 其余 `bool` 返回值语义正确，**一律不动**（`DHTBucket::contains/isInRange`、
  `DHTRoutingTable::addNode`、`CookieStorage::parseAndStore/load/saveNsFormat`、
  `AuthConfigFactory::activateBasicCred`、`HttpHeaderProcessor::parse` 等）。
- `src/WebSocketSession.h:89` `parseFinal(..., ssize_t& error)` 的 `ssize_t&` 出参
  **保留**（PLAN-003 并未把 `GenericParser::parseFinal` 改成结果结构体，
  用户第 1 轮已定不回改基础层），本计划只做 span 化。

## 分步计划

（一个阶段一次提交；4 个阶段，用户授权后用 `todo` 同步 4 个任务）

1. [x] **阶段 1：NET 簇（48 文件 / 7 191 行）**
   - 惯用法：`typedef`→`using` 2 处（`src/AuthConfigFactory.h:78-80` `BasicCredSet`、
     `src/SimpleRandomizer.h:54` `result_type`）；`enum class` 9 个（见上表 NET 行，
     含 `CRYPTO_TYPE` 位运算符）；`std::endl` 2 处 → `'\n'`；`std::find` 1 处 → ranges；
     `src/SimpleRandomizer.cc:60` 的 `new` **保留**并加 `shortcut:` 注释。
   - span 化：`src/InternalARC4Encryptor.h:48,52` 与 `src/LibsslARC4Encryptor.h:64,68`（两版签名
     **必须完全一致**，本机只编译 Libssl 版，编译器兜不住 Internal 版）、
     `src/SimpleRandomizer.h:65` `getRandomBytes(std::span<std::byte>)`、
     `src/UDPTrackerClient.h:78,86,153,157`、
     `src/DHTConnection.h` 之外不动（属 DHT）。
   - 错误处理：`src/FtpConnection.h:76` → `std::optional`。
   - 标准库：`src/FtpConnection.cc` 的 `sscanf` 保留；`c_str()` 顺手清理。
   - **跨阶段耦合点**（实测，均已在本阶段改完）：`src/DHTInteractionCommand.cc:130,152`
     （`receiveReply`/`createRequest` 的 span 与 `UDPTrackerAction`）、
     `src/DefaultBtAnnounce.cc:220,230,233,236,239`、`src/TrackerWatcherCommand.cc:146,151-152`、
     `src/NameResolveCommand.cc:130-137`（枚举）、
     `src/PeerConnection.cc:90,241`、`src/InitiatorMSEHandshakeCommand.cc:160`
     （`ARC4Encryptor::encrypt` span）、`src/ReceiverMSEHandshakeCommand.cc:87`
     （`identifyHandshakeType()` 返回类型）。
     `src/util.cc:1868` 无需改：`util::generateRandomData` 在 PLAN-003 已收 `std::span<std::byte>`。
   - 门禁：见「测试与验证」阶段 1 专项。

2. [ ] **阶段 2：DHT 簇（116 文件 / 12 230 行）**
   - 惯用法：`typedef`→`using` 1 处（`src/DHTPeerAnnounceStorage.h:59`）；
     `src/DHTUnknownMessage.cc:62,67` 的 `new[]`/`delete[]` → `std::vector<unsigned char>`；
     `std::find` 9 处 → ranges。
   - DHT ID → `std::array<unsigned char, DHT_ID_LENGTH>`：11 处成员数组 + 约 30 个 API
     （`src/DHTNode.h:67,73,81`、`src/DHTBucket.h:72,78,88,94,104,106,125`、
     `src/DHTBucketTree.h:63,64,67,68,99,105,112`、`src/DHTRoutingTable.h:88,93`、
     `src/DHTTaskFactory.h:59,69` + `src/DHTTaskFactoryImpl.h:76,85` +
     `test/MockDHTTaskFactory.h`、`src/DHTTokenTracker.h:49,56,61,64`、
     `src/DHTMessageFactoryImpl.h:71`、`src/DHTPeerAnnounceEntry.h:60,83`、
     `src/DHTPeerAnnounceStorage.h:64,73,76`、`src/DHTAnnouncePeerMessage.h:65,77`、
     `src/DHTFindNodeMessage.h:62`、`src/DHTGetPeersMessage.h:81`、
     `src/DHTPingReplyMessage.h:61`、`src/DHTNodeLookupTask.h:47`、
     `src/DHTIDCloser.h:51`、`src/XORCloser.h:49-66`）。
     **保留 `src/DHTNode.cc:69-80` 的「全等返回 true」比较语义**（行为变化点，见上）。
     序列化点 `src/DHTRoutingTableSerializer.cc:114,147`、
     `src/DHTRoutingTableDeserializer.cc:148,190` 用 `.data()` 适配。
   - span 化：`src/DHTConnection.h:48,51`（`receiveMessage`/`sendMessage`，虚函数，
     实现 `src/DHTConnectionImpl.h:80,84`/`.cc:91,107`）、`src/DHTMessageFactory.h:117`、
     `src/DHTMessageFactoryImpl.h:80,146`、`src/DHTMessageReceiver.h:60,70`、
     `src/DHTUnknownMessage.h:52`、`src/DHKeyExchange.h` 两实现
     （`src/InternalDHKeyExchange.h:56,57,61,63,65`、
     `src/LibsslDHKeyExchange.h:63,64,68,70,72`，签名必须一致）。
   - 跨组适配：**无**（实测：`src/DHTConnectionImpl.cc:64` 用的是
     `SimpleRandomizer::getInstance()` 的随机数生成器接口，非阶段 1 改的
     `getRandomBytes`；`src/BtPortMessage.cc:94` 的
     `createNodeLookupTask(localNode_->getID())` 两侧同时变 `std::array`，源码不改）。
   - 门禁：见阶段 2 专项。

3. [ ] **阶段 3：HTTP 簇（53 文件 / 9 107 行）**
   - 惯用法：`typedef`→`using` 2 处（`src/HttpConnection.h:79` `HttpRequestEntries`
     在全局命名空间、`src/WebSocketSessionMan.h:52-54` `WebSocketSessions`）；
     `enum class` 4 个（`InterestingHeader`、`ParserMode`、`HttpHeaderProcessor.cc:50`
     匿名状态机、`RequestType`）；`stringstream` 2 处清掉。
   - span 化：`src/HttpHeaderProcessor.h:60` `parse(std::span<const std::byte>)`
     （保留 `parse(const std::string&)` 重载）、`src/WebSocketSession.h:84,89`
     （`ssize_t& error` 保留）。
   - 错误处理：`src/cookie_helper.h:51` `parseDate` → `std::optional<time_t>`。
   - `std::find` 11 处 → ranges；`c_str()` 顺手清理。
   - 跨组适配（1 处）：`src/LpdMessageReceiver.cc:95,99-100`
     （`SERVER_PARSER` 加 `ParserMode::` 限定、`parse` 传 span）。
   - 门禁：见阶段 3 专项。

4. [ ] **阶段 4：BT 簇（139 文件 / 16 429 行）**
   - 惯用法：`typedef`→`using` 1 处（`src/ExtensionMessageRegistry.h:44`）；
     `enum class` 2 个（`AnnounceEvent`、`InterestingExtension` + `MAX_EXTENSION`
     提 `constexpr size_t`）；`std::find` 5 处 → ranges。
   - span 化（40 处声明）：消息类的 `static create(std::span<const std::byte>)`
     （18 个具体类 + `IndexBtMessage`/`RangeBtMessage`/`ZeroBtMessage` 3 个模板 +
     `HandshakeExtensionMessage`/`UTPexExtensionMessage`）：
     `src/BtAllowedFastMessage.h:50`、`src/BtBitfieldMessage.h:49,57,63`、
     `src/BtCancelMessage.h:50`、`src/BtChokeMessage.h:52`、`src/BtExtendedMessage.h:60`、
     `src/BtHandshakeMessage.h:68,71`、`src/BtHaveAllMessage.h:50`、`src/BtHaveMessage.h:50`、
     `src/BtHaveNoneMessage.h:50`、`src/BtInterestedMessage.h:57`、
     `src/BtNotInterestedMessage.h:58`、`src/BtPieceMessage.h:86,94,99`、
     `src/BtPortMessage.h:69`、`src/BtRejectMessage.h:50`、`src/BtRequestMessage.h:57`、
     `src/BtSuggestPieceMessage.h:51`、`src/BtUnchokeMessage.h:53`、
     `src/IndexBtMessage.h:51`、`src/RangeBtMessage.h:53`、`src/ZeroBtMessage.h:49`、
     `src/HandshakeExtensionMessage.h:108`、`src/UTPexExtensionMessage.h:108`）；
     工厂虚函数链同提交（`src/BtMessageFactory.h:69,73,76` +
     `src/DefaultBtMessageFactory.h:91,94,98` + `test/MockBtMessageFactory.h`；
     `src/ExtensionMessageFactory.h:51` + `src/DefaultExtensionMessageFactory.h:72` +
     `test/MockExtensionMessageFactory.h`）；
     `src/BtAnnounce.h:104` + `src/DefaultBtAnnounce.h:120`（虚函数）；
     `src/bittorrent_helper.h:77,83,142,163,174,179,184,188,192,196,199,205,209,220,230,242`
     （`loadFromMemory`×2、`getStaticPeerId`、`getInfoHash`×2、5 个整数读写、`getId`、
     `checkBitfield`、`createPeerMessageString`、`packcompact`、`unpackcompact`、`assertID`；
     `:90,96,103` 的 3 个 `loadFromMemory` 字符串/值重载不变）。
   - `bittorrent::getInfoHash`/`getStaticPeerId` → `std::array<unsigned char, N>`
     （`src/bittorrent_helper.h:142,163`）。
   - 跨组适配（集中在本次提交，脚本实测的完整集合）：DHT 7 文件
     （`src/DHTFindNodeReplyMessage.cc`、`src/DHTGetPeersReplyMessage.cc`、
     `src/DHTMessageFactoryImpl.cc`、`src/DHTRoutingTableDeserializer.cc`、
     `src/DHTRoutingTableSerializer.cc`、`src/DHTTokenTracker.cc`、
     `src/DHTPeerLookupTask.cc`）+ NET 2 文件（`src/UDPTrackerClient.cc` 18 处、
     `src/MSEHandshake.cc` 1 处）
     + 引擎层 3 文件（`src/PeerInteractionCommand.cc`、`src/RequestGroup.cc`、
     `src/InitiatorMSEHandshakeCommand.cc`）
     + `src/download_helper.cc`（`loadFromMemory`），均为调用点单行适配。
   - **不改**：`src/bittorrent_helper.h:345-392` 的 13 处 `o.printf`。
   - 门禁：见阶段 4 专项。

## 预计修改的文件

范围 = 上表 356 个文件，权威清单可由下面两条命令逐字复现（写进提交说明备查）：

```sh
git ls-files 'src/*.h' 'src/*.cc' \
 | grep -E '^src/(Ftp|UDPTracker|Lpd|MSEHandshake|AuthConfig|AuthResolver|Netrc|InternalARC4|LibsslARC4|SimpleRandomizer|XORCloser)'   # NET
git ls-files 'src/*.h' 'src/*.cc' | grep -E '^src/(DHT|DHKeyExchange|InternalDHKeyExchange|LibsslDHKeyExchange)'                        # DHT
git ls-files 'src/*.h' 'src/*.cc' | grep -E '^src/(Http|WebSocket|Cookie|NsCookieParser|cookie_helper|NullWebSocket)'                  # HTTP
git ls-files 'src/*.h' 'src/*.cc' \
 | grep -E '^src/(DefaultBt|Bt|AbstractBt|SimpleBt|IndexBt|ZeroBt|RangeBt|Extension|Handshake|UTPex|UTMetadata|Announce|MetadataInfo|TorrentAttribute|bittorrent_helper|magnet)'  # BT
```

**跨组单行适配**（不计入阶段范围，但在对应阶段提交里改）：

- 阶段 1：`src/DHTInteractionCommand.cc:130,152`（`UDPTrackerClient::receiveReply`/`createRequest`
  调用点）、`src/DefaultBtAnnounce.cc`、`src/TrackerWatcherCommand.cc`、`src/NameResolveCommand.cc`
  （UDPTracker 枚举）、`src/PeerConnection.cc:90,241`、`src/InitiatorMSEHandshakeCommand.cc:160`
  （`ARC4Encryptor::encrypt` span）、`src/ReceiverMSEHandshakeCommand.cc:87`
- 阶段 2：无
- 阶段 3：`src/LpdMessageReceiver.cc`
- 阶段 4：`src/Peer.h`、`src/Peer.cc`、`src/PeerSessionResource.h`、`src/PeerSessionResource.cc`、
  `src/PeerInteractionCommand.cc`、`src/RequestGroup.cc`、`src/DefaultBtAnnounce.cc`（若阶段 1 未覆盖）、
  `src/DHTFindNodeReplyMessage.cc`、`src/DHTGetPeersReplyMessage.cc`、
  `src/DHTMessageFactoryImpl.cc`、`src/DHTRoutingTableDeserializer.cc`、
  `src/DHTRoutingTableSerializer.cc`、`src/DHTTokenTracker.cc`、`src/DHTPeerLookupTask.cc`、
  `src/UDPTrackerClient.cc`、`src/MSEHandshake.cc`、`src/InitiatorMSEHandshakeCommand.cc`、
  `src/download_helper.cc`、`src/ReceiverMSEHandshakeCommand.cc`（阶段 1）、
  `src/NameResolveCommand.cc`、`src/TrackerWatcherCommand.cc`（阶段 1）

  测试侧同阶段适配：`test/Bt*MessageTest.cc`（16 个）、`test/DefaultBtMessageFactoryTest.cc`、
  `test/DefaultExtensionMessageFactoryTest.cc`、`test/UTPexExtensionMessageTest.cc`、
  `test/DHTFindNodeReplyMessageTest.cc`、`test/DHTGetPeersReplyMessageTest.cc`、
  `test/DHTMessageFactoryImplTest.cc`、`test/DHTRoutingTableSerializerTest.cc`、
  `test/UDPTrackerClientTest.cc`、`test/MSEHandshakeTest.cc`。

**测试文件**（81 个，按阶段）：
NET 10（`AuthConfigFactoryTest`、`FtpConnectionTest`、`LpdMessageDispatcherTest`、
`LpdMessageReceiverTest`、`MSEHandshakeTest`、`NetrcAuthResolverTest`、`NetrcTest`、
`SimpleRandomizerTest`、`UDPTrackerClientTest`、`XORCloserTest`→阶段 2）、
DHT 25（`DHKeyExchangeTest`、`DHT*Test`）、
HTTP 10（`CookieBoxTest`、`CookieHelperTest`、`CookieStorageTest`、`CookieTest`、
`HttpHeaderProcessorTest`、`HttpHeaderTest`、`HttpRequestTest`、`HttpResponseTest`、
`HttpServerTest`、`NsCookieParserTest`）、
BT 36（`AnnounceListTest`、`Bt*MessageTest`、`DefaultBt*Test`、
`ExtensionMessageRegistryTest`、`HandshakeExtensionMessageTest`、`UTMetadata*Test`、
`UTPexExtensionMessageTest`），
共享 mock：`test/MockBtMessageFactory.h`、`test/MockExtensionMessageFactory.h`、
`test/MockBtAnnounce.h`、`test/MockDHTMessageFactory.h`、`test/MockDHTTask.h`、
`test/MockDHTTaskFactory.h`、`test/MockDHTConnection.h`。

## 测试与验证

- 每阶段门禁（沿用 PLAN-001/002/003）：
  ```
  git worktree add --detach /tmp/aria2-m4-p<N> HEAD
  cd /tmp/aria2-m4-p<N> && autoreconf -i
  ./configure --enable-werror
  make -j"$(nproc)" check
  ```
  **绝不在当前工作区原地增量 `make`**：现存 187 个 `test/*.o` 与旧 `test/aria2c`
  会让增量构建假绿。每阶段验证完 `git worktree remove`；临时脚本不入库。
- 通过标准（每阶段都要满足）：
  1. `make check` 退出码 0；`test/test-suite.log` 为 `# TOTAL: 1` / `# PASS: 1`，
     **且 `test/aria2c.log` 末尾为 `OK (N)`、无 `Failures !!!`**（PLAN-003 终值
     `OK (913)`，本计划任一阶段不得低于该数）。
  2. 构建目录 `src/Makefile` 中 `CXX = g++ -std=c++20`；`WARNCXXFLAGS` 含
     `-Wall -Werror -Wformat-security`。
  3. 本阶段宣称清掉的债务用 `git grep` 复核为 0 命中（命令见各阶段专项）。
  4. `git status --porcelain` 无生成文件被误加入。
- 阶段专项验证：
  - **阶段 1（NET）**：
    `git grep -nE '\b(typedef|std::endl)\b' -- 'src/(Ftp|UDPTracker|Lpd|MSEHandshake|AuthConfig|AuthResolver|Netrc|InternalARC4|LibsslARC4|SimpleRandomizer)'` 仅剩
    `src/SimpleRandomizer.h:54` 之外的 0 命中；
    `git grep -nE '\benum\s+(?!class)' -P -- 'src/(Ftp|UDPTracker|Lpd|MSEHandshake|AuthConfig|Netrc)'` 0 命中；
    `test/UDPTrackerClientTest.cc`、`test/MSEHandshakeTest.cc`、`test/FtpConnectionTest.cc`
    是本阶段主要回归网；`src/InternalARC4Encryptor.*` 本机不编译，
    **必须人工逐行核对与 `src/LibsslARC4Encryptor.*` 签名一致**（写成 diff checklist）。
  - **阶段 2（DHT）**：
    `git grep -c 'DHT_ID_LENGTH\]' -- 'src/DHT*'` 应为 0（全部换 `std::array`）；
    `git grep -n 'memcmp' -- 'src/DHT*.cc'` 逐处确认语义等价；
    `src/DHTNode.cc:69-80` 的 `operator<` 手工比对：9 个差异用例
    （全等、首字节大小、末字节大小）在 `test/DHTNodeTest.cc` 里补 1 个断言，
    覆盖「全等返回 true」这一非标准语义；
    `test/DHKeyExchangeTest.cc`、`test/XORCloserTest.cc`、`test/DHTIDCloserTest.cc`
    是 ID 类改造的主要回归网。
  - **阶段 3（HTTP）**：
    `git grep -nE '\b[io]?stringstream\b' -- 'src/(Http|WebSocket|Cookie|NsCookieParser|cookie_helper)'` 0 命中；
    `git grep -n 'MAX_INTERESTING_HEADER' -- 'src/*.cc' 'src/*.h'` 只剩声明处；
    `test/CookieHelperTest.cc` 的 `parseDate` 断言必须改写为 `optional` 语义
    并保留原有 6 个日期用例；
    `test/HttpHeaderProcessorTest.cc` 13 处 `ParserMode` 引用；
    `test/HttpHeaderTest.cc:177-184` 的 `fieldContains` 8 条断言。
  - **阶段 4（BT）**：
    `git grep -nE 'create\(const unsigned char\*' -- 'src/Bt*.h' 'src/IndexBtMessage.h' 'src/RangeBtMessage.h' 'src/ZeroBtMessage.h'` 0 命中；
    `git grep -nE '\btypedef\b' -- 'src/(Bt|DefaultBt|Extension|Handshake|UTPex|UTMetadata|Announce|MetadataInfo|TorrentAttribute|bittorrent_helper|magnet)'` 0 命中；
    **逐字节语义风险最高**：`bittorrent_helper.h` 的
    `getIntParam/setIntParam/createPeerMessageString/checkBitfield` 是 BT 线上格式的核心，
    有 `test/Bt*MessageTest.cc`（36 个）兜底；
    `src/bittorrent_helper.cc:624-628` 的 `getInfoHash` 桥接必须与
    `getInfoHashString` 一致（`test/BtHandshakeMessageTest.cc`、
    `test/DefaultBtAnnounceTest.cc` 覆盖）。
- 债务复核脚本（每阶段末尾统一跑，剥注释后统计，避免 `git grep` 把注释算进去）：
  用 `python3` 读文件、先删 `/* */` 与 `//`，再正则计数上表 12 个指标。
- **不执行**：clang 二次构建、`build_test.sh` 的 9 种依赖矩阵、ASan/UBSan、
  `make distcheck`、ABI 检查工具、`--disable-bittorrent` 编译路径
  （用户第 1 轮验收口径）。

## 验收标准

1. 4 次提交，每次都能在干净副本上
   `autoreconf -i && ./configure --enable-werror && make -j check` 通过，
   `test/aria2c.log` 末尾 `OK (N)`（N ≥ 913）且无 `Failures !!!`。
2. 协议层 356 个文件的 `typedef`（6）、非 scoped `enum`（15）、`stringstream`（2）、
   `std::endl`（2）清零；`std::find(_if)`（26）全部换 `std::ranges::`。
3. 裸 `new`/`delete`：`src/DHTUnknownMessage.cc` 清除；`src/SimpleRandomizer.cc:60`
   保留并带 `shortcut:` 注释（私有构造阻止 `make_unique`），提交信息写明理由。
4. 73 处 `ptr+len` 头文件声明换 `std::span`，DHT ID 换
   `std::array<unsigned char, DHT_ID_LENGTH>`，`getInfoHash`/`getStaticPeerId`
   换 `std::array`；`TorrentAttribute::infoHash` 仍为 `std::string`。
5. 错误处理只改 2 个（`bulkReceiveResponse`、`parseDate`），其余 `bool` 接口不动；
   `WebSocketSession::parseFinal` 保留 `ssize_t& error`。
6. `src/includes/aria2/aria2.h` 哈希仍为
   `2216641161b15acb3c3ddf2600dc19e3decd59d5`（公共 ABI 不变），`examples/` 不改。
7. 范围外的引擎层/配置层文件只有上表「跨组单行适配」列出的改动，
   无其它逻辑改动；`configure.ac`、`src/Makefile.am`、7 个 `src/*.c`、
   `README.rst`、CI 不动。
8. 提交信息为 `PLAN-004 阶段N: <子模块> 现代化`，无 `Co-authored-by`；
   本文档勾选项与实际一致。

## 风险与权衡

- **四组互相 include，成环**：提交顺序不能按编译闭包，只能按「谁改签名」排序
  （NET → DHT → HTTP → BT）。代价是阶段 4（BT）需同时改 DHT/NET 的 17 个
  调用点文件，diff 最大、最易漏；收益是每阶段仍可独立编译、独立验证。
- **`src/InternalDHKeyExchange.*` 与 `src/InternalARC4Encryptor.*` 本机不编译**
  （`config.h` 里 `USE_INTERNAL_BIGNUM`/`USE_INTERNAL_ARC4` 未定义，
  `src/Makefile.am:299-312` 条件包含）。改这两对签名时编译器**完全兜不住**，
  只能靠人工 diff 与 Libssl 版逐行对齐。这是本计划唯一「编译器看不见」的风险面。
- **DHT `operator<`「全等返回 true」是历史非标准语义**（`src/DHTNode.cc:69-80`）。
  改 `std::array` 后若顺手写 `id_ < node.id_` 会静默改变排序语义
  （影响 `std::set`/`std::sort` 的去重行为）。必须保留手写循环 + 补 1 条断言。
- **`enum class` 位掩码**（`CRYPTO_TYPE`）：`src/MSEHandshake.cc` 7 处与
  `unsigned char` 互操作，需 `static_cast`；`operator&` 返回 `bool`
  以保持 `if (x & CRYPTO_ARC4)` 写法。编译器会拦住，不会静默错。
- **`UDPTrackerRequest` 的裸 int 字段换枚举**：字段同时是线上字节来源
  （`setIntParam(data + 8, req->action)`）。换枚举后必须 `static_cast`，
  且 `struct UDPTrackerReply::action` 与 `getUDPTrackerActionStr(int)` 的
  `int` 参数要一起处理，否则 `-Werror` 报 implicit conversion（这是好事，
  编译器兜得住）。外部 `src/NameResolveCommand.cc`、`src/TrackerWatcherCommand.cc`
  属引擎层，会多两次跨层 touch。
- **`InterestingHeader` 换 `enum class` 会改 `HttpHeader::table_` 的键类型**
  （`std::multimap<int,std::string>` → `std::multimap<InterestingHeader,std::string>`），
  波及 `put/defined/find/findAll/equalRange/remove/fieldContains` 7 个 API 与
  `src/HttpResponse.cc:136,284`、`src/HttpServerCommand.cc:214-215` 的调用点。
  `idInterestingHeader` 返回 `std::optional` 后，`src/HttpHeaderProcessor.cc:370`
  的赋值与 5 处 `!= MAX_INTERESTING_HEADER` 比较必须同阶段改完，漏一处就编译失败。
- **`.c_str()` 280 处清理无自动化断言**：`fmt()` 已是 `std::format` 变参模板，
  删 `c_str()` 属纯文本等价变换，但 `exception`/`strcmp`/`Output::printf` 处
  必须保留。因此定为「顺手删、不作验收断言」，避免为凑数字改出风险。
- **span 化的隐式构造陷阱**（PLAN-002 实测）：`std::span{p, n}` 对字符串字面量
  会把结尾 `\0` 算进去。协议层大量 `create(&msg[4], msg.size() - 4)` 形态，
  改写时必须显式 `std::as_bytes(std::span{&msg[4], msg.size() - 4})`，
  不能写成 `std::as_bytes(std::span{msg.data() + 4, ...})` 之外的隐式形态。
- **测试覆盖不均**：`src/LpdMessageReceiver.*`、`src/FtpConnection.cc:222,271,417,490`
  的 PASV/PORT 解析、`src/Cookie.cc` 的 `toNsCookieFormat` 无逐字节断言，
  属已知盲区，靠 `--enable-werror` 与人工 diff 兜。

## 自查（写完后对照代码逐条核验）

1. **引用真实性**：本计划引用的 60+ 个「文件:行号」逐条 `sed -n` 核对过
   （枚举声明、`new`/`delete`、`stringstream`、`typedef`、`ptr+len` 签名、
   `bittorrent_helper.h` 的函数行号、`aria2.h` 哈希）。`src/includes/aria2/aria2.h`
   实测哈希确为 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。
2. **阶段可编译**：四组互相 include 成环，因此按「谁改签名」而非「编译闭包」排序；
   每阶段的跨组调用点数均由脚本实测（`#include` 扫描 + `bittorrent::fn` 调用统计），
   不是估算：阶段 1 = 7 文件（初稿记 2 处，实测漏了 ARC4 span 的两个引擎层调用点
   `src/PeerConnection.cc:90,241` 与 `src/InitiatorMSEHandshakeCommand.cc:160`、
   `src/ReceiverMSEHandshakeCommand.cc:87`、以及 UDPTracker 枚举的三个消费点）、
   阶段 2 = 0 处、阶段 3 = 1 处、阶段 4 = 13 文件。
3. **测试不假绿**：门禁沿用 PLAN-003 的 `test/aria2c.log` 末尾 `OK (N)` 检查
   （Autotools 只看 1 个测试程序，光看 `test-suite.log` 会假绿）；
   并明确不得低于 `OK (913)`。
4. **分支覆盖**：`enum class` 位掩码（`CRYPTO_TYPE`）加了运算符要求；
   `DHTNode::operator<` 的「全等返回 true」非标准语义单独标注并要求补断言；
   `Config.h` 下 `Internal{DHKeyExchange,ARC4Encryptor}` 不参与编译的盲区已列为风险。
6. **执行期修正（审查 #1）**：`enum class` 后 `-Werror=switch` 对
   `src/UDPTrackerClient.cc:334` 的 `switch (req->action)` 报未覆盖枚举值，
   该 switch 原本只判 `Connect` 一支，已改写为 `if`；
   同文件 `A2_LOG_INFO(... action={} ...)` 在 `:245` 处不能再直接格式化枚举，
   改用已有的 `getUDPTrackerActionStr(action)`。
   `test/ARC4Test.cc`、`test/DefaultBtAnnounceTest.cc`、`test/UDPTrackerClientTest.cc`
   的断言语义随之加 `(int)` 转型（CppUnit 的比较需要 `operator<<`，枚举没有）。
5. **反向修正**：初稿曾写「阶段 1 零跨组适配」「阶段 2 有 1 处适配」「`bittorrent_helper.h:301`
   的 `assertID`」，核对后分别改为 2 处、0 处、`:242`；初稿曾把
   `src/DHTBucket.cc`、`src/DHTGetPeersCommand.cc`（只用未变的 `getInfoHashString`）
   误列为跨组适配，已删。

## 备注

- 用户第 2 轮明确：DHT ID 改 `std::array` 不留给 PLAN-005；
  15 个 enum 全改（含位掩码与 UDPTracker 的 int 字段）；错误处理只改 2 个。
- 用户第 3 轮明确：范围取 356 文件全量；span 只改协议层内部签名；
  `TorrentAttribute::infoHash` 保持 `std::string`，边界适配；
  枚举的 `int` 形参 API 一并换枚举类型。
- PLAN-005 预定范围：引擎层 208 文件 / 26 187 行（`Peer*`、`Piece*`、`BitfieldMan`、
  `SegmentMan`、`RequestGroup*`、`DownloadEngine` 等）；PLAN-006 预定范围：
  配置与 RPC 层 + CLI（含 `src/OptionHandlerFactory.cc` 的 200 处裸 `new`）。
  另有 7 个 `src/*.c` 兼容 shim 待后续单独计划处理。
