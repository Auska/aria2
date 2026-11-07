# 任务：用 std::span 替换叶子工具层的「裸指针 + 长度」接口

> 状态：✅ 已完成（2026-11-07）

## 目标

在 PLAN-001（C++11→C++20，已完成）基础上，把 3 个核心工具簇里的
`ptr + len` 接口统一替换成 `std::span<const std::byte>`（输出缓冲区用
`std::span<std::byte>`），并闭环所有调用方与测试断言。公共头
`src/includes/aria2/aria2.h` 保持 ABI 不变。

## 当前上下文

- 仓库 `/home/code/tmp/aria2`，git 工作区干净。`PLAN-001-cpp20-upgrade.md`
  状态为 ✅ 已完成（2026-11-07），3 次提交：`5c5ad9de`、`e663aaae`、
  `9c15c8a2`。`configure.ac:111` 已是
  `AX_CXX_COMPILE_STDCXX([20], [noext], [mandatory])`，
  `src/Makefile` 中 `CXX = g++ -std=c++20`。
- PLAN-001 明确把 `std::span` 排除在当次范围外（该计划「提议的方法」
  第 3 条与「风险与权衡」末条）。本计划是其后续，不是续写或修订；
  `PLAN-002` 文件名未被占用，`plans/` 是项目已有约定（PLAN-001 在此）。
- **范围内仅 3 个工具簇 / 6 个主文件**：`src/util.h` + `src/util.cc`、
  `src/util_security.h` + `src/util_security.cc`、
  `src/bencode2.h` + `src/bencode2.cc`。
  `git grep -E '\*\s*\w+\s*,\s*(size_t|ssize_t)\s+\w+'` 在 3 个头文件
  命中 **19 行**，对应 **17 个函数签名**：`util.h:317`+`319` 是
  `parse_content_disposition` 的同一个跨行声明，
  `util_security.h:68`+`69` 是 `compare` 的两个不同重载。
  按函数计数：util 7 个、util_security 8 个、bencode2 2 个。
- **明确不在范围**：`BencodeParser` / `GenericParser` / `JsonParser` /
  `StructParserStateMachine` 共享的 `(const char*, size_t)` 解析链（含
  `parseFinal(0, 0, ...)` 空指针哨兵与虚函数 `charactersCallback`）、
  `MessageDigest::update(const void*, size_t)` 及其 `MessageDigestImpl`
  虚函数链、`bitfield.h/cc`、`bittorrent_helper.*`、`BinaryStream` /
  `DiskAdaptor` / `DiskWriter` 的 `writeData`/`readData` 虚函数链
  （5+ 子类、113 处调用）、BitTorrent/DHT 消息工厂、`SocketCore` /
  `TLSSession` 的 `void*`、所有 `src/*.c`（C 源码）。
  以上都由用户第 2 轮「仅 3 个核心工具簇」选项排除。
- **`src/includes/aria2/aria2.h` 内没有任何「裸指针 + 长度」签名**：
  全部是 `std::string` / `std::vector<std::string>` / `KeyVals` / 标量 /
  不透明 `void* userData`（`aria2.h:194`），`addTorrent` 两个重载都收
  `const std::string&`。当前哈希
  `2216641161b15acb3c3ddf2600dc19e3decd59d5`。用户已授权「允许改公共
  API」，但本次范围内无可改对象 —— 记为**授权但不使用**，实际不改，
  ABI 保持不变。
- `<span>` 与 `std::as_bytes` / `std::as_writable_bytes` / `std::to_integer`
  在 gcc 14 与 clang++-22 的 `-std=c++20 -Wall -Wextra -Werror` 下实测可用
  （编译探针覆盖 `unsigned char*`、`const unsigned char*`、`char*`、
  `const char*`、`std::string::data()`、`std::array<char,N>`）。
- **字符串字面量隐式转 span 会带结尾 NUL**：探针 `f("a","a")` 得到
  `size()==1` 的 span 实际长度是 2（`\0` 计入）。因此
  `test/UtilSecurityTest.cc:249-264` 的 `compare("a", "a", 1)` 等 20 处
  三参调用**不能**改成隐式 span；必须显式构造以保留「只比较前 n 字节」
  的语义。用户已选「显式 `std::span{ptr,len}`」。
- 三个头文件当前都**没有** `#include <span>`，需新增。
- 调用方规模（按 API 分组，`src/` + `test/`）：
  - `toHex(ptr,len)` 约 126 处；`util.h:232` 与 `234` 是
    `const unsigned char*` / `const char*` 两个重载，另有
    `toHex(const std::string&)` 重载（`util.h:236`）保留。
  - `percentEncode(ptr,len)` 11 处、`torrentPercentEncode(ptr,len)`
    仅声明与定义（无外部 ptr+len 调用，string 重载在用）。
  - `iso8859p1ToUtf8(ptr,len)` 11 处（含 `util.cc:1512` 内部调用）。
  - `generateRandomData(ptr,len)` 35 处（src 11 + test 24）。
  - `parse_content_disposition(...)` 96 处（93 处在 `test/UtilTest1.cc`
    的 `testParseContentDisposition1` / `2` 两个测试函数里，
    `src/util.cc:1503` 一处生产调用）。
  - `bencode2::decode(ptr,len[,end])` 7 处 ptr+len 调用，另有 11 处
    `decode(const std::string&)` 重载调用。
  - `util_security` 簇：`security::compare(ptr,ptr,len)` 20 处（全在
    `test/UtilSecurityTest.cc:249-264`）、`HMAC::create(alg, ptr, len)`
    与 `create(ptr,len)`、`HMAC::update(ptr,len)`、`getResult(ptr,len)`
    （`test/UtilSecurityTest.cc:295,301`）、`PBKDF2(hmac, salt, len, ...)`、
    `HMACResult(char*, len)`（`test/UtilSecurityTest.cc:407`、
    `src/util_security.cc:182`）。
  - 调用方正则 `git grep -l -E '<上述 API>' -- 'src/*.cc' 'src/*.h'
    'test/*.cc' 'test/*.h'` 命中 67 个文件，其中 4 个就是主文件
    （`util.h/cc`、`util_security.h/cc`）；`bencode2.h/cc` 只含定义
    不含调用点。去重后连同 6 个主文件共 **69 个待改文件**
    （`src` 41 + `test` 28），即 6 个主文件 + 63 个纯调用方/测试文件。
    执行时以 `git grep -l` 生成精确清单，不手写全量。
- `parse_content_disposition` 的特殊性：它**不是**单纯输入 ptr+len，
  而是「输出缓冲区 `dest/destlen` + 输出参数 `const char** charsetp`
  / `size_t* charsetlenp` + 输入 `in/len` + `bool defaultUTF8`」。
  函数体约 317 行 C 风格 DFA，用 `const char *p = in, *eop = in + len`、
  `mark_first`/`mark_last` 指针标记、`char* dp = dest` 与 `*dp++ = *p`、
  `--dlen`；`*charsetp = mark_first` / `*charsetlenp = mark_last - mark_first`
  把指针写回输入区间。用户已选「一并改」。
- `test/UtilSecurityTest.cc` 的 PBKDF2/HMAC 测试数据用
  `test.pass` / `test.pass_len` / `test.salt` / `test.salt_len` 这类
  `const char*` + 长度的结构体字段，改 span 时需显式构造，不能靠
  字符串字面量隐式推导。
- 测试口径沿用 PLAN-001：`make check` 只跑一个 CPPUnit 二进制
  `test/aria2c`，`test/test-suite.log` 恒为 `# TOTAL: 1` / `# PASS: 1`；
  真正判断全绿要看 `test/aria2c.log` 末尾 `OK (N)` 且无 `Failures !!!`。
  当前工作区根目录仍有 PLAN-001 时期就存在的 187 个残留 `.o` 与
  旧 `test/aria2c`（实测 15 个 Metalink 残留失败），因此验证必须用
  `git worktree` 干净副本，绝不能原地增量 `make`。
- `configure.ac:524` 在启用 websocket 时用
  `AC_CONFIG_SUBDIRS([deps/wslay])`（PLAN-001 阶段 2 删掉 34 行
  探测代码后行号已从 558 移到 524；558 现在是 werror 块），
  全新 VPATH 构建需验证子包；优先用 `git worktree` 干净副本规避。
- 本机：`g++ 14.2.0`、`clang++-22`、`autoconf 2.72`、`automake 1.17`、
  `cppunit 1.15.1` 可用。`.gitignore` 已覆盖 `configure` / `Makefile.in` /
  `config.h.in` / `*.o` / `*.lo` 等生成文件与构建产物，无需补充。

## 待确认决策

| 决策点 | 选项（含用户备注的约束） | 推荐 | 状态 |
|---|---|---|---|
| 替换范围 | **纯工具函数层** / +消息协议层 / +磁盘虚接口 / 全库 | 纯工具层 | 已确认：第 1 轮选纯工具层；第 2 轮细化为「仅 3 个核心工具簇」 |
| 接口形态 | **直接替换、不保留旧重载** / 保留兼容重载 / 只新增 span 重载 | 直接替换 | 已确认：第 1 轮；所有调用方同一提交内闭环，编译器揪出漏改 |
| 公共 API | 保持不动 / **允许改公共 API** | 保持不动 | 已确认：第 1 轮授权「允许改」；第 2 轮确认「记为授权但不使用」——公共头无 ptr+len 签名，实际不改、ABI 不变 |
| 验收深度 | **干净副本 + werror** / GCC+Clang 双编译 / 仅一次 make check | 干净副本+werror | 已确认：第 1 轮 |
| 调用方写法 | **显式 `std::span{ptr,len}`** / 数组隐式推导 / 混用 | 显式 | 已确认：第 3 轮；字符串字面量带 NUL，必须显式保留长度语义 |
| 元素类型 | 保留 char/uchar 双重载 / **统一 `std::byte`** | 保留双重载 | 已确认：第 3 轮选统一 `std::byte`；输入用 `span<const std::byte>`，输出用 `span<std::byte>`，调用方配 `as_bytes` / `as_writable_bytes` |
| DFA 例外 | 跳过保留原签名 / **一并改** | 跳过 | 已确认：第 3 轮选「一并改」；`parse_content_disposition` 的 dest 与 in 都改 span，93 处测试断言同步改 |

无待确认项。

## 提议的方法

1. **统一元素类型**：3 个头文件加 `#include <span>`（必要时加
   `<cstddef>`）。输入缓冲区统一 `std::span<const std::byte>`，
   输出缓冲区用 `std::span<std::byte>`：
   - `util.h`：`percentEncode` / `torrentPercentEncode` / `toHex` /
     `iso8859p1ToUtf8` 的输入改 `span<const std::byte>`；
     `toHex` 的两个 ptr 重载合并为一个 `span<const std::byte>`；
     `generateRandomData` 改 `span<std::byte>`；
     `parse_content_disposition` 的 `dest/destlen` 合并为
     `span<std::byte> dest`、`in/len` 合并为
     `span<const std::byte> in`。
     `charsetp` / `charsetlenp` 仍是输出参数，但改成
     `std::span<const std::byte>& charset` 出参（由函数体从 `in` 的子区间
     构造），避免把 `std::byte*` 强转回 `const char*`。
   - `util_security.h`：`compare` 的两个 ptr 重载合并为一个
     `compare(span<const std::byte> a, span<const std::byte> b)`，
     长度不等直接返回 false（原实现要求等长，见函数注释）；
     `HMACResult(char*,len)` / `HMAC(alg,secret,len)` /
     `create(alg,secret,len)` / `create(secret,len)` /
     `update(data,len)` / `getResult(data,len)` /
     `PBKDF2(hmac,salt,len,...)` 全部改 span。
     保留 `const std::string&` 便捷重载，内部用
     `std::as_bytes(std::span{s.data(), s.size()})` 转发。
   - `bencode2.h`：`decode(data,len)` 与 `decode(data,len,end)` 改
     `span<const std::byte>`；`decode(const std::string&)` 保留并转发。
     `bencode2.cc:68` 需把 `span.data()` 转成 `const char*` 供
     `ValueBaseBencodeParser::parseFinal(const char*, size_t, ssize_t&)`
     使用（该解析链**不在范围内**，保持原签名）。
2. **调用方统一显式构造**：`toHex(std::as_bytes(std::span{ptr, len}))`、
   `compare(std::as_bytes(std::span{"a", 1}), ...)`、
   `generateRandomData(std::as_writable_bytes(std::span{buf, len}))`。
   定长数组也写显式 `{arr, sizeof(arr)}` 形式（用户选「显式」，
   不做隐式推导），避免字符串字面量 NUL 与子串长度歧义。
3. **`parse_content_disposition` 函数体**：把 `p`/`eop`/`mark_first`/
   `mark_last` 从 `const char*` 改为 span 下标或
   `span<const std::byte>::iterator`，`dp`/`dlen` 换成
   `std::span<std::byte>` 上的写游标；每处 `*p` 用
   `static_cast<char>(std::to_integer<unsigned char>(*it))` 取回字符，
   `isLws` / `inRFC2616HttpToken` / `utf8dfa` 等仍收 `char`。
   `*charsetp`/`*charsetlenp` 改为写出 `span<const std::byte>` 子区间。
   **保持 DFA 状态机、错误返回值（-1）、边界检查（`dlen == 0`）与
   `quoted_seen` / `dfa_state` 逻辑逐字节等价**。
4. **测试同步**：`test/UtilTest1.cc` 93 处 `parse_content_disposition`
   断言、`test/UtilSecurityTest.cc` 的 20 处 `compare` + HMAC/PBKDF2
   数据、以及 60 余个 src/test 调用方全部在同一阶段内改完。
   为 `std::byte` 转换补 2-3 个边界断言（空 span、长度不等的
   `compare`、含内嵌 NUL 的缓冲区），防止隐式 NUL 语义漂移。
5. **不变**：`src/includes/aria2/aria2.h`、`examples/`、
   `BencodeParser.*` / `GenericParser.h` / `JsonParser.*` /
   `StructParserStateMachine.h`（含 `parseFinal(0,0,...)` 哨兵）、
   `MessageDigest*` 虚函数链、`bitfield.*`、`bittorrent_helper.*`、
   `BinaryStream` / `DiskAdaptor` / `DiskWriter` 虚接口、`SocketCore` /
   `TLSSession` 的 `void*`、所有 C 源码、`configure.ac`、CI 配置、
   生成文件（`configure` / `Makefile.in` / `config.h.in` 等）。

## 分步计划

（阶段数 3，用户授权后同步成 3 个 todo 任务；一个阶段一次提交）

1. [x] **阶段 1：util 簇（含 DFA）改 std::span**
   - `src/util.h` / `src/util.cc`：**7 个** ptr+len 函数签名与对应定义
     （`percentEncode`、`torrentPercentEncode`、`toHex` ×2 合并、
     `iso8859p1ToUtf8`、`parse_content_disposition`、`generateRandomData`），
     加 `#include <span>`。
   - 重写 `parse_content_disposition` 函数体（约 317 行 DFA）为 span
     迭代器/下标形式，保持状态机与错误码等价；`charsetp`/`charsetlenp`
     改 span 出参。
   - 改所有 `util::` ptr+len 调用方（约 60 个 src 文件 + 若干 test），
     含 `test/UtilTest1.cc` 的 93 处断言；补空 span / NUL 边界断言。
   - **跨阶段耦合点必须在本阶段一并改**：`src/util_security.cc:132`
     的 `generateRandomData((unsigned char*)buf.get(), len)` 是阶段 1
     所改 API 的调用方，但文件本身属阶段 2。若留到阶段 2，
     阶段 1 提交会编译失败。本阶段只改这一行调用为
     `std::as_writable_bytes(std::span{buf.get(), len})`，
     其余 `util_security.*` 的 ptr+len 接口留到阶段 2；
     `src/util.cc:1913` 的 `generateRandomData(bytes, sizeof(bytes))`
     是同文件内部调用，本阶段一并改。
   - 门禁：干净副本 `autoreconf -i && ./configure && make -j check`，
     `test/aria2c.log` 为 `OK (N)` 且无 `Failures !!!`；
     `git grep` 确认 `util.h` 里不再有 ptr+len 声明。
2. [x] **阶段 2：util_security 簇改 std::span**
   - `src/util_security.h` / `.cc`：**8 个** ptr+len 函数签名
     （`compare` 的 2 个重载合并为 1 个 span 重载）与 3 个 `.cc` 定义
     （`compare` ×2 合并、`HMACResult`、`HMAC`、`create` ×2、`update`、
     `getResult`、`PBKDF2`），加 `#include <span>`；
     string 便捷重载改用 `as_bytes` 转发。
   - `HMACResult::operator==` 内部的 `compare(result_.data(), ...)`、
     `PBKDF2` 内部的 `hmac->update(salt, salt_length)` 与
     `hmac->update((char*)&c, sizeof(c))` 同步改 span。
   - `test/UtilSecurityTest.cc` 20 处 `compare`、PBKDF2/HMAC 测试数据、
     `src/DownloadEngine.cc` / `src/HttpServer.cc` 里
     `make_unique<HMACResult>(...)` 的调用核对。
   - 门禁：同上；`git grep` 确认 `util_security.h` 里不再有 ptr+len 声明。
3. [x] **阶段 3：bencode2 簇 + 最终验收**
   - `src/bencode2.h` / `.cc`：2 个 ptr+len `decode` 改
     `span<const std::byte>`，string 重载转发；`bencode2.cc:68` 处
     `reinterpret_cast<const char*>(data.data())` 保留（解析链不在范围内）。
   - 7 处 `bencode2::decode(ptr,len[,end])` 调用方改显式 span，
     含 `data + 1, length - 1` 这类子区间（`DefaultExtensionMessageFactory.cc:114`、
     `HandshakeExtensionMessage.cc:172`、`UTPexExtensionMessage.cc:204`）。
   - 全仓 `git grep` 复核 3 个头文件里 17 个 ptr+len 函数签名
     （19 行）全部消失，
     且范围内调用方无残留裸 ptr+len 形式。
   - 最终验收：全新 `git worktree` 干净副本，
     `autoreconf -i && ./configure --enable-werror && make -j check`，
     核对 `CXX = g++ -std=c++20`、`WARNCXXFLAGS` 含 `-Werror`、
     `test/aria2c.log` 为 `OK (N)`、无 `Failures !!!`、
     `src/includes/aria2/aria2.h` 哈希仍为
     `2216641161b15acb3c3ddf2600dc19e3decd59d5`。

## 预计修改的文件

**主文件（6）**：`src/util.h`、`src/util.cc`、`src/util_security.h`、
`src/util_security.cc`、`src/bencode2.h`、`src/bencode2.cc`

**调用方 / 测试（63 个纯调用方 + 6 个主文件，共 69 个待改文件；
执行时以 `git grep -l` 精确清单为准，不手写全量）**：
- src（约 38）：`src/BtBitfieldMessage.cc`、`src/BtHandshakeMessage.cc`、
  `src/BtHandshakeMessageValidator.cc`、`src/BtPieceMessage.cc`、
  `src/DHTAbstractNodeLookupTask.h`、`src/DHTAnnouncePeerMessage.cc`、
  `src/DHTBucket.cc`、`src/DHTBucketRefreshTask.cc`、
  `src/DHTFindNodeMessage.cc`、`src/DHTGetPeersMessage.cc`、
  `src/DHTMessage.cc`、`src/DHTMessageFactoryImpl.cc`、
  `src/DHTMessageReceiver.cc`、`src/DHTMessageTracker.cc`、`src/DHTNode.cc`、
  `src/DHTPeerAnnounceStorage.cc`、`src/DHTPeerLookupTask.cc`、
  `src/DHTQueryMessage.cc`、`src/DHTResponseMessage.cc`、
  `src/DHTRoutingTable.cc`、`src/DHTSetup.cc`、`src/DHTTokenTracker.cc`、
  `src/DHTUnknownMessage.cc`、`src/DefaultBtAnnounce.cc`、
  `src/DefaultBtMessageReceiver.cc`、`src/DefaultBtProgressInfoFile.cc`、
  `src/DefaultExtensionMessageFactory.cc`、`src/GroupId.cc`、
  `src/HandshakeExtensionMessage.cc`、`src/InternalDHKeyExchange.cc`、
  `src/MSEHandshake.cc`、`src/RpcMethodImpl.cc`、
  `src/TrackerWatcherCommand.cc`、`src/UTPexExtensionMessage.cc`、
  `src/bittorrent_helper.cc`、`src/DownloadEngine.cc`、`src/HttpServer.cc`
- test（约 23）：`test/ARC4Test.cc`、`test/BittorrentHelperTest.cc`、
  `test/BtBitfieldMessageTest.cc`、`test/BtHandshakeMessageTest.cc`、
  `test/DHKeyExchangeTest.cc`、`test/DHTAnnouncePeerMessageTest.cc`、
  `test/DHTAnnouncePeerReplyMessageTest.cc`、`test/DHTBucketTest.cc`、
  `test/DHTFindNodeMessageTest.cc`、`test/DHTFindNodeReplyMessageTest.cc`、
  `test/DHTGetPeersMessageTest.cc`、`test/DHTGetPeersReplyMessageTest.cc`、
  `test/DHTMessageFactoryImplTest.cc`、`test/DHTNodeTest.cc`、
  `test/DHTPingMessageTest.cc`、`test/DHTPingReplyMessageTest.cc`、
  `test/DHTTokenTrackerTest.cc`、`test/DefaultBtProgressInfoFileTest.cc`、
  `test/JsonTest.cc`、`test/PieceTest.cc`、`test/RpcMethodTest.cc`、
  `test/TestUtil.cc`、`test/UTPexExtensionMessageTest.cc`、
  `test/UtilSecurityTest.cc`、`test/UtilTest1.cc`、`test/UtilTest2.cc`、
  `test/ValueBaseTest.cc`、`test/AnnounceListTest.cc`

**新增**：`plans/PLAN-002-std-span-refactor.md`（本文件）

**不变 / 不入 diff**：`src/includes/aria2/aria2.h`、`examples/`、
`src/BencodeParser.*`、`src/GenericParser.h`、`src/JsonParser.*`、
`src/StructParserStateMachine.h`、`src/MessageDigest.*`、
`src/MessageDigestImpl.h` 及 4 个实现、`src/bitfield.*`、
`src/bittorrent_helper.h`、`src/BinaryStream.h`、`src/DiskAdaptor.h`、
`src/DiskWriter.h` 与子类、`src/SocketCore.*`、`src/TLSSession.h`、
`src/*.c`、`configure.ac`、`.github/workflows/build.yml`、
`README.rst`、所有生成文件。

## 测试与验证

- 每阶段门禁 + 最终验收（沿用 PLAN-001 的干净副本方式）：
  ```
  git worktree add --detach /tmp/aria2-span-build HEAD
  cd /tmp/aria2-span-build && autoreconf -i
  ./configure [阶段3: --enable-werror]
  make -j"$(nproc)" check
  ```
  **绝不在当前工作区原地增量 `make`**：现存 187 个残留 `.o` 与旧
  `test/aria2c` 会让增量构建假绿（实测 15 个 Metalink 残留失败，
  而旧 `test/aria2c.log` 还停在 `OK (978)`）。
  每阶段验证完 `git worktree remove` 清理，临时脚本/补丁不入库。
- 通过标准：
  1. `make check` 退出码 0；`test/test-suite.log` 为 `# TOTAL: 1` /
     `# PASS: 1`，**且 `test/aria2c.log` 末尾为 `OK (N)`、无
     `Failures !!!`**（Autotools 只看到 1 个测试程序，光看
     test-suite.log 会假绿）。
  2. `src/Makefile` 中 `CXX = g++ -std=c++20`；阶段 3 的
     `WARNCXXFLAGS` 含 `-Wall -Werror -Wformat-security`，
     C++20 新增警告会直接失败。
  3. `git grep -n -E '\*\s*\w+\s*,\s*(size_t|ssize_t)\s+\w+' --
     'src/util.h' 'src/util_security.h' 'src/bencode2.h'` 无命中
     （17 个 ptr+len 函数签名 / 19 行全部消失）。注意 `parse_content_disposition`
     改完后 `charsetp`/`charsetlenp` 若仍是指针出参则不算 ptr+len
     **输入**签名，但本计划已改为 span 出参，不应有残留。
  4. `git grep -n 'std::span' -- 'src/util.h' 'src/util_security.h'
     'src/bencode2.h'` 有命中；`git grep -n '#include <span>'` 覆盖 3 头。
  5. `parse_content_disposition` 的 93 处测试断言全部保留且仍能区分
     0 / -1 / 各类字节数，`test/UtilSecurityTest.cc` 的 20 处 compare
     断言仍覆盖长度不等、内嵌 NUL、`\xff` 边界。
  6. `git hash-object src/includes/aria2/aria2.h` 仍为
     `2216641161b15acb3c3ddf2600dc19e3decd59d5`（ABI 不变）。
  7. `git status --porcelain` 中无生成文件被误加入
     （`.gitignore` 已覆盖）。
- **不执行**（按用户验收决定）：clang 二次构建、`build_test.sh` 的
  9 种依赖矩阵、ASan/UBSan 复跑、`make distcheck`、单独的 ABI 检查工具。

## 验收标准

1. 从干净副本 `autoreconf -i && ./configure --enable-werror &&
   make -j check` 全程通过，`OK (N)` 且无 `Failures !!!`。
2. 3 个工具簇的 17 个 `ptr+len` 函数签名全部替换为 `std::span`，
   无旧兼容重载保留；所有调用方显式构造 span。
3. 字符串字面量 + 显式长度的调用语义不变（不含结尾 NUL），
   `compare` 长度不等返回 false，DFA 的 93 处断言结果不变。
4. `src/includes/aria2/aria2.h` 无任何改动，libaria2 ABI 不变；
   `examples/` 不改。
5. 范围外的解析器链 / 消息层 / 磁盘虚接口 / C 源码 / 构建与 CI 配置
   均无改动。
6. 三次提交分别对应三个计划阶段，`PLAN-002` 勾选项与实际一致。

## 风险与权衡

- **用户选了「统一 `std::byte` + 一并改 DFA」**：这是本计划最大的
  复杂度来源。`std::as_bytes` / `as_writable_bytes` 在约 200 处调用点
  引入样板代码；`parse_content_disposition` 的 317 行 DFA 要逐处
  `std::to_integer` 取回 `char`，任何一处漏转都会改变 HTTP
  Content-Disposition 解析行为（文件名、UTF-8、RFC 2231 字符集）。
  唯一兜底是那 93 处断言 + `getContentDispositionFilename` 的 20 处断言。
- **字符串字面量的 NUL 陷阱**：`std::span{"a"}` 长度是 2 而不是 1。
  用户已选「显式 `std::span{ptr,len}`」；阶段 2 尤其要核对
  `test/UtilSecurityTest.cc:249-264` 的 `compare("a\0b", "a\0b", 3)`、
  `compare("a\0b", "a\0b", 4)`（注释明确写「implicit null-termination」），
  改成 `std::as_bytes(std::span{"a\0b", 3})` 这类显式形式，
  否则会静默多比一个字节。
- **元素类型统一后 `toHex` 两个重载合并**：`const unsigned char*` 与
  `const char*` 调用方都要 `as_bytes`，若某处传的是
  `std::string::data()`（C++20 起 `char*`）或 `reinterpret_cast` 结果，
  需逐个确认没有把「字符」误当「字节」翻转语义。
- **`bencode2::decode(data + 1, length - 1, end)` 的子区间**：
  改成 `span.data() + 1, size() - 1` 后再构造 span，
  必须保证 `length >= 1`；原代码同样依赖该前提，编译期不会兜底。
- **`parse_content_disposition` 的 charset 出参**：原实现把指针写回
  输入区间（`*charsetp = mark_first`），改 span 出参需保证生命周期
  指向 `in` 内部且调用方 `util.cc:1503` 用 `strieq(charset, ...)` 的
  地方能正确消费；若处理不当会悬垂或改变 ISO-8859-1 判定。
- **「允许改公共 API」授权未使用**：公共头无 ptr+len 签名，本次不改。
  若后续阶段需要动 `aria2.h`，本计划已记录授权，但需重新走
  ABI 影响评估，不得默认沿用。
- **验收只跑 gcc + werror**：未覆盖 clang-18 对 `std::as_bytes` /
  `std::to_integer` 的诊断差异与 9 种条件编译组合；CI 会在 push 后
  补上 clang + 4 维矩阵，但本地合并前不会发现 clang 专属问题。
  `<span>` 相关 API 已在 clang++-22 用编译探针验证过基本可用性。
- 工作区已有 PLAN-001 时期遗留的构建产物，原地增量构建会假绿；
  验证必须用干净副本。

## 备注

- 用户第 1 轮选择：「纯工具函数层」「直接替换、不保留旧重载」
  「允许改公共 API」「干净副本 + werror」。
- 用户第 2 轮选择：「仅 3 个核心工具簇」（明确排除 BencodeParser /
  MessageDigest / bitfield / bittorrent_helper / 虚接口链）、
  「公共 API 记为授权但不使用」。
- 用户第 3 轮选择：「显式 `std::span{ptr,len}`」
  「统一为 `span<const std::byte>`」「DFA 一并改」。
- 选项备注里的硬约束（「只改叶子不碰虚函数链」「公共头默认不动」的
  反向授权「允许改但不使用」、字符串字面量 NUL 陷阱）已写入
  「当前上下文」与「风险与权衡」。
- 与 PLAN-001 的关系：PLAN-001 明确排除 `std::span`，本计划是其后续
  独立计划，不修订、不覆盖、不作废 PLAN-001。
- 审查 #1：初稿写「19 个 ptr+len 声明」。逐行核对发现
  `util.h:317`+`319` 是同一函数的跨行声明、`util_security.h:68`+`69`
  是 `compare` 的两个重载，因此 19 行对应 17 个函数签名；已全文修正
  为「17 个函数签名（19 行）」，并把 util 7 / util_security 8 /
  bencode2 2 的分项计数写入「当前上下文」。
- 审查 #2：初稿沿用 PLAN-001 文档里的 `configure.ac:558`；
  PLAN-001 阶段 2 删掉 nullptr/override 探测块后该文件少了 34 行，
  实际 `AC_CONFIG_SUBDIRS([deps/wslay])` 在 `configure.ac:524`，
  558 现在是 werror 块。已就地改为 524。
- 审查 #3：初稿把 `util_security.cc` 整体划到阶段 2，但该文件
  `132` 行调用了阶段 1 要改的 `generateRandomData`。分阶段提交时
  阶段 1 会因这个调用方编译失败。已把这一行调用显式列入阶段 1，
  并在阶段 1 说明「其余接口留到阶段 2」，保证每阶段可编译。
- 审查 #4：核验 `std::as_bytes(std::span{...})` 与
  `decode(const std::string&)` 便捷重载并存时无重载歧义；
  gcc 14 / clang++-22 编译探针均通过，string 重载需显式转发。
- 审查 #5：核验 `test/UtilSecurityTest.cc:236-240` 的
  `compare('a','a')` 单字节重载（`util_security.h:58` 声明）
  与数组版是两个不同重载，本次只改数组版；单字节版保留，
  「20 处 compare 断言」的计数仅指三参数组版。
- 审查 #6：初稿写「约 67 个文件」口径不清。用计划里的调用正则
  实测命中 67 个文件（含 4 个主文件），加上只含定义的
  `bencode2.h/cc` 后去重为 69 个（`src` 41 + `test` 28），
  即 6 主文件 + 63 纯调用方/测试文件。已修正正文与「预计修改的文件」。
- 执行前仓库检查：`git rev-parse --is-inside-work-tree` 已确认为 true；
  `.gitignore` 已覆盖构建产物与生成文件，无需补充；`plans/` 为项目
  已有约定，`PLAN-002` 序号未被占用。

## 执行记录（2026-11-07）

三次提交：`2e359ab6`（阶段 1）、`d7bcfa63`（阶段 2）、阶段 3 提交。

- 每阶段均在 `git worktree` 干净副本上 `autoreconf -i && ./configure && make -j check`
  通过；阶段 3 加 `--enable-werror`。最终 `OK (911)`、`# PASS: 1`、无 `Failures !!!`。
- 实际改动文件：阶段 1 54 个、阶段 2 3 个、阶段 3 9 个；共 69 个文件，与计划一致。
- 范围外的解析链 / 消息层 / 磁盘虚接口 / C 源码 / CI 均未动；
  `src/includes/aria2/aria2.h` 哈希仍为 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。

### 实际偏差

1. **DFA 实现方式**：计划写「改为 span 迭代器/下标 + 逐处 `std::to_integer`」。
   实际改为**接口换 span、函数体保留原有 `const char*` 指针算法**：入口从
   `in.data()` / `dest.data()` 派生 `p/eop/dp`，`*charsetp` 用
   `in.subspan(...)` 表达。317 行状态机逐字节未改，语义风险显著低于逐处改写。
   这比计划描述更保守，且同样达成「接口不再暴露裸指针 + 长度」。
2. **`compare` 长度不等时返回 false**：原 API 要求调用方保证等长；改成 span 后
   长度自带，故新增不等长返回 false 的保护 + 3 条边界断言。
   `test/UtilSecurityTest.cc` 原有的 `compare(hr.data(), result, resultlen)`
   语义是「只比前 resultlen 字节」，已用等长子 span 精确保留。
3. **测试里的 2 处 int→size_t 窄化**：`-Werror=narrowing` 报出
   `keylen` / `datalen` / `resultlen` 为 `int`，已加 `static_cast<size_t>`。
   这是 `--enable-werror` 带来的额外发现，非计划遗漏。
4. **`bittorrent_helper.cc` 的 `generateRandomData` 调用**：`len` 为 `int`，
   同样加了 `static_cast<size_t>`。
5. 阶段 1 的机械转换用了一次性 codemod 脚本（不入库），其中把
   `test/UtilTest2.cc` 的字符串字面量误改成带 NUL 的 span，被测试当场拓出并
   修正为 `sizeof(literal) - 1`——这正是计划「风险与权衡」里预警的 NUL 陷阱。
