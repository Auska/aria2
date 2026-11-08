# 任务：解除限制后的全面现代化（PLAN-010）

## 目标

前 9 轮现代化（PLAN-001~009）都带着自我施加的限制：不动 `.c` 移植 shim、不动公开 ABI 头、不改 `memcpy`/`ssize_t`、不碰构建系统。本次用户明确「解除所有限制」，把这些此前被排除的方向纳入：

1. 删除 6 个现代平台上永不编译的 `.c` 移植 shim，移除对应 `configure.ac` 平台检查。
2. 公开 ABI 头 `src/includes/aria2/aria2.h` 彻底现代化（接受 breaking change）。
3. `error_code::Value` 非 scoped enum → `enum class`。
4. `ssize_t` 返回错误码 → `size_t` / `std::expected`。
5. 「传入容器& 收集结果」的出参 → 返回值。
6. 巨型文件拆分 + 头文件前向声明化。

## 当前上下文

### 仓库状态

- 路径 `/home/code/tmp/aria2`，分支 `slim-features`，HEAD `a644a5eb`（PLAN-009 收尾），`git status --porcelain` 空。
- 语言基线 `g++ -std=c++23`（`configure.ac:111` `AX_CXX_COMPILE_STDCXX([23], [noext], [mandatory])`，PLAN-007 阶段 1 切）。
- 规模：`git ls-files` 1254 文件；`src/*.{h,cc,c}` 858 文件 / 124 564 行；`test/*Test.cc` 177 个（CPPUnit）；版本 `AC_INIT([aria2],[1.37.0],...)`。
- 构建：`configure.ac` 617 行、`src/Makefile.am` 614 行、`test/Makefile.am` 292 行。
- 本机工具链：g++ 14.2.0、autoconf 2.72、automake 1.17、nproc 24。**无 clang++**。

### 沿用的门禁口径（每阶段）

```
rm -rf /tmp/aria2-m10-pN && git worktree prune
git worktree add --detach /tmp/aria2-m10-pN HEAD
cd /tmp/aria2-m10-pN && git apply /tmp/p10-stageN.patch
autoreconf -i && ./configure --enable-werror
nohup make -j8 check > /tmp/p10-pN-check.log 2>&1 &
```

判据：`# TOTAL: 1` / `# PASS: 1` / `FAIL 0` / `ERROR 0`、`grep -c 'error:'` = 0、`grep -c 'OK (914)' test/aria2c.log` = 1、无 `Failures !!!`。
`--enable-werror` 实际是 `WARNCXXFLAGS = -Wall -Werror -Wformat-security`。

**本计划改变 ABI 判据**：阶段 2 起 `src/includes/aria2/aria2.h` 哈希不再要求等于 `2216641161b15acb3c3ddf2600dc19e3decd59d5`，改为「阶段 2 提交后重新记录新哈希，阶段 3~8 保持不变」。

### 已实测的关键事实

**`.c` shim（`src/Makefile.am:526-546` + `config.h`）**

| 文件 | 行数 | 条件包含 | 本机 config.h |
|---|---|---|---|
| `src/alloca.c` | 483 | **无任何引用**（`Makefile.am` 里没有它） | `HAVE_ALLOCA 1` |
| `src/getaddrinfo.c` | 579 | `if !HAVE_GETADDRINFO` | `HAVE_GETADDRINFO 1` → 不编译 |
| `src/gai_strerror.c` | 84 | `if !HAVE_GAI_STRERROR` | `HAVE_GAI_STRERROR 1` → 不编译 |
| `src/libgen.c` | 87 | `if !HAVE_BASENAME` | `HAVE_BASENAME 1` → 不编译 |
| `src/strptime.c` | 456 | `if !HAVE_STRPTIME` | `HAVE_STRPTIME 1` → 不编译 |
| `src/timegm.c` | 78 | `if !HAVE_TIMEGM` | `HAVE_TIMEGM 1` → 不编译 |
| `src/uri_split.c` | 493 | **无条件包含**（`src/Makefile.am:254`） | 编译 |

引用点：`src/a2netcompat.h:70,75`（`#ifndef HAVE_GETADDRINFO` / `HAVE_GAI_STRERROR` 的 include 分支）、`src/a2time.h:43,47`（`strptime.h`/`timegm.h`）、`src/strptime.c:401` 用 `alloca`。

**公开 ABI 头 `src/includes/aria2/aria2.h`（871 行）**

- 6 个非 scoped enum：`DownloadEvent`(`:105`)、`RUN_MODE`(`:225`)、`OffsetMode`(`:502`)、`UriStatus`(`:562`)、`BtFileMode`(`:637`)、`DownloadStatus`(`:691`)。
- 3 个 `typedef`：`A2Gid`(`:91`)、`KeyVals`(`:98`)、`DownloadEventCallback`(`:145`)。
- 7 处 `const std::string&` 形参/返回：`hexToGid`(`:267`)、`addTorrent`(`:319,329`)、`getGlobalOption`(`:418`)、`getInfoHash`(`:762`)、`getDir`(`:806`)、`getOption`(`:837`)。
- **无** `const char*` 形参（0 处）、**无** ptr+len 对、**不含** `error_code`、**不含** `#include <span>`/`<string_view>`。
- 消费点：`DownloadEvent` 14 处、`RUN_MODE` 3、`OffsetMode` 7、`UriStatus` 3、`BtFileMode` 5、`DownloadStatus` 4。
- 实现侧 `src/aria2api.cc`（829 行）用到 `error_code::Value`(`:149`)、`case error_code::FINISHED:`(`:755`)、`case error_code::REMOVED:`(`:757`)。

**`src/error_code.h:41-78`**

- `enum Value { UNDEFINED = -1, FINISHED = 0, UNKNOWN_ERROR = 1, ..., CHECKSUM_ERROR = 32 }` —— 33 个值，非 scoped，缺 20。
- `error_code::` 全仓 **227 处**；裸 `UNDEFINED`/`FINISHED` 散在 `src/Context.cc`、`src/DownloadResult.cc`、`src/MultiUrlRequestInfo.{h,cc}`、`src/RequestGroup.{h,cc}`。
- 使用者：`src/Exception.h:58 errorCode_`、`src/DownloadResult.h:98 result`、`src/AbstractDiskWriter.cc:123`、`src/DlAbortEx.{h,cc}` 等。

**`ssize_t`（175 处 / 64 文件，50 个函数声明）**

- 语义实测分两类：
  - **throw-on-error 族**（返回值只表示字节数，负数从不外泄）：
    - `src/AbstractDiskWriter.cc:306 ssize_t AbstractDiskWriter::readData(...)` —— 内部 `readDataInternal` 返回 `<0` 时直接 `throw DL_ABORT_EX3(...)`，`return ret;` 只可能是 `>= 0`。`writeData`(`:285`) 已经是 `void`。
    - `src/SocketCore.cc:715-800 writeData/readData/writeVector/readDataFrom` —— EAGAIN 时置 `wantRead_`/`wantWrite_` 并 `ret = 0`，真错误直接 `throw DL_RETRY_EX`；**负数从不返回给调用方**。调用点 56 处（`src/MultiDiskAdaptor.cc` 8、`src/SocketCore.cc` 6、`src/AbstractSingleDiskAdaptor.cc` 6、`src/PeerConnection.cc` 4 等）。
  - **错误码族**（负数就是错误码，调用方必须判断）：
    - `src/BencodeParser.h:67,73 parseUpdate/parseFinal` 返回 `ssize_t`，负值是 `enum class BencodeError { UNEXPECTED_CHAR_BEFORE_VAL = -1, ..., INVALID_FLOAT_NUMBER = -8 }`。
    - `src/GenericParser.h:62,71` 同名转发，`:71 parseFinal(std::span<const std::byte> data, ssize_t& consumed)` 还带出参。
    - `src/WebSocketSession.h:92` + `.cc:332` 的 `ssize_t& error`。
    - `parseUpdate`/`parseFinal` 全仓 68 处引用（`src/JsonParser.cc:102,528`、`src/bencode2.cc:69`、`src/download_helper.cc:314`、`src/BtPostDownloadHandler.cc:97`、`src/HttpServerBodyCommand.cc:229`、`test/ValueBaseBencodeParserTest.cc` 等）。
- `TLSSession::readData/writeData` 返回 `TLSErrorCode::WOULDBLOCK` 负值，是 SocketCore 内部实现细节，**不对外暴露**。

**出参数组（19 处）**

`git grep '(std::vector<[^>]+>& \w+' src/*.h` 命中 19 处，典型：
`src/AbstractCommand.h:138 resolveHostname(std::vector<std::string>& addrs, ...)`、`src/AsyncNameResolverMan.h:67 getResolvedAddress(std::vector<std::string>& res) const`、`src/BitfieldMan.h:120 getFirstNMissingUnusedIndex(std::vector<size_t>& out, size_t n) const`、`src/CookieStorage.h:61 findCookie(std::vector<const Cookie*>& out, ...)`、`src/DefaultPieceStorage.h:242 getAdvertisedPieceIndexes(std::vector<size_t>& indexes, ...)`、`src/HttpResponse.h:127 getDigest(std::vector<Checksum>& result) const`、`src/NameResolveCommand.h:62`、`src/DHTEntryPointNameResolveCommand.h:84`、`src/NameResolver.h:55`、`src/BtLeecherStateChoke.h:88,90`、`src/BtSeederStateChoke.h:82`。
其中 `BtLeecherStateChoke::plannedOptimisticUnchoke/regularUnchoke` 与 `BtSeederStateChoke::unchoke` 是**就地重排**语义（不是收集），属误命中，**不改**。`back_inserter` 全仓 54 处。

**巨型文件 / 头文件**

- 最大：`src/util.cc` 2181、`src/OptionHandlerFactory.cc` 1882、`src/RpcMethodImpl.cc` 1562、`src/SocketCore.cc` 1383、`src/RequestGroup.cc` 1307。
- 头文件 include 数 top：`src/util.h` 28（**被 175 个 src 文件 include，含 test 共 255**）、`src/DHTAbstractNodeLookupTask.h` 19、`src/bittorrent_helper.h` 16、`src/FileEntry.h` 15、`src/RequestGroup.h` 13、`src/DownloadEngine.h` 13。
- `src/util.h` 目前 include 的本地头：`a2time.h`、`a2netcompat.h`、`a2functional.h`、`SegList.h`、`a2iterator.h` 等（`src/util.h:58-64`），另含 20 个系统头（`<cstdio> <cstring> <cstddef> <span> <string> <string_view> <utility> <iosfwd> <ostream> <numeric> <map> <iomanip> <algorithm> <vector> <memory>` …）。

### C++23 特性现状

已用：`std::span` 588、`std::optional` 57、`std::ranges::` 93、`std::source_location` 69、`std::format` 5、`std::from_chars` 6、`std::views::` 6。
零使用：`std::expected`、`std::print`、`std::to_chars`、`std::filesystem`、`std::variant`、`std::out_ptr`、`std::to_underlying`（1）、`std::byteswap`、`std::jthread`/`std::atomic`/`std::mutex`（全 0，单线程事件循环）。

本机 g++14 `<flat_map>`/`<mdspan>` **缺失**；`<expected>`/`<spanstream>`/`<print>`/`<stacktrace>`/`<generator>` 可用。探针已验证 `std::expected` + `std::ispanstream` + `std::ospanstream` + `std::print` + `views::enumerate` 组合可编译（`std::ospanstream` **无 `.view()`**，需自持缓冲）。

## 待确认决策

| 决策点 | 选项（含用户备注的约束） | 推荐 | 状态 |
|---|---|---|---|
| 架构方向 | 四项全要：`ssize_t/错误码 → std::expected`、`出参数组 → 返回值`、`error_code::Value → enum class`、`巨型文件拆分 + 头文件瘦身` | — | **已确认（m11037）** |
| `.c` shim | **删 shim 并移除 configure 平台检查**（用户未选「保留」或「只删文件」） | — | **已确认（m11037）** |
| 公开 ABI | **彻底现代化，接受 breaking change** | — | **已确认（m11037）** |
| 工具链 / CI 增强 | 用户未回答 | 不做 | **待用户确认 → 本计划按「不含 CI 改动」执行** |

## 提议的方法

### 阶段 1：删除 `.c` shim，移除 configure 平台检查

- 删除 `src/alloca.c`、`src/getaddrinfo.c`、`src/gai_strerror.c`、`src/libgen.c`、`src/strptime.c`、`src/timegm.c` 及其同名 `.h`（`getaddrinfo.h`、`gai_strerror.h`、`libgen.h`、`strptime.h`、`timegm.h`）。
- `src/Makefile.am:526-546` 删掉 5 个 `if !HAVE_*` / `endif` 块（保留 `if !HAVE_DAEMON` 块，`daemon.cc` 是 C++ 实现且 `HAVE_DAEMON` 检查的是 `daemon()` 系统调用）。
- `configure.ac:464-484` 删 `AC_CHECK_FUNCS([basename]/[gai_strerror]/[getaddrinfo]/[strptime]/[timegm])` 及对应 `AM_CONDITIONAL`。
- `src/a2netcompat.h` 删除 `#ifndef HAVE_GETADDRINFO ... #endif` 与 `#ifndef HAVE_GAI_STRERROR ... #endif` 两个 include 分支（`:70-77`）。
- `src/a2time.h` 删除 `#ifndef HAVE_STRPTIME ... #endif` 与 `#ifndef HAVE_TIMEGM ... #endif`（`:43-49`）。
- 保留 `src/uri_split.c`/`uri_split.h`（无条件编译、自研 URI 解析器）。
- `README.rst` 的平台支持说明（FreeBSD/macOS 提及）**不动**（属于文档，本计划不涉及）。

**风险**：删除后无法在老 Solaris/Windows MinGW 上构建。用户已明确选择此路径。

### 阶段 2：公开 ABI 头现代化（breaking change）

`src/includes/aria2/aria2.h`：

1. 6 个非 scoped enum → `enum class`：
   - `DownloadEvent`(`:105`) 值 `EVENT_ON_DOWNLOAD_START` 等 → 调用方改用 `DownloadEvent::EVENT_ON_DOWNLOAD_START`。
   - `RUN_MODE`(`:225`)、`OffsetMode`(`:502`)、`UriStatus`(`:562`)、`BtFileMode`(`:637`)、`DownloadStatus`(`:691`) 同理。
   - `typedef int (*DownloadEventCallback)(Session*, DownloadEvent, A2Gid, void*)` 签名不变（参数类型名相同，只是 now scoped）。
2. 3 个 `typedef` → `using`：`using A2Gid = uint64_t;`、`using KeyVals = std::vector<std::pair<std::string, std::string>>;`、`using DownloadEventCallback = int (*)(Session*, DownloadEvent, A2Gid, void*);`。
3. 7 处 `const std::string&` 只读形参 → `std::string_view`：`hexToGid(std::string_view)`、`addTorrent(..., std::string_view torrentFile, ...)`（`:319,329`）、`getGlobalOption(Session*, std::string_view name)`、`getOption(std::string_view name)`。
   **不改返回类型** `const std::string&`（`getInfoHash`/`getDir`，生命周期契约）。
4. 新增 `#include <string_view>`。

同步改：`src/aria2api.cc`（6 个 enum 的 `case` 分支）、全仓 14+3+7+3+5+4 = 36 处 enum 引用点。

**ABI 判据变更**：本阶段提交后重新记录 `git hash-object src/includes/aria2/aria2.h` 的新值，阶段 3~8 必须保持不变。

### 阶段 3：`error_code::Value` → `enum class`

- `src/error_code.h`：`enum class Value { UNDEFINED = -1, FINISHED = 0, ... CHECKSUM_ERROR = 32 };`。
- 227 处 `error_code::XXX` 不变（已带作用域）；裸用的 `UNDEFINED`/`FINISHED` 加 `error_code::Value::` 前缀（`src/Context.cc`、`src/DownloadResult.cc`、`src/MultiUrlRequestInfo.{h,cc}`、`src/RequestGroup.{h,cc}`）。
- `error_code::Value` 用作 `switch` 的 `case`（`src/aria2api.cc:755,757`）需要 `case error_code::Value::FINISHED:`。
- 若出现「隐式转 int」的地方（如 `static_cast<int>(code)`、`fmt("{}", code)`），逐处 `static_cast` 或 `std::to_underlying`（C++23 已可用，本机验证过）。
- **不改** `src/DownloadResult.h:98` 的字段类型（仍是 `error_code::Value`）。

### 阶段 2 实测（2026-11-08）

- 6 个 enum 全部改 `enum class`：`DownloadEvent`、`RUN_MODE`、`OffsetMode`、`UriStatus`、`BtFileMode`、`DownloadStatus`；值引用点 14 个文件（`src/BtCheckIntegrityEntry.cc`、`src/DefaultPieceStorage.cc`、`src/IndexedList.h`、`src/RequestGroupMan.cc`、`src/RpcMethodImpl.cc`、`src/TorrentAttribute.cc`、`src/WebSocketSessionMan.cc`、`src/aria2api.cc`、`src/bittorrent_helper.cc`、`test/Aria2ApiTest.cc`、`test/BittorrentHelperTest.cc`、`test/IndexedListTest.cc`、`test/RequestGroupManTest.cc`、`test/RpcMethodTest.cc`）。
- 3 个 `typedef` → `using`；4 个只读形参 → `std::string_view`（`hexToGid`、`addTorrent` ×2、`getGlobalOption`、`getOption`）；返回类型 `const std::string&` 未动。
- 连带改 `src/prefs.h:63` / `src/prefs.cc:76,88,108`：`option::k2p(const std::string&)` → `k2p(std::string_view)`，`PrefFactory::k2p_` 加 `std::less<>` 透明比较器（否则 `getOption(std::string_view)` 每次都要构造 `std::string`）。
- 3 处函数体适配：`src/RpcMethodImpl.cc:641` `if (torrentAttrs->mode != BtFileMode::BT_FILE_MODE_NONE)`（原 `if (mode)`）；`src/aria2api.cc hexToGid` 加 `std::string hexCopy{hex}` 给 `GroupId::toNumericId`；`addTorrent` 加 `std::string torrentFileCopy{torrentFile}` 给 `createRequestGroupForBitTorrent`。
- 门禁：`autoreconf -i` rc=0、`./configure --enable-werror` rc=0、`make -j8 check` → `# TOTAL: 1` / `# PASS: 1`、`OK (914)`、`error:` 0。
- **新 ABI 哈希：`132c72d9f055756769456c35ca7f8e7a879fbaa8`**（旧值 `2216641161b15acb3c3ddf2600dc19e3decd59d5` 作废）。提交 `0f4143e7`。

### 阶段 3 实测（2026-11-08）

- `src/error_code.h:44` `enum Value` → `enum class Value`；40 个文件的 `error_code::XXX` → `error_code::Value::XXX`（227 处引用）。
- 4 处需显式转整型：`src/main.cc:77` `return static_cast<int>(r);`、`src/Context.cc:138` `exit(static_cast<int>(rv));`、`src/option_processing.cc:196,214` `exit(static_cast<int>(error_code::Value::FINISHED));`、`src/Exception.cc:118,135` 用 `std::to_string(std::to_underlying(errorCode_))` 并补 `#include <utility>`。
- 门禁：`# TOTAL: 1` / `# PASS: 1`、`OK (914)`、`error:` 0、ABI 哈希保持 `132c72d9...`。

### 阶段 4：`ssize_t` → `size_t`（throw-on-error 族）

只改「负数从不外泄」的 4 个类，不动 `TLSSession`（内部仍需负值区分 WOULDBLOCK）：

- `src/AbstractDiskWriter.h:78` / `.cc:306` `readData(std::span<std::byte>, int64_t) → size_t`；`readDataInternal` 保持 `ssize_t`（内部要判 `<0`）。
- `src/SocketCore.h:254,255,260,263,276,279` 6 个 → `size_t`：`writeData`、`writeVector`、`readData`、`readDataFrom` 及内部同名重载。
- `src/SocketBuffer.h:63,84,98,141` 4 个 `send(...)` → `size_t`：`src/SocketBuffer.cc:132 SocketBuffer::send()` 实测 `totalslen += slen` 累加 `socket_->writeVector()` 返回值，且 `slen == 0 && !wantRead && !wantWrite` 时直接 `throw DL_ABORT_EX`，**负值不会外泄** → 安全。`:84,98` 两个子类实现同理（需逐个 grep `< 0` 复核）。
- `src/IOFile.h` / `src/AbstractDiskWriter.h` 里若还有返回负数表示错误的函数需逐个复核。
- **阶段 4 必做的前置检查**：对每个候选函数 grep 全部调用点里的 `< 0` / `<= 0` / `== -1`，有命中的剔除（属错误码族，留给阶段 5）。

调用点 56 处（`src/MultiDiskAdaptor.cc` 8、`src/SocketCore.cc` 6、`src/AbstractSingleDiskAdaptor.cc` 6、`src/PeerConnection.cc` 4 等）：多数只做 `ret > 0` / `ret == 0` 判断，`size_t` 语义等价；**必须逐处检查有没有 `ret < 0` / `ret == -1` 的判断**（有的话说明该函数属错误码族，不能进本阶段）。

### 阶段 4 实测（2026-11-08）

- 只改「负数从不外泄」的族，共 16 文件 +59/−58：
  - `src/BinaryStream.h:52` `readData` 纯虚 + 5 个实现：`src/AbstractDiskWriter.{h,cc}`、`src/AbstractSingleDiskAdaptor.{h,cc}`（`readData` + `readDataDropCache`）、`src/ByteArrayDiskWriter.{h,cc}`、`src/MultiDiskAdaptor.{h,cc}`（3 个重载）、`src/ValueBaseDiskWriter.h`、`src/DiskAdaptor.h` 纯虚。`readDataInternal` 保持 `ssize_t`（内部判 `<0` 后抛异常）。
  - `src/SocketCore.{h,cc}` 6 个：`writeData`(3 重载含 `const std::string&` 便捷版)、`writeVector`、`readData`、`readDataFrom`。
  - `src/SocketBuffer.{h,cc}`：`BufEntry::send` 纯虚 + 2 个实现 + `SocketBuffer::send()`。内部 `firstlen`/`slen`/`len` 的符号混用改成 `size_t firstlen` 不可行（`amount` 是 `ssize_t` 且要判 `> 0`），最终用 `static_cast<ssize_t>`/`static_cast<size_t>` 显式桥接 3 处。
- 顺带清掉的 2 处冗余 `static_cast`：`src/LpdMessageDispatcher.cc:92` 的 `== static_cast<ssize_t>(request_.size())` → `== request_.size()`（`-Werror=sign-compare` 拦住后才发现的）。
- **未改**（属错误码族，留给阶段 5）：`src/TLSSession.h:89 readData`、`src/LibsslTLSSession.h:60`、`src/AbstractDiskWriter.h:56 readDataInternal`、`src/BencodeParser`/`JsonParser` 的 `parseUpdate/parseFinal`、`src/DHTConnection.h:51 sendMessage`/`receiveMessage`（返回 `ssize_t` 且被上层与 0 比较，但内部实为 socket 转发，本阶段**未动**——保留原因：`src/DHTConnectionImpl.cc:96` 用 `length == 0` 判空包，改 `size_t` 语义等价但需一并复核 `src/LpdMessageReceiver.cc:85`、`test/LpdMessageDispatcherTest.cc:66`，风险大于收益，记入下述残余）。
- 门禁：`# TOTAL: 1` / `# PASS: 1`、`OK (914)`、`error:` 0、ABI 哈希保持 `132c72d9...`。

### 阶段 5：parse 链 `ssize_t` → `std::expected`

- `src/BencodeParser.h:67,73`：
  - `std::expected<size_t, BencodeError> parseUpdate(std::span<const std::byte> data);`
  - `std::expected<size_t, BencodeError> parseFinal(std::span<const std::byte> data);`
- `src/JsonParser.{h,cc}` 同形态（`JsonError` 已在 `src/JsonParser.h`）。
- `src/GenericParser.h:62,71`：
  - `parseUpdate` 转发 `std::expected<size_t, ErrorType>`；
  - `parseFinal(std::span<const std::byte> data, ssize_t& consumed)` → `std::expected<ResultType, ErrorType> parseFinal(std::span<const std::byte> data);`（出参消失）。
- `src/WebSocketSession.h:92` / `.cc:332` 的 `ssize_t& error` 一并改成返回 `std::expected`。
- 68 处调用点逐处迁移：`if (rv < 0)` → `if (!rv)`，`rv` 当长度用 → `*rv`。
- `src/bencode2.cc:69`、`src/download_helper.cc:314`、`src/BtPostDownloadHandler.cc:97`、`src/HttpServerBodyCommand.cc:229`、`src/JsonParser.cc:102,528`、`test/ValueBaseBencodeParserTest.cc` 等。
- `ErrorType` 通过 **`Parser` 类**暴露，不是状态机：`src/BencodeParser.h` 加 `using ErrorType = BencodeError;`、`src/JsonParser.h` 加 `using ErrorType = JsonError;`；`src/GenericParser.h` 里 `using ErrorType = typename Parser::ErrorType;`（`Parser` 已是模板参数，见 `src/GenericParser.h:45-46`）。**不能**放到 `src/StructParserStateMachine.h` —— 那个状态机被 bencode 与 json 两种 parser 共用。

### 阶段 5 实测（2026-11-08）

- `src/BencodeParser.h`：加 `<expected>`/`<optional>`，`using ErrorType = BencodeError;`，`std::expected<size_t, BencodeError> parseUpdate/parseFinal(std::span<const std::byte>)`，私有 `std::optional<BencodeError> pushState(int)`，成员 `int lastError_` → `BencodeError lastError_`（初值 `BencodeError::UNEXPECTED_CHAR_BEFORE_VAL`）。`src/BencodeParser.cc` 7 处跨行 `return lastError_ = static_cast<int>(X);` 全改 `lastError_ = X; return std::unexpected(lastError_);`。
- `src/JsonParser.{h,cc}` 同形态，17 处错误返回（13 处 `return lastError_ = static_cast<int>(` + 4 处双行缩进形态）改写；`consumeLowSurrogate` 分支改 `lastError_ = static_cast<JsonError>(rv); return std::unexpected(lastError_);`。
- `src/GenericParser.h`：`using ErrorType = typename Parser::ErrorType;`；`parseFinal` 去掉 `ssize_t& consumed` 出参，改返回 `std::expected<ResultType, ErrorType>`；新增 `size_t getConsumed() const` + 成员 `consumed_`，因为 `bencode2::decode(span, size_t& end)` 的 `end`（trailing garbage 位置）语义必须保留 —— 调用方 `src/DefaultExtensionMessageFactory.cc:115,140,151` 靠它切片。
- `src/bencode2.cc` `decode(span, size_t& end)`：`if (!res) throw DL_ABORT_EX2(...)`，`end = parser.getConsumed()`。
- `src/ValueBaseDiskWriter.h::finalize()` 保持返回 `int`（`rv ? 0 : static_cast<int>(rv.error())`）—— `src/BtPostDownloadHandler.cc:79` 与 `src/HttpServerBodyCommand.cc:235` 都用 `error == 0`。
- `src/WebSocketSession.{h,cc}`：`parseUpdate` → `std::expected<size_t, json::JsonError>`，`parseFinal(span, ssize_t&)` → `std::expected<std::unique_ptr<ValueBase>, json::JsonError> parseFinal(span)`；`onMsgRecvCallback` 改 `auto json = wsSession->parseFinal({}); if (!json) {...}` + `downcast<Dict>(*json)`。
- `src/download_helper.cc:314`、`src/BtPostDownloadHandler.cc:97`、`src/HttpServerBodyCommand.cc:229` 迁移。
- 测试：`test/ValueBaseBencodeParserTest.cc` 加 `parseValue()` 辅助（取 `std::expected` 并断言成功），15 处 `shared_ptr<ValueBase> x = parser.parseFinal(...)` → `auto x = parseValue(parser, src);`；trailing garbage 用例改 `parser.getConsumed()` 断言 `(size_t)7`。`test/ValueBaseJsonParserTest.cc` 18 处 `downcast<X>(r)` → `downcast<X>(*r)`，两处 `error` 断言改 `parser.getConsumed()`（`(size_t)2`）。
- 门禁（`/tmp/aria2-m10-p5`，base `00640811`，patch `/tmp/p10-stage5.patch` 1271 行，14 文件 +255/−234）：`autoreconf -i` rc=0、`./configure --enable-werror` rc=0（`CXX = g++ -std=c++23`、`WARNCXXFLAGS = -Wall -Werror -Wformat-security`）、`make -j8 check` → `# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`OK (914)`、`Failures` 0、`error:` 0、ABI 哈希 `132c72d9f055756769456c35ca7f8e7a879fbaa8`。
- 过程教训：局部 `make -C test` 单目标编译成功 ≠ 全量通过；`downcast<X>(expected)` 这种模板实参推导错误只在真正实例化该 TU 时暴露（`../src/ValueBase.h:350:8: error: 'const class std::unique_ptr<aria2::ValueBase>' has no member named 'accept'`），必须跑全量 `make check`。

### 阶段 6：出参数组 → 返回值（16 处，排除 3 处误命中）

逐个改成返回值：
- `src/AsyncNameResolverMan.h:67 void getResolvedAddress(std::vector<std::string>&) const` → `std::vector<std::string> getResolvedAddress() const`。
- `src/BitfieldMan.h:120 size_t getFirstNMissingUnusedIndex(std::vector<size_t>& out, size_t n) const` → `std::vector<size_t> getFirstNMissingUnusedIndex(size_t n) const`（返回值是 out 的填充个数，长度信息已在返回值里，需同步调用点）。
- `src/CookieStorage.h:61 findCookie(std::vector<const Cookie*>& out, ...)` → `std::vector<const Cookie*> findCookie(...) const`。
- `src/DefaultPieceStorage.h:242 getAdvertisedPieceIndexes(std::vector<size_t>& indexes, ...)` → 返回 `std::vector<size_t>`；调用方 `src/RequestGroup.cc` 用到 `indexes` 与返回值两个信息 → 返回 `std::pair<std::vector<size_t>, uint64_t>` 或小结构体。
- `src/HttpResponse.h:127 getDigest(std::vector<Checksum>& result) const` → `std::vector<Checksum> getDigest() const`。
- `src/AbstractCommand.h:138` / `src/DHTEntryPointNameResolveCommand.h:84` / `src/NameResolveCommand.h:62` / `src/NameResolver.h:55` 的 `resolveHostname(std::vector<std::string>& addrs, ...)` → 返回 `std::vector<std::string>`（同时保留 `int` 状态码的用 `std::pair<int, std::vector<std::string>>` 或 `struct ResolveResult`）。
- `src/BtLeecherStateChoke.h:88,90` / `src/BtSeederStateChoke.h:82` 是**就地重排**，不改。

### 阶段 7：巨型文件拆分 + 头文件前向声明化

- `src/util.cc` 2181 行按职责拆 4 个 TU（实测函数边界，按行号）：
  - `src/util.cc`（保留）：`strip`(`:96`)、`itos`(`:103`)、`difftv`(`:127`)、`difftvsec`(`:137`)、`replace`(`:145`)、字符分类族(`:164-260`)、`isUtf8`(`:243`)、`getContentDispositionFilename`(`:1477`)、大小写转换(`:1508-1525`)、`getHomeDir`/`getXDGDir`/`getConfigFile`/`getDHTFile`(`:1582-1621`)、`getRealSize`/`abbrevSize`(`:1632-1664`)、`sleep`/`usleep`(`:1692-1703`)、`mkdirs`(`:1712`)、`toString`(`:1737`)、`allocateAlignedMemory`(`:1756`)、`getNumericNameInfo`(`:1768`)、`htmlEscape`(`:1783`)、`createIndexPaths`(`:1830`)、`generateRandomData`(`:1840`)、`saveAs`(`:1846`)、路径安全族(`:1869-2050`)、`makeString`/`safeStrerror`(`:2055-2063`)、`noProxyDomainMatch`/`tlsHostnameMatch`(`:2065-2073`)、`strieq`/`istartsWith`/`iendsWith`/`strless`(`:2114-2144`)、`make_fd_cloexec`(`:2162`)。
  - 新增 `src/util_encoding.cc`：`percentEncode*`(`:311-353`)、`torrentPercentEncode`(`:353-368`)、`percentDecode`(`:374`)、`toHex`(`:397`)、`hexCharToUInt`(`:423`)、`secfmt`(`:442`)、`iso8859p1ToUtf8`(`:660`)、`encodeNonUtf8`(`:2050`)。
  - 新增 `src/util_parse.cc`：`trimSpaces`(`:463`)、`parseIntegral`(`:474`)、`parseIntNoThrow`/`parseUIntNoThrow`/`parseLLIntNoThrow`/`parseDoubleNoThrow`(`:485-515`)、`parseIntSegments`(`:534`)、`computeHeadPieces`(`:576`)、`computeTailPieces`(`:598`)、`parsePrioritizePieceRange`(`:621`)。
  - 新增 `src/util_net.cc`：`setGlobalSignalHandler`(`:1560`)、`convertBitfield`(`:1725`)、`generateRandomKey`(`:1894`)、`inPrivateAddress`(`:1908`)、`detectDirTraversal`(`:1925`)、`escapePath`(`:1942`)、`inSameCidrBlock`(`:1958`)、`executeHook`/`executeHookByOptName`(`:1985-2016`)。
  - `src/Makefile.am` 的 `SRCS` 加 3 个 `.cc`。
  - `src/util.h` 声明不拆（保持 include 方的零改动），避免连锁。`src/util.h` 当前被 **175** 个 `src/` 文件直接 include。
- `src/util.h` 前向声明化：把只用于返回类型/形参的本地头（`SegList.h`、`a2iterator.h`、`a2functional.h`、`a2time.h`、`a2netcompat.h`）改成前向声明 + 需要的 TU 自行 include；实测 `src/util.h` 被 175 个 src 文件（含 test 255 个）include，**风险高**，只在编译验证通过后才保留。
- `src/OptionHandlerFactory.cc`（1882 行）**本计划不做**（表驱动重写属功能重构，非现代化）；记入实际偏差。

### 阶段 8：收尾

状态行、5 个（含）以上提交的实测门禁表、债务前后对比、实际偏差、MEMORY.md 记录。

## 分步计划

1. [x] 阶段 1：删除 6 个 `.c` shim 与对应 configure/Makefile 条件（提交 `PLAN-010 阶段1: 删除未编译的 C 移植 shim`）
2. [x] 阶段 2：公开 ABI 头现代化（提交 `PLAN-010 阶段2: 公开 ABI 头现代化`，随后记录新哈希）
3. [x] 阶段 3：`error_code::Value` → `enum class`（提交 `PLAN-010 阶段3: error_code::Value 改为 enum class`）
4. [x] 阶段 4：`ssize_t` → `size_t`（throw-on-error 族）（提交 `PLAN-010 阶段4: throw-on-error 族返回值改 size_t`）
5. [x] 阶段 5：parse 链 `ssize_t` → `std::expected`（提交 `PLAN-010 阶段5: parse 链改用 std::expected`）
6. [ ] 阶段 6：出参数组 → 返回值（提交 `PLAN-010 阶段6: 容器出参改为返回值`）
7. [ ] 阶段 7：`src/util.cc` 拆分 + `src/util.h` 前向声明化（提交 `PLAN-010 阶段7: 拆分 util 巨型文件并瘦身头文件`）
8. [ ] 阶段 8：收尾勾选与实测记录（提交 `PLAN-010: 勾选全部阶段、标记计划完成并记录实测门禁结果`）

### 阶段 1 实测（2026-11-08）

- 删除 11 个文件（`alloca.c`、`getaddrinfo.{c,h}`、`gai_strerror.{c,h}`、`libgen.{c,h}`、`strptime.{c,h}`、`timegm.{c,h}`）+ 测试 `test/TimegmTest.cc`，共 2380 行。
- `configure.ac`：删 5 组 `AC_CHECK_FUNCS([basename]/[gai_strerror]/[getaddrinfo]/[strptime]/[timegm])` + `AM_CONDITIONAL`；额外发现并删除 `AC_FUNC_ALLOCA`（`configure.ac:290`）—— 它会让 automake 把 `src/alloca.c` 写进 `am__DIST_COMMON`，`autoreconf` 报 `src/Makefile.am:573: error: required file 'src/alloca.c' not found`。
- `src/Makefile.am`：删 5 个 `if !HAVE_*` / `endif` 块 + `EXTLDADD` 里的 `@ALLOCA@`。
- `test/Makefile.am`：删 `if !HAVE_TIMEGM → aria2c_SOURCES += TimegmTest.cc` 块。
- `src/a2netcompat.h`：删 `#ifndef HAVE_GETADDRINFO` / `#ifndef HAVE_GAI_STRERROR` 两个 include 分支。
- `src/a2time.h`：删 `#ifndef HAVE_STRPTIME` / `#ifndef HAVE_TIMEGM` 两个 include 分支。
- `test/FtpConnectionTest.cc:194-200`：`#ifdef HAVE_TIMEGM ... #else ... #endif` 死分支删掉（`HAVE_TIMEGM` 现在恒为未定义，`#else` 分支断言 `(time_t)-1` 与真实 glibc `timegm` 行为矛盾 → 首轮 `make check` 报 `FtpConnectionTest.cc:199 Assertion / Expected: -1 / Actual: 1247056992`）。
- 门禁：`autoreconf -i` rc=0、`./configure --enable-werror` rc=0（`CXX = g++ -std=c++23`、`WARNCXXFLAGS = -Wall -Werror -Wformat-security`）、`config.h` 里 `HAVE_STRPTIME`/`HAVE_TIMEGM`/`HAVE_GETADDRINFO`/`HAVE_BASENAME`/`HAVE_GAI_STRERROR`/`C_ALLOCA` 全为 0 命中。
- **踩坑**：原地 `make check` 首轮 FAIL 1 是上面那个死分支；改完 `make -C test aria2c` + 直接跑 `./test/aria2c` 时挂住（aria2c 全量 914 用例本身要 ~10 分钟以上，bash 超时 600s 被杀）。结论：**不要手工跑 `./test/aria2c`**，一律用 `make check` + 轮询日志。

## 预计修改的文件

- 删除：`src/alloca.c`、`src/getaddrinfo.{c,h}`、`src/gai_strerror.{c,h}`、`src/libgen.{c,h}`、`src/strptime.{c,h}`、`src/timegm.{c,h}`（12 文件）。
- 构建：`configure.ac`、`src/Makefile.am`、`test/Makefile.am`。
- 头文件：`src/a2netcompat.h`、`src/a2time.h`、`src/includes/aria2/aria2.h`、`src/error_code.h`、`src/BencodeParser.h`、`src/JsonParser.h`、`src/GenericParser.h`、`src/StructParserStateMachine.h`、`src/WebSocketSession.h`、`src/AbstractDiskWriter.h`、`src/SocketCore.h`、`src/SocketBuffer.h`、`src/BitfieldMan.h`、`src/CookieStorage.h`、`src/DefaultPieceStorage.h`、`src/HttpResponse.h`、`src/AsyncNameResolverMan.h`、`src/NameResolver.h`、`src/NameResolveCommand.h`、`src/DHTEntryPointNameResolveCommand.h`、`src/AbstractCommand.h`、`src/util.h`。
- 实现：`src/aria2api.cc`、`src/BencodeParser.cc`、`src/JsonParser.cc`、`src/WebSocketSession.cc`、`src/AbstractDiskWriter.cc`、`src/SocketCore.cc`、`src/SocketBuffer.cc`、`src/BitfieldMan.cc`、`src/CookieStorage.cc`、`src/DefaultPieceStorage.cc`、`src/HttpResponse.cc`、`src/AsyncNameResolverMan.cc`、`src/NameResolver.cc`、`src/NameResolveCommand.cc`、`src/DHTEntryPointNameResolveCommand.cc`、`src/AbstractCommand.cc`、`src/util.cc` + 新增 `src/util_encoding.cc`、`src/util_parse.cc`、`src/util_file.cc`。
- 测试：`test/ValueBaseBencodeParserTest.cc`、`test/ValueBaseJsonParserTest.cc`、`test/BitfieldManTest.cc`、`test/CookieStorageTest.cc`、`test/SocketCoreTest.cc`、`test/BufferedFileTest.cc`、`test/FileTest.cc`、`test/UtilTest2.cc`、`test/UriTest.cc`、`test/Bencode2Test.cc` 等（按阶段命中）。注：`test/IOFileTest.cc` **不存在**，IOFile 相关测试在 `test/BufferedFileTest.cc`。

## 测试与验证

每阶段：干净 worktree + `--enable-werror` + `make -j8 check`，判据 `# TOTAL: 1` / `# PASS: 1` / `FAIL 0` / `ERROR 0`、`OK (914)`、`error:` 0、无 `Failures !!!`。

阶段专项验证（0 命中 / 结构断言）：

| 阶段 | 命令 | 通过标准 |
|---|---|---|
| 1 | `git ls-files 'src/*.c'` | 只剩 `src/uri_split.c`；`git grep -c 'HAVE_GETADDRINFO\|HAVE_STRPTIME\|HAVE_TIMEGM\|HAVE_BASENAME\|HAVE_GAI_STRERROR' -- configure.ac src` = 0 |
| 2 | `git grep -n '^enum ' -- src/includes/aria2/aria2.h` | 0 命中；`git grep -c '^typedef ' -- src/includes/aria2/aria2.h` = 0 |
| 3 | `git grep -n 'enum Value' -- src/error_code.h` | 命中 `enum class Value` |
| 4 | `git grep -c 'ssize_t' -- src/SocketCore.h src/SocketBuffer.h src/AbstractDiskWriter.h` | 只剩 `readDataInternal`/`writeDataInternal`（内部用） |
| 5 | `git grep -c 'ssize_t' -- src/BencodeParser.h src/GenericParser.h src/JsonParser.h` | 0 |
| 6 | `git grep -nE '\(std::vector<[^>]+>& \w+' -- 'src/*.h'` | 只剩 3 处 Choke 类误命中 |
| 7 | `wc -l src/util.cc src/util_*.cc` | `src/util.cc` < 900 行；`make check` 用例数仍 914 |

**假绿风险与对策**：改公共头（阶段 2/3/5/6）后本地增量 `make` 会假绿（`.deps` 不追踪未变更的 TU）→ 每轮必须 `touch src/*.cc test/*.cc test/*.h` 再 `make`；最终判据只认干净 worktree。

## 验收标准

1. `git ls-files 'src/*.c'` 只剩 `src/uri_split.c`。
2. `src/includes/aria2/aria2.h` 无 `enum `（非 class）、无 `typedef `，含 4 个 `std::string_view` 形参。
3. `src/error_code.h` 是 `enum class Value`。
4. `src/SocketCore.h`/`src/SocketBuffer.h`/`src/AbstractDiskWriter.h` 的公开返回值无 `ssize_t`。
5. `src/BencodeParser.h`/`src/GenericParser.h`/`src/JsonParser.h` 无 `ssize_t`；`parseFinal` 无出参。
6. `src/*.h` 的「容器& 出参」只剩 3 处 Choke 类误命中。
7. `src/util.cc` < 900 行。
8. 每阶段干净 worktree 门禁 `OK (914)`、`# TOTAL: 1`/`# PASS: 1`、`error:` 0。
9. 阶段 2 之后 ABI 哈希在各阶段保持一致（新值记录在文档）。
10. 8 次提交（7 代码 + 1 收尾），无 `Co-authored-by`。

## 风险与权衡

- **阶段 1 不可逆的平台取舍**：删 shim 后只能在 POSIX-2008+（Linux/FreeBSD/macOS）上构建。用户已明确选择「删 shim 并移除 configure 平台检查」。
- **阶段 2 破坏 libaria2 用户源码**：`DownloadEvent::EVENT_*` 等需要加作用域；`getInfoHash()` 仍返回 `const std::string&`（不改生命周期契约）。二进制布局不变（enum 底层类型仍 `int`），但源级 breaking。
- **阶段 3 与阶段 2 耦合**：`src/aria2api.cc` 同时被两者改，阶段 2 先做、阶段 3 复用同一文件，避免两次大改同一 TU。
- **阶段 4 的最大风险是误判**：把「错误码族」函数当「throw-on-error 族」改会让错误被静默成巨大 `size_t`。对策：改前对每个函数 grep 其所有调用点里的 `< 0` / `== -1` 判断，有则剔除。
- **阶段 5 跨 68 个调用点 + 模板 trait**：`GenericParser` 是模板，`ErrorType` 需通过 `ParserStateMachine` 暴露；`parseFinal` 出参消失会改所有调用形态。
- **阶段 7 `src/util.h` 前向声明化风险最高**：175 个 src include 方（含 test 255），任何遗漏都是全仓编译错误。做成独立提交、失败则整个阶段 7 只保留 `src/util.cc` 拆分。
- **条件编译未验证**：`ENABLE_BITTORRENT=1`/`ENABLE_SSL=1`/`HAVE_SQLITE3=1` 已启用；`ENABLE_LIBARIA2`/`HAVE_LIBUV`/`USE_INTERNAL_MD`/`USE_INTERNAL_ARC4`/`USE_INTERNAL_BIGNUM` 未定义 → 相关分支编译器兜不住，需人工 diff checklist。
- **clang 未本地验证**：本机无 clang++，CI 是唯一 clang 验证渠道；本计划不含 CI 改动，故 clang 兼容性风险留存。

## 备注

- 用户授权原文（m10941）："解除所有限制，审查项目，创建现代化计划"。
- 用户决策（m11037）：架构方向四项全选；`.c` shim「删 shim 并移除 configure 平台检查」；公开 ABI「彻底现代化，接受 breaking change」；工具链/CI 问题未回答 → 本计划按不含 CI 改动执行。
- 审查 #1：阶段 4 原本写「`ssize_t` 全族 → `size_t`」，实测 `readData`/`writeData` 内部仍需 `ssize_t` 判负数、`TLSSession` 仍需负值区分 WOULDBLOCK → 收窄为「公开返回值改 `size_t`，内部实现保持 `ssize_t`」。
- 审查 #2：出参数组原写 19 处，实测 3 处是 `BtLeecherStateChoke`/`BtSeederStateChoke` 的**就地重排**误命中 → 改成 16 处。
- 审查 #3：阶段 1 原写「删 7 个 shim」，实测 `src/uri_split.c` 是**无条件编译**的自研 URI 解析器，不属移植 shim → 只删 6 个；`src/alloca.c` 连 `Makefile.am` 都没引用（纯死文件）。
- 审查 #4：C++23 库特性中 `<flat_map>`/`<mdspan>` 在本机 g++14 **缺失** → 计划不使用它们。
- 审查 #5：阶段 2 修改公开 ABI 后，原门禁的「ABI 哈希恒等于 `2216641161b15acb3c3ddf2600dc19e3decd59d5`」不再适用 → 改为「阶段 2 提交后记录新哈希，后续阶段保持」。
- 审查 #6：阶段 5 原写「`ErrorType` 通过 `ParserStateMachine` 暴露」，但 `src/StructParserStateMachine.h` 被 bencode/json 两种 parser 共用，放 ErrorType 会产生类型冲突 → 改放到 `Parser` 类（`BencodeParser`/`JsonParser`），`GenericParser` 用 `typename Parser::ErrorType`。
- 审查 #7：原计划的测试清单含 `test/IOFileTest.cc`，实测不存在 → 换成 `test/BufferedFileTest.cc` + `test/FileTest.cc`。
- 审查 #8：`SocketBuffer::send()` 的 4 处 `ssize_t` 经 `src/SocketBuffer.cc:132-197` 逐行核对，`totalslen += slen` 且 `slen == 0` 时直接 throw → 确认属 throw-on-error 族，可安全改 `size_t`。
- 审查 #9：阶段 7 的拆分方案从「按职责估」改为「按 `src/util.cc` 实测函数行号分组」，新增 `src/util_net.cc`（原写 `util_file.cc` 但文件相关函数与网络/hook 函数纠缠，改按行号实际归属）；`src/util.h` include 方实测 175 个（`git grep -l '#include "util.h"' -- src`），此前写 255 是含 `test/` 的总数。
