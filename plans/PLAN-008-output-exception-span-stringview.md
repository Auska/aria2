# 任务：输出层去 C 可变参数、异常层用 `std::source_location`、残余 ptr+len 转 span、只读字符串形参转 `string_view`

> 状态：⚠️ 部分完成（2026-11-08）—— 阶段 1~3 完整落地；阶段 4 按用户决策收窄为「能干净闭环的子集」（60/672 处），其余记入「实际偏差」

## 目标

PLAN-003~007 已把语法层（`typedef`/enum/C cast/`std::ranges`/`from_chars`）、协议层、引擎层、配置层的主干现代化做完，语言基线已切 C++23。剩下的是四类**仍在头文件里暴露 C 风格接口**的结构性债：

1. **输出层**：`src/OutputFile.h` 的 `virtual int vprintf(const char* format, va_list va)` 是唯一的 C 可变参数残留，`IOFile`/`BufferedFile`/`GZipFile`/`SHA1IOFile` 各自实现 `onVprintf`，`NullOutputFile` 实现 `vprintf`；**89 处调用点**（排除 `src/IOFile.h`/`src/IOFile.cc` 的接口自身）。删掉后输出层只剩 `write`。
2. **异常层**：`DL_ABORT_EX*`/`DL_RETRY_EX*` 7 个宏 + 7 个异常类构造函数的 `(const char* file, int line, ...)` 前两个参数，靠 **16 处** `__FILE__`/`__LINE__` 填充（7 个头文件）。改用 `std::source_location` 默认参数后这 16 处全部消失。
3. **残余 ptr+len**：162 处（已排除 `Exception` 家族 `__FILE__` 假阳性），含 `BitfieldMan` 的 8 个 `ignoreBitfield/misbitfield` 系列、`Peer`/`PeerConnection` 的 `sendMessage`、`ValueBase::String::g`、`message_digest_helper::digest`、`bittorrent_helper` 6 个 helper、`Logger`、`IOFile::gets/getsn`。
4. **只读字符串形参**：`const std::string&` 在头文件里 672 处 / 159 文件（全仓 1310 处），改成 `std::string_view` 可让字面量与 `string_view` 直接传入、调用点少写 `.c_str()`。

## 当前上下文

### 仓库前置状态

- HEAD `d191e03a`（`PLAN-007: 勾选全部阶段、标记计划完成并记录实测门禁结果`），分支 `slim-features`，`git status --porcelain` 空。
- 语言基线 **C++23**：`configure.ac:111` `AX_CXX_COMPILE_STDCXX([23], [noext], [mandatory])`。g++ 14.2 下可用 `std::expected`、`std::ispanstream`、`std::source_location`（已探针验证）、`std::to_underlying`、`std::byteswap`、`std::print`。**`std::ranges::contains` 仍缺失**（`__cpp_lib_ranges_contains` 定义了但符号不存在），继续用 `std::ranges::find(x) != std::end(x)`。
- 公开 ABI 头 `src/includes/aria2/aria2.h`，哈希必须保持 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。其中 `const std::string&` **7 处**（`:267 hexToGid`、`:319`/`:329 addTorrent`、`:418 getGlobalOption`、`:762 getInfoHash`、`:806 getDir`、`:837 getOption`），属 libaria2 公开接口，**本计划不改**。
- 门禁口径（沿用 PLAN-004~007）：干净 worktree + `autoreconf -i` + `./configure --enable-werror` + `**make -j8 check**`（`-j24` 有 automake 依赖竞态 `mv: cannot stat '.deps/XXX.Tpo'`，PLAN-007 阶段 4 实测两次），判据 `# TOTAL: 1` / `# PASS: 1` / `FAIL 0` / `ERROR 0`、`test/aria2c.log` `OK (914)`、无 `Failures !!!`、`grep -c 'error:'` = 0、ABI 哈希不变。

### 各主题实测数据

| 主题 | 现量 | 分布 |
|---|---|---|
| `OutputFile::vprintf` / `printf` | 接口 2 处（`src/OutputFile.h:51,52`）+ `onVprintf` 实现 4 处（`src/IOFile.h:97`、`src/BufferedFile.cc:86`、`src/GZipFile.cc:141`、`src/SHA1IOFile.cc:64`）+ `NullOutputFile` 1 处 + **调用 89 处** | `src/version_usage.cc` 31、`src/option_processing.cc` 20、`src/bittorrent_helper.h` 13、`src/Logger.cc` 6、`src/Context.cc` 6、`src/OptionHandler.cc` 4、`src/RequestGroupMan.cc` 3、`src/util.h` 2、`src/Platform.cc` 1、`src/main.cc` 1、`src/ConsoleStatCalc.cc` 1、`src/MultiUrlRequestInfo.cc` 1 |
| printf 格式串实况 | `%s` 36、`%d` 1、`%06ld` 1 | 首参形态：字面量 62 处、`_(...)` gettext 19 处、其余 4 处；**无一处用 `%*s`/`%n`/浮点** |
| `DL_ABORT_EX*`/`DL_RETRY_EX*` | 402 站点 | `src/DlAbortEx.h:60-66` 3 个宏、`src/DlRetryEx.h:55-57` 3 个宏 |
| `__FILE__`/`__LINE__` | **16 处 / 7 头文件** | `src/DlAbortEx.h` 3、`src/DlRetryEx.h` 2、`src/DownloadFailureException.h` 3、`src/FatalException.h` 2、`src/LogFactory.h` 2、`src/OptionHandlerException.h` 2、`src/UnknownOptionException.h` 2 |
| 异常类 `(file, line, ...)` 构造 | `src/Exception.cc` 6 个、`src/RecoverableException.cc` 6 个（各 6 个重载）| `src/Exception.h:66-79`、`src/RecoverableException.h:46-60` |
| ptr+len 声明 | 162 处 / 74 文件 | `src/BitfieldMan.cc` 16、`src/BitfieldMan.h` 12、`src/bittorrent_helper.{cc,h}` 各 7、`src/DefaultPieceStorage.{cc,h}` 各 6、`src/Logger.{cc,h}` 6+5、`src/PieceStorage.h` 5、`src/UnknownLengthPieceStorage.h` 5、`src/HttpHeaderProcessor.cc` 4、`src/IOFile.h` 3、`src/PieceStatMan.*` 3 |
| `const std::string&`（头文件） | 672 处 / 159 文件 | `src/util.h` 53、`src/OptionHandlerImpl.h` 35、`src/bittorrent_helper.h` 29、`src/DownloadEngine.h` 27、`src/Request.h` 20、`src/DHTMessageFactoryImpl.h` 17、`src/DNSCache.h` 16、`src/DHTMessageFactory.h` 14、`src/SocketCore.h` 14、`src/FileEntry.h` 13 |

### 关键源码事实

- `src/OutputFile.h:48-61`：`virtual size_t write(const char* str) = 0;`（`:49`）、`virtual int flush() = 0;`（`:50`）、`virtual int vprintf(const char* format, va_list va) = 0;`（`:51`）、`inline int printf(const char* format, ...)`（`:52-60`，`va_start`/`vprintf`/`va_end`）、`virtual bool supportsColor() = 0;`（`:61`）。直接实现类 2 个：`src/IOFile.h:85`（`src/IOFile.cc:138` 调 `onVprintf`）与 `src/NullOutputFile.h:47`。
- **`onVprintf` 有 4 个实现（实测）**：`src/IOFile.h:97`（纯虚 `virtual int onVprintf(const char* format, va_list va) = 0;`）、`src/BufferedFile.h:58`+`src/BufferedFile.cc:86`、`src/GZipFile.h:54`+`src/GZipFile.cc:141`、`src/SHA1IOFile.h:59`+`src/SHA1IOFile.cc:64` —— 阶段 1 必须连这 4 个实现一起删，漏一个就是纯虚未实现。
- `src/bittorrent_helper.h:346-397` 的 `print(Output& o, ...)` 是模板，13 处 `o.printf(...)`，全部 `%s`/`%lu`/`%u` 形态，且有 `o.write("...")` 混用（说明 `write` 与 `printf` 并存是既有约定）。
- `src/version_usage.cc` 31 处、`src/option_processing.cc` 20 处是帮助文本输出，`%s` 占位对应用 `_()` 包裹的 gettext 串；gettext 的 `_()` 返回 `const char*`。**实测 `po/` 目录不存在（`ls po/*.po` = 0 个文件）**，仓库尚未建立翻译文件，所以 msgid 改动不存在翻译失配风险，19 处可放心改成 `fmt("{}{}", _("..."), arg)`（msgid 内去掉 `%s`）。
- `src/Exception.h:44-56` 的成员 `const char* file_; int line_;` 只用于 `stackTrace()`（`src/Exception.cc:103,105,120,122` 拼字符串）。改用 `std::source_location` 后成员类型变成 `std::source_location`，`file_name()`/`line()` 取代两个字段。
- `std::source_location` 探针（`/tmp/sl.cc`，`g++ -std=c++23 -Wall -Wextra -Werror`）输出正确文件名与行号，可作为默认参数。
- `src/BitfieldMan.h` 的 8 个 `ignoreBitfield/misbitfield` 系列签名：`hasMissingPiece(const unsigned char*, size_t)`、`getSparseMissingUnusedIndex(int32_t, const unsigned char*, size_t)`、`getGeomMissingUnusedIndex(int32_t, const unsigned char*, size_t, double, size_t)`、`getInorderMissingUnusedIndex` ×2、`getAllMissingIndexes(unsigned char*, size_t)` ×2、`getAllMissingUnusedIndexes(unsigned char*, size_t, const unsigned char*, size_t)`、`setBitfield(const unsigned char*, size_t)`、`setBitInternal(unsigned char*, size_t, bool)`、`getBitfield()`/`getFilterBitfield()`（**保持返回 `const unsigned char*`，PLAN-007 已确认**）。
- `src/StreamPieceSelector.h:62` 与 `src/UnknownLengthPieceStorage.h` 的 `ignoreBitfield` 参数依赖 `BitfieldMan` 的这一族，**必须与 `BitfieldMan.h` 同阶段改**，否则中间的 `shortcut:` 注释无法兑现。
- 只读形参的 672 处里，`src/util.h` 的 53 处是模板与普通函数混合（`util::strip`、`util::split`、`util::toHex`、`util::parseIntNoThrow` 等），部分是返回 `std::string` 的工具函数 —— 改 `string_view` 后内部若用 `std::string` 拼接需显式构造。
- `src/includes/aria2/aria2.h` 的 **7 处** `const std::string&` **不改**（公开 ABI）。

## 待确认决策

无。用户已在两轮提问中确认：

| 决策点 | 结果 |
|---|---|
| 纳入主题 | 第 1 题勾选：输出层 ✔、异常层 ✔、剩余 ptr+len span ✔；**只读字符串形参 `string_view` 第 1 题未勾**，第 2 题却回答了「只改头文件公共形参」—— 按后者纳入为阶段 4，**若不需要可只删阶段 4，前三阶段不受影响** |
| `string_view` 口径 | **只改头文件里的公共只读形参**（672 处 / 159 文件），实现与调用点同步 |
| `OutputFile` 策略 | **彻底删除 `vprintf`/`printf`**，接口只留 `write`/`flush`/`supportsColor` |
| 阶段与门禁 | **一主题一阶段 + 每阶段干净 worktree 门禁**（`make -j8 check`） |

## 提议的方法

### 提交顺序与理由

按「被依赖方向」从内到外：

1. **阶段 1 输出层**（`OutputFile`）—— 影响 85 个调用点，但都在叶子层（帮助文本、日志、progress），不改变任何业务类型。
2. **阶段 2 异常层**（`std::source_location`）—— `src/Exception.h` 被 100+ 文件包含，但只改构造函数前两个参数与 7 个宏；先做输出层可让异常层的 `stackTrace()` 输出同步走新的 `write`。
3. **阶段 3 残余 ptr+len → span** —— 依赖阶段 1、2 已稳定（`Logger` 的 `printf` 与 `Exception` 的构造都在这里被同一个文件触达）。
4. **阶段 4 只读形参 → `string_view`** —— 全仓最大面（159 文件），放最后，因为它的调用点最多、最容易掩盖前三个阶段的回归。

### 阶段 1：输出层

```cpp
// src/OutputFile.h（目标形态）
class OutputFile {
public:
  virtual ~OutputFile() = default;
  virtual size_t write(std::string_view str) = 0;
  virtual int flush() = 0;
  virtual bool supportsColor() = 0;
};
```

- 删 `vprintf`、`printf`、`write(const char*)`；`src/IOFile.h/.cc` 删 `onVprintf`/`vprintf`，保留 `onWrite`；`src/NullOutputFile.h` 删两处 override；**`onVprintf` 的 4 个实现全删**（`src/IOFile.h:97` 纯虚、`src/BufferedFile.*`、`src/GZipFile.*`、`src/SHA1IOFile.*`）。
- 89 处调用点改法三选一，按首参形态：
  - 纯字面量（62 处）→ `o.write("...")`，若有格式化 → `o.write(fmt("...{}", arg))`。
  - gettext 首参（19 处）→ `o.write(_("..."));` 后接需要的话 `o.write(arg);`，或 `o.write(fmt("{}{}", _("..."), arg))`。**msgid 原样保留**（去掉 msgid 里的 `%s`）。
  - `%d`/`%06ld`/`%u`（3 处，含 `src/Logger.cc:136` 的时间戳）→ `fmt("{:06}{}", tv.tv_usec, ...)`。
- `src/bittorrent_helper.h` 的模板 `print(Output& o, ...)` 同步改；`o.write(const char*)` 由 `std::string_view` 隐式接受，调用点无需改。
- 顺带（同阶段、零风险）：清掉剩余 `std::stringstream` 10 处中与输出层相关的 `src/ConsoleStatCalc.cc:247`、`src/RequestGroupMan.cc:801,822,833`（它们都写 `std::ostream&`，可改 `std::string` + `write`）。`src/ByteArrayDiskWriter.h:49`（`seekp/seekg` 语义）、`src/libsslTLSContext.cc:198`、`src/json.cc:101`、`src/RpcResponse.cc:103,151` 的流式 `operator<<` 语义**保留并加 `shortcut:` 注释**。

### 阶段 2：异常层

```cpp
// src/Exception.h（目标形态）
class Exception : public std::exception {
  std::source_location loc_;
  int errNum_;
  std::string msg_;
  error_code::Value errorCode_;
  std::shared_ptr<Exception> cause_;
public:
  Exception(const std::string& msg,
            std::source_location loc = std::source_location::current());
  Exception(const std::string& msg, error_code::Value errorCode,
            const Exception& cause,
            std::source_location loc = std::source_location::current());
  // ... 其余 4 个重载同样把 (const char* file, int line) 换成默认参数
};
```

- `src/DlAbortEx.h`、`src/DlRetryEx.h`、`src/FatalException.h`、`src/DownloadFailureException.h`、`src/OptionHandlerException.h`、`src/UnknownOptionException.h` 的构造签名同步，删掉各自的 `__FILE__, __LINE__` 宏体。
- **402 个 `DL_ABORT_EX*`/`DL_RETRY_EX*` 站点**：宏保留**名字**但改空壳（`#define DL_ABORT_EX(arg) DlAbortEx(arg)`），这样 `git grep DL_ABORT_EX` 仍可审计、400 处调用点零改动；等后续清理宏名时再删。**理由**：直接删宏会让 402 行 diff 淹没真正的逻辑改动，且用户没有要求删宏名。
- `src/LogFactory.h` 的 `A2_LOG`/`A2_LOG_EX` 宏（2 处 `__FILE__`/`__LINE__`）同样改用 `std::source_location::current()`（`Logger::log(...)` 加默认参数）。
- `src/Exception.cc:103,105,120,122` 的 `s += file_; s += ':'; s += std::to_string(line_);` 改 `s += loc_.file_name(); s += ':'; s += std::to_string(loc_.line());`。
- 行为差异（需记录）：`__FILE__` 是编译期字面量，`source_location::file_name()` 也是；但 `source_location::current()` 作为默认实参在**调用点**求值，与宏在调用点展开**语义一致**。唯一差异是内联函数中的 `file_name()` 可能指向内联点——需 `git grep -n 'DL_ABORT_EX' -- 'src/*.h'` 检查头文件里的内联包装。

### 阶段 3：残余 ptr+len → span

沿用 PLAN-004/007 的 span 规则：**输入 `std::span<const unsigned char>`、输出 `std::span<unsigned char>`、调用方显式构造 `std::span{p, n}`，不依赖数组退化**。

- `src/BitfieldMan.h` 8 个函数 + `.cc` 实现 + 调用点（`src/DefaultPieceStorage.cc`、`src/PieceStatMan.cc`、`src/StreamPieceSelector.h` 及其 4 个实现、`src/UnknownLengthPieceStorage.*`、`test/BitfieldManTest.cc`）同阶段闭环。
- `src/bittorrent_helper.h/.cc` 6 个 helper（`load`, `loadFromMemory`, `save`, `removeAnnounceUri`, `addAnnounceUri`... 实际以阶段内 `grep` 为准）。
- `src/ValueBase.h/.cc` 的 `String(const char*, size_t)` / `String::g(...)` —— **实测 `String::g` 调用点 86 处**（`git grep -c 'String::g' -- src test` 求和），超过预设阈值 40，**本阶段不改**，记入「实际偏差」。
- `src/IOFile.h:66,68` 的 `gets`/`getsn`（`fgets` 包装）**保留**：`fgets` 需要可写缓冲区，span 化不减少任何东西。
- `src/Logger.h` 的 4 个 `log(LEVEL, const char* sourceFile, int lineNum, ...)` 重载：阶段 2 把 `sourceFile`/`lineNum` 换成 `std::source_location` 后，这里的 ptr+len 债务自然消失，无需单独 span 化。

### 阶段 4：只读形参 → `string_view`

- 口径：**只改头文件里的 `const std::string&` 只读形参**（672 处 / 159 文件）；`.cc` 内部的局部 `const std::string&` 只在与新签名配套时改。
- **不改**：`src/includes/aria2/aria2.h`（公开 ABI，7 处）、返回值 `const std::string&`（虚函数 `getInfoHash()` 等，生命周期契约）、`std::string&` 可写引用（16 处，都是出参）、构造函数存成员的 `const std::string&`（要拷贝，改 `string_view` 反而要先构造 `std::string`）。
- 风险控制：`std::string_view` 与 `std::string` 混用会产生重载歧义。规则：**同一函数族要么全 `string_view` 要么全 `const std::string&`**，不新增「同时存在两个重载」的形态。每改一个文件后立即编译该文件对应的目标，而非全量。
- 典型改法：`std::string toHex(const std::string& src)` → `std::string toHex(std::string_view src)`，实现里若用 `src.c_str()` 需改 `src.substr()` 或显式 `std::string{src}`。

## 分步计划

1. [x] **阶段 1：输出层去 `vprintf`/`printf`** — `src/OutputFile.h`、`src/IOFile.{h,cc}`、`src/NullOutputFile.h`、`src/BufferedFile.*`、`src/GZipFile.*`、`src/SHA1IOFile.*`（4 个 `onVprintf` 实现）+ 12 个调用文件 + 3 处 `stringstream`；干净 worktree 门禁；提交 `PLAN-008 阶段1: 输出层移除 C 可变参数接口`。
2. [x] **阶段 2：异常层改用 `std::source_location`** — `src/Exception.{h,cc}`、`src/RecoverableException.{h,cc}`、6 个异常子类头、`src/LogFactory.{h,cc}`/`src/Logger.{h,cc}`；干净 worktree 门禁；提交 `PLAN-008 阶段2: 异常与日志改用 std::source_location`。
3. [x] **阶段 3：残余 ptr+len → `std::span`** — `src/BitfieldMan.{h,cc}` 8 个函数 + `src/DefaultPieceStorage.*`、`src/PieceStatMan.*`、`src/StreamPieceSelector.h` + 4 实现、`src/UnknownLengthPieceStorage.*`、`src/bittorrent_helper.*`、`src/ValueBase.*`、测试；干净 worktree 门禁；提交 `PLAN-008 阶段3: 残余指针加长度参数改用 std::span`。
4. [x] **阶段 4：只读形参 → `std::string_view`**（范围收窄，见下）— 从 159 头文件的 672 处里筛出能干净闭环的 32 文件 / 60 处 + 对应实现与调用点；干净 worktree 门禁；提交 `5d905cca PLAN-008 阶段4: 部分只读字符串形参改用 std::string_view`。
5. [x] **阶段 5：收尾** — 勾选、状态行、实测门禁表、实际偏差、债务前后对比；提交 `PLAN-008: 勾选全部阶段、标记计划完成并记录实测门禁结果`。

### 阶段 1 实测（2026-11-08）

- 改动 **22 文件 +152/−219**（`clang-format` 后统计）。接口收敛为 `write(std::string_view)` / `flush()` / `supportsColor()`；`onVprintf` 4 个实现全删，`src/GZipFile` 的 `buflen_`/`buf_` 成员（只服务 `onVprintf` 的伸缩缓冲）连带删除。
- 89 处调用点分布：`src/version_usage.cc` 31、`src/option_processing.cc` 20、`src/bittorrent_helper.h` 13、`src/Context.cc` 6、`src/OptionHandler.cc` 4、`src/Logger.cc` 3、`src/RequestGroupMan.cc` 3、`src/util.h` 2、`src/ConsoleStatCalc.cc`/`src/Platform.cc`/`src/main.cc`/`src/MultiUrlRequestInfo.cc` 各 1。格式串转换：`%s`/`%d`/`%lu`/`%u`→`{}`、`%06ld`→`{:06}`、`%3d`→`{:3}`、`%x`→`{:x}`；19 处 gettext 首参改 `write(fmtRuntime(_("..."), args...))`，字面量改 `write(fmt("...", args...))`，无参改 `write("...")`。
- 编译修复 5 轮：①`src/Context.cc:showFiles()` 的裸 `write(...)` 被 ADL 解析成 `::write(int, const void*, size_t)` → 改 `global::cout()->write(...)`；②`src/main.cc` 的 `main()` 在 `namespace aria2` 外，`fmt` 需写 `aria2::fmt(...)`；③`src/OptionHandler.cc`、`src/ConsoleStatCalc.cc` 补 `#include "fmt.h"`；④`src/IOFile.h` 原 `vprintf` 位置留下了重复的 `int flush() override;`（已删）；⑤`src/BufferedFile.cc`/`src/IOFile.cc` 删 `<cstdarg>`。
- 门禁（干净 worktree `/tmp/aria2-m8-p1`，base `d191e03a`）：`autoreconf -i` rc=0、`./configure --enable-werror` rc=0（`CXX = g++ -std=c++23`、`WARNCXXFLAGS = -Wall -Werror -Wformat-security`）、`make -j8 check` rc=0 → `# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`test/aria2c.log` `OK (914)`、无 `Failures`、`error:` 0、ABI 哈希 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。
- 遗留：`test/UtilSecurityTest.cc:439` 仍用 `printf`（stdio 自身的，非 `OutputFile` 接口），保留。

### 阶段 2 实测（2026-11-08）

- 改动 **19 文件 +234/−217**。`src/Exception` / `src/RecoverableException` 的 6 个构造重载全部改为「业务参数在前 + `std::source_location location = std::source_location::current()` 收尾」，`file_`/`line_` 由 `location.file_name()` / `location.line()` 填充（`errNum` 形态因此移到首参，与无 `errNum` 形态不再歧义）。
- 6 个子类（`DlAbortEx`/`DlRetryEx`/`DownloadFailureException`/`FatalException`/`OptionHandlerException`/`UnknownOptionException`）的头与实现同步；`DL_ABORT_EX*`、`DL_RETRY_EX*`、`DOWNLOAD_FAILURE_EXCEPTION*`、`FATAL_EXCEPTION*`、`OPTION_HANDLER_EXCEPTION*`、`UNKNOWN_OPTION_EXCEPTION*` 共 **16 个宏全部保留原名**，宏体只去掉 `__FILE__, __LINE__`，因此 **402 处调用点零改动**。
- `__FILE__`/`__LINE__` 在 `src` 与 `test` 中 **已清零**（`git grep` 0 命中）。
- `src/Logger`：4 个 `log()` 合并为 2 个（`const std::string&` + 可选 `const Exception&`，均带 `source_location` 默认参数）；`writeLog()` / 内部 `writeHeader()` 改收 `std::source_location`，输出格式 `[file:line]` 保持不变。`src/LogFactory.h` 的 `A2_LOG` / `A2_LOG_EX` 宏体改为 `logger->log(level, msg[, ex])`，调用点同样零改动。
- 门禁（干净 worktree `/tmp/aria2-m8-p2`，base `6434ec32`）：`autoreconf -i` rc=0、`./configure --enable-werror` rc=0（`CXX = g++ -std=c++23`）、`make -j8 check` rc=0 → `# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`test/aria2c.log` `OK (914)`、无 `Failures`、`error:` 0、ABI 哈希 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。

### 阶段 3 实测（2026-11-08）

- 改动 **63 文件 +510/−540**。`src/BitfieldMan.{h,cc}` 8 个函数、`src/StreamPieceSelector.h` + 4 个实现、`src/PieceStorage.h` + `DefaultPieceStorage`/`UnknownLengthPieceStorage`/`MockPieceStorage` 的 `getMissingPiece`/`setBitfield`/`addPieceStats`/`subtractPieceStats`/`updatePieceStats`、`src/Peer`/`PeerSessionResource`、`src/Piece`、`src/PieceStatMan`、`src/SegmentMan.cc`、`src/bittorrent_helper::createPeerMessageString` 全部改为 `std::span`。
- `createPeerMessageString(std::span<unsigned char> msg, size_t payloadLength, uint8_t messageId)`：删掉 `msgLength` 形参（由 `msg.size()` 得出），7 个产物点 + 66 处测试调用随之简化。
- 关键取舍：`getBitfield()`/`getFilterBitfield()` 返回类型保持 `const unsigned char*`（公开契约，11 处声明），所以调用点统一写 `{ptr, len}` / `std::span{p, n}` 聚合初始化；`src/BitfieldMan.cc` 内部 `copyBitfield(unsigned char* dst, ...)`、`testAllBitSet(const unsigned char*, ...)` 保持裸指针（局部辅助，无跨层调用方）。
- 明确不改（记入「实际偏差」）：`src/ValueBase.{h,cc}` 的 `String(const char*, size_t)` / `String::g(...)`（`String::g` 调用点 86 处，超阈值 40）；`src/IOFile.h` 的 `gets`/`getsn`（fgets 语义）；`src/DHTConnection.h`、`src/PeerConnection.h`、`src/GZipEncoder.h`、`src/HttpHeaderProcessor.cc`、`src/DHTMessageFactoryImpl.h`、`src/DHTRoutingTableDeserializer.cc`、`src/InternalARC4Encryptor.cc`、`src/LibsslARC4Encryptor.cc`、`src/message_digest_helper.{h,cc}` 的 ptr+len 形参（含 MS 握手 / DHT / ARC4 等协议层与外部契约，调用点跨层）；`src/bittorrent_helper.cc` 的 `getLLIntParam/getIntParam/getShortIntParam(const unsigned char* msg, size_t pos)` 是「基址 + 偏移」语义而非指针+长度，span 化不减少任何东西。
- 门禁（干净 worktree `/tmp/aria2-m8-p3`，base `c06e974c`，clean 构建）：`autoreconf -i` rc=0、`./configure --enable-werror` rc=0（`CXX = g++ -std=c++23`、`WARNCXXFLAGS = -Wall -Werror -Wformat-security`）、`make -j8 check` rc=0 → `# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`test/aria2c.log` `OK (914)`、无 `Failures`、`error:` 0、ABI 哈希 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。
- 过程教训：①本地增量 `make -C test` 会因未跟踪头文件依赖而假绿（改 `test/MockPieceStorage.h` 后只有重新触发编译的文件才看到错误），必须 `touch test/*.cc` 或干净 worktree 才能暴露；②干净 worktree 每轮只暴一批新错误，`git apply` 全部重编是唯一可靠判据。

### 阶段 4 实测（2026-11-08）

- 改动 **58 文件 +192/−152**（含 1 个测试 mock）。转换只读形参 **60 处 / 32 头文件**（`git show 5d905cca` 中头文件新增 `std::string_view` 形参计数）；`const std::string&` 净减少 112 处。
- **范围从 672 处收窄到 60 处的实测依据**（三项独立验证，结论一致）：
  1. 脚本全量转换（159 头 + 348 个 `.cc` 函数定义）后 `make -k -j24 -C src` 出 **约 150 个错误 / 45 个 `.cc`**，典型：`has no member named 'c_str'` 44、`no matching function for call to X` 28、`invalid initialization of reference of type 'const std::string&' from 'std::string_view'` 16、`no declaration matches X` 9；
  2. 逐头文件单目标验证（`make -C src <base>.o` 通过才保留）：**OK 41 文件 / 143 形参，SKIP 102 文件 / 397 形参**；但 41 文件在整仓链接期仍因 **58 个 undefined reference** 失败 —— 根因是 `.cc` 里**自由函数**（`util::strip`、`util::parseUIntNoThrow`、`cookie::parse`、`uri::parse`、`bencode2::decode`、`magnet::parse`、`rpc::getMethod` 等）没有 `Class::` 前缀，脚本的 `Xxx::yyy(` 匹配不到，头改了定义没改 → 同 TU 编译过、链接期符号对不上；
  3. 再补「头文件函数名 → `.cc` 自由函数定义」的转换后仍有大量失败：`std::string_view::const_iterator`（实为 `const char*`）赋给 `std::string::const_iterator`（`src/util.cc:245,537`、`src/json.cc:52,120`、`src/cookie_helper.cc:228`）、`could not convert 'target' from 'std::string_view' to 'std::string'`（`src/util.cc:149,330,339`）、`map<std::string,...>::find(string_view)` 不收（`src/prefs.cc:78`、`src/RpcMethodFactory.cc:258`）、`File::File(string_view)` 不收（`src/util.cc:1714,1849`）、`ostream << string_view` 与 `ColorizedStream::operator<<` 重载歧义（`src/ConsoleStatCalc.cc` 13 处）。
- 因此按用户决策（「只做能干净闭环的一小部分」）落地：只保留整仓 `make -C src` + `make -C test` 全绿且干净 worktree 门禁通过的子集，**放弃 612 处**并逐类说明：
  - 函数体用了 `c_str()`（需先 `std::string{sv}` 拷贝，等于没省拷贝，反而多一次）；
  - 函数体把形参当 `std::string::const_iterator` 的容器用（`splitIter` 族、`cookie_helper`、`json`），要整段重写成 `string_view` 迭代器语义；
  - 实参是 `std::map<std::string, ...>` 键（`find`/`erase` 不收异构键，除非 `std::less<>` 透明比较器，改动面远超本阶段）；
  - 需要 null-terminated 传给 C API（`uri_split(us, s.c_str())`、`getaddrinfo`、`stat`）；
  - 虚函数链（`OptionHandler::parse` 30+ 子类、`ColorizedStream::operator<<`）—— 少改一圈就 `marked 'override', but does not override` 或 `-Werror=overloaded-virtual`。
- 排除清单中**明确不动**（与计划原则一致）：`src/includes/aria2/aria2.h`（公开 ABI 7 处）、返回值 `const std::string&`、`std::string&` 可写出参、构造函数存成员的 `const std::string&`。
- 编译修复 3 处：①`src/bittorrent_helper.cc:284` 的 `std::function<std::string(const std::string&)>(static_cast<std::string (*)(const std::string&)>(util::percentEncode))` → 两层都改 `std::string_view`；②`test/MockBtAnnounce.h:25` 的 `createUDPTrackerRequest(const std::string&, ...)` 必须跟着 `src/BtAnnounce.h:74` 改 `std::string_view`（否则 `marked 'override', but does not override`）；③`src/ColorizedStream.h`、`src/OptionHandler.h`、`src/OptionHandlerImpl.h`、`src/AbstractOptionHandler.h` 整族回滚（歧义 + 虚函数链）。
- 门禁（干净 worktree `/tmp/aria2-m8-p4`，base `90d0c289`）：`autoreconf -i` rc=0、`./configure --enable-werror` rc=0（`CXX = g++ -std=c++23`、`WARNCXXFLAGS = -Wall -Werror -Wformat-security`）、`make -j8 check` rc=0 → `# TOTAL: 1`/`# PASS: 1`/`FAIL 0`/`ERROR 0`、`test/aria2c.log` `OK (914)`、无 `Failures`、`error:` 0、ABI 哈希 `2216641161b15acb3c3ddf2600dc19e3decd59d5`。
- 过程教训：干净 worktree 里 `test/MockBtAnnounce.h` 已改对，但 `test/BtRegistryTest.o` 仍是 patch 应用前的旧对象 → 复现 `marked 'override', but does not override` 假错误。`make -C test BtRegistryTest.o` 单独重编即可；根因是 `rm -rf` 后 `git worktree add` 复用 `/tmp` 目录时 `.deps` 残留。

## 预计修改的文件

**阶段 1**（22 文件）：`src/OutputFile.h`、`src/IOFile.h`、`src/IOFile.cc`、`src/NullOutputFile.h`、`src/BufferedFile.h`、`src/BufferedFile.cc`、`src/GZipFile.h`、`src/GZipFile.cc`、`src/SHA1IOFile.h`、`src/SHA1IOFile.cc`、`src/version_usage.cc`、`src/option_processing.cc`、`src/bittorrent_helper.h`、`src/Logger.cc`、`src/OptionHandler.cc`、`src/RequestGroupMan.cc`、`src/util.h`、`src/Context.cc`、`src/Platform.cc`、`src/main.cc`、`src/ConsoleStatCalc.cc`、`src/MultiUrlRequestInfo.cc`。

**阶段 2**（12 文件）：`src/Exception.h`、`src/Exception.cc`、`src/RecoverableException.h`、`src/RecoverableException.cc`、`src/DlAbortEx.h`、`src/DlRetryEx.h`、`src/FatalException.h`、`src/DownloadFailureException.h`、`src/OptionHandlerException.h`、`src/UnknownOptionException.h`、`src/LogFactory.h`、`src/Logger.h`、`src/Logger.cc`。

**阶段 3**（约 20 文件）：`src/BitfieldMan.h`、`src/BitfieldMan.cc`、`src/DefaultPieceStorage.h`、`src/DefaultPieceStorage.cc`、`src/PieceStatMan.h`、`src/PieceStatMan.cc`、`src/StreamPieceSelector.h`、`src/DefaultStreamPieceSelector.*`、`src/GeomStreamPieceSelector.*`、`src/InorderStreamPieceSelector.*`、`src/RandomStreamPieceSelector.*`、`src/UnknownLengthPieceStorage.h`、`src/UnknownLengthPieceStorage.cc`、`src/PieceStorage.h`、`src/bittorrent_helper.h`、`src/bittorrent_helper.cc`、`src/ValueBase.h`、`src/ValueBase.cc`、`test/BitfieldManTest.cc`、`test/bitfieldTest.cc`、`test/BittorrentHelperTest.cc`、`test/ValueBaseTest.cc`。

**阶段 4**（159 个头文件 + 对应 `.cc`，按 `const std::string&` 出现数排序取前 20 优先）：`src/util.h`(53)、`src/OptionHandlerImpl.h`(35)、`src/bittorrent_helper.h`(29)、`src/DownloadEngine.h`(27)、`src/Request.h`(20)、`src/DHTMessageFactoryImpl.h`(17)、`src/DNSCache.h`(16)、`src/DHTMessageFactory.h`(14)、`src/SocketCore.h`(14)、`src/FileEntry.h`(13)、`src/util_security.h`(12)、`src/CookieStorage.h`(11)、`src/HttpServer.h`(11)、`src/DownloadContext.h`(10)、`src/HttpHeader.h`(10)、`src/HttpRequest.h`(10)、`src/Netrc.h`(9)、`src/cookie_helper.h`(9)、`src/MessageDigest.h`(8)、`src/TimeA2.h`(8)、`src/crypto_hash.h`(8)。

## 测试与验证

每阶段门禁（`/tmp/aria2-m8-p<N>`）：

```bash
git worktree prune && rm -rf /tmp/aria2-m8-p<N>
git worktree add --detach /tmp/aria2-m8-p<N> HEAD
git -C /tmp/aria2-m8-p<N> apply /tmp/p8-stage<N>.patch
cd /tmp/aria2-m8-p<N> && autoreconf -i && ./configure --enable-werror
make -j8 check > /tmp/p8-p<N>.log 2>&1; echo rc=$?
```

通过标准（全部满足）：
- `make check` rc=0；`grep -c 'error:' /tmp/p8-p<N>.log` = 0
- `# TOTAL: 1` / `# PASS: 1` / `# FAIL: 0` / `# ERROR: 0`
- `test/aria2c.log` 出现 `OK (914)`，无 `Failures !!!`
- `git hash-object src/includes/aria2/aria2.h` = `2216641161b15acb3c3ddf2600dc19e3decd59d5`
- `Makefile` 中 `CXX = g++ -std=c++23`、`WARNCXXFLAGS` 含 `-Wall -Werror -Wformat-security`

阶段专项验证：

| 阶段 | 命令 | 通过标准 |
|---|---|---|
| 1 | `git grep -cE '\b(v?printf)\s*\(' -- 'src/*.h' 'src/*.cc'` | `src/OutputFile.h` / `src/IOFile.{h,cc}` / `src/NullOutputFile.h` / `src/BufferedFile.*` / `src/GZipFile.*` / `src/SHA1IOFile.*` 全部 0 命中；其余文件的 `printf` 只剩真实 C API（`snprintf` 不在正则内） |
| 1 | `git grep -c 'va_list' -- src` | 只允许出现在 `src/util.h` 等真实 C API 包装处（阶段内逐个确认） |
| 2 | `git grep -nE '__FILE__\|__LINE__' -- src` | 0 命中 |
| 2 | 手动跑 `./src/aria2c --help > /dev/null` + 一个非法参数（`./src/aria2c --nope`） | 错误信息里的 `file:line` 仍指向真实调用行（不是宏所在行） |
| 3 | `git grep -nE '(unsigned char\|char)\s*\*\s*\w+\s*,\s*(size_t\|int)\s+\w+' -- src \| grep -v Exception` | 162 → 显著下降；保留项逐个在「实际偏差」说明理由 |
| 4 | `git grep -c 'const std::string&' -- 'src/*.h' \| awk -F: '{s+=$2} END {print s}'` | 672 → 7（只剩 `src/includes/aria2/aria2.h` 的 7 处公开 ABI） |

**假绿风险**：
- 原地增量 `make` 会复用旧 `.o`，改头文件后不重编依赖它的目标 → 每阶段必须干净 worktree 全量。
- `make -j24` 有 automake 依赖文件竞态（`mv: cannot stat '.deps/XXX.Tpo'`），用 `-j8`。
- `OK (914)` 不覆盖 `LibuvEventPoll`（`--with-libuv` 才编译）与 `--disable-bittorrent` 分支 → 阶段 3 若碰到 `ENABLE_BITTORRENT` 内的文件，需人工 diff 核对 `src/Makefile.am` 的 `if` 块。
- `std::string_view` 的重载歧义**可能编译通过但选了错误重载**（例如 `foo(std::string)` vs `foo(std::string_view)` 同时存在时字面量选后者）→ 阶段 4 每改一族函数都 `git grep -n 'template.*\bfoo\b'` 确认无并存重载。

## 验收标准

1. `src/OutputFile.h` 只有 `write(std::string_view)`/`flush`/`supportsColor` 三个虚函数，无 `printf`/`vprintf`/`va_list`。
2. `git grep -nE '__FILE__|__LINE__' -- src` = 0；异常对象仍能打印正确的 `file:line`（手工冒烟）。
3. `src/BitfieldMan.h` 不含 `const unsigned char*, size_t` 形态的 `ignoreBitfield` 参数；`getBitfield()`/`getFilterBitfield()` 返回值不变。
4. 头文件里 `const std::string&` 只读形参：原定清零，**实测 672→575 处（净减 97）**（阶段 4 按用户决策只做 60 处），残余逐类在「实际偏差」说明。
5. `git hash-object src/includes/aria2/aria2.h` = `2216641161b15acb3c3ddf2600dc19e3decd59d5`。
6. 4 次代码提交 + 1 次文档收尾，每次都有干净 worktree 门禁记录。
7. `configure.ac`、`src/Makefile.am`、`test/Makefile.am`、`.github/workflows/build.yml`、7 个 `src/*.c` shim、`README.rst` 不动。

## 风险与权衡

| 风险 | 影响 | 缓解 |
|---|---|---|
| 阶段 1 的 19 处 gettext 首参 | ~~msgid 改动导致 `.po` 失配~~ **已排除**：实测 `po/` 目录不存在，仓库无翻译文件 | 无需缓解；改后 msgid 内不含 `%s`，将来建 `po/` 时按新 msgid 生成即可 |
| 阶段 2 内联函数里的 `source_location::current()` | 默认实参在调用点求值，但若被内联包装包裹，`file_name()` 可能指向包装函数而非业务调用点 | 阶段内 `git grep -n 'DL_ABORT_EX' -- 'src/*.h'` 找出头文件里的内联包装（实测 `src/bittorrent_helper.h` 等模板内），逐个改写为显式传 `loc` |
| 阶段 2 改 402 处宏站点 | diff 巨大、review 困难 | 宏名保留、宏体改空壳，402 处调用点零改动 |
| 阶段 3 `ValueBase::String::g` 消费点 > 40 | diff 失控 | 预设判断标准：调用点 > 40 则本阶段不改，写进残余 |
| 阶段 4 `string_view` 悬垂引用 | `string_view` 指向临时 `std::string` 被销毁 → UB | 只改**形参**不改成员；实现里若需保存必须显式 `std::string` 拷贝；`-Werror`+914 例做基本兜底 |
| 阶段 4 重载歧义 | 编译通过但选错重载，行为静默变化 | 同族函数全改或全不改；每文件改动后单文件编译 |
| CI 用 clang-18，本机无 clang++ | 阶段 2 的 `source_location` 与阶段 4 的 `string_view` 在 clang-18 下未本地验证 | C++23 基线已切（PLAN-007），clang-18 支持两者；CI 首跑若失败，最小回退是阶段 2 保留 `__FILE__` 宏 |

## 自查

1. **引用真实**：上表所有 `文件:行` 已用 `grep -n`/`git grep -c` 逐条核对（`src/OutputFile.h:44-59`、`src/IOFile.h:85`、`src/IOFile.cc:136`、`src/NullOutputFile.h:47`、`src/DlAbortEx.h:60-66`、`src/DlRetryEx.h:55-57`、`src/Exception.h:66-79`、`src/Exception.cc:103,105,120,122`、`src/BitfieldMan.h:109,135,154,166,177,181,184,187,229`、`src/StreamPieceSelector.h:62`、`src/IOFile.h:66,68`、`src/includes/aria2/aria2.h:267,319,329,418,762,806,837`）；计数全部来自剥注释正则或 `git grep -c`，非估算。
2. **阶段可编译、测试不假绿**：每阶段都列了调用点清单与 grep 命令；`OutputFile` 只有 2 个实现类（已确认）；异常类 6 个重载都在 `src/Exception.cc`/`src/RecoverableException.cc`（已确认）；`BitfieldMan` 8 个函数与 `StreamPieceSelector`/`UnknownLengthPieceStorage` 同阶段闭环；每阶段干净 worktree 全量重编译（原地增量会假绿，PLAN-004 已实测）。
3. **覆盖所有分支与消费点**：「`OutputFile` 的 89 处调用点」按文件逐个列出；「`__FILE__/__LINE__` 16 处 / 7 文件」逐个列出；`src/includes/aria2/aria2.h` 的 7 处 `const std::string&` 明确排除；`--with-libuv` / `--disable-bittorrent` 分支不覆盖，已在假绿风险里声明。

## 备注

- **审查 #1**（自查发现 → 已修订）：初稿写「85 处调用点 / 实现类 2 个」，实测 `git grep -cE '\b(v?printf)\s*\('` 排除接口自身后是 **89 处**（`src/Context.cc` 实际 6 处不是 2 处），且 `onVprintf` 有 **4 个实现**（漏了 `src/BufferedFile.*`）—— 已在「当前上下文」与阶段 1 清单改正。
- **审查 #2**（自查发现 → 已修订）：初稿写 `__FILE__`/`__LINE__` 32 处、`aria2.h` 的 `const std::string&` 9 处；实测分别是 **16 处** 与 **7 处**（`git grep -nE '__FILE__|__LINE__' -- src` 逐行数、`git grep -n 'const std::string&' -- src/includes/aria2/aria2.h` 逐行数）—— 已全文改正（含验收标准与专项验证的判据）。
- **审查 #3**（自查发现 → 已修订）：初稿把「gettext msgid 失配」列为风险，实测 `po/` 目录不存在（0 个 `.po`），风险不存在 —— 已改为「已排除」，19 处 gettext 首参可放心改 `fmt`。
- 用户授权状态：**尚未说「开始执行」**，本次只写计划文档，未改任何 `src/**` 代码。
- 第 1 题未勾 `string_view`、第 2 题选了「只改头文件公共形参」——按后者执行，范围已写明；若用户只想做前三项，删除阶段 4 即可，其余不受影响。
- 上一轮盘点里的 `memcpy`(71)/`memset`(37)/`hton*`(37)/`errno`(43)/`sscanf`(5) **不在本计划**：前四者是正确的 C 语义（字节序、摘要算法、系统错误码），`sscanf` 5 处已在 PLAN-007 记为可解释残余。
- `src/*.c` 7 个 shim、`src/uri_split.h` 的 3 个 C typedef、`src/error_code.h` 与 `src/uri.cc` 的 2 个 enum 按既有决定保持不动。

## 实测门禁结果（2026-11-08）

| 阶段 | 提交 | 干净 worktree | 构建与测试 | ABI 哈希 |
|---|---|---|---|---|
| 1 输出层 | `6434ec32` | `/tmp/aria2-m8-p1`（base `d191e03a`） | `# TOTAL: 1`/`# PASS: 1`、`OK (914)`、`error:` 0 | `2216641161b15acb3c3ddf2600dc19e3decd59d5` |
| 2 异常层 | `c06e974c` | `/tmp/aria2-m8-p2`（base `6434ec32`） | 同上 | 同上 |
| 3 ptr+len→span | `90d0c289` | `/tmp/aria2-m8-p3`（base `c06e974c`） | 同上 | 同上 |
| 4 string_view（收窄） | `5d905cca` | `/tmp/aria2-m8-p4`（base `90d0c289`） | 同上 | 同上 |
| 5 收尾 | 本文档提交 | — | — | — |

四轮门禁均为干净 worktree `autoreconf -i` + `./configure --enable-werror`（`CXX = g++ -std=c++23`、`WARNCXXFLAGS = -Wall -Werror -Wformat-security`）+ `make -j8 check`，rc 均为 0。

## 债务前后对比

| 指标 | 阶段前 | 阶段后 | 说明 |
|---|---|---|---|
| `OutputFile::vprintf` 及 `onVprintf` 实现 | 1 接口 + 4 实现 | **0** | 输出层只剩 `write(std::string_view)` |
| `printf` 调用点（`OutputFile` 接口） | 89 | **0** | 余 1 处 `test/UtilSecurityTest.cc:439` 是 stdio 自身 |
| `__FILE__` / `__LINE__` | 16（7 头文件） | **0** | 改 `std::source_location`，16 个宏名与 402 处调用点零改动 |
| ptr+len 形参（`git grep -nE '(unsigned char|char)\s*\*\s*\w+\s*,\s*(size_t|int)\s+\w+' -- src \| grep -v Exception`） | 162 | **58** | `BitfieldMan` 8 函数、`PieceStorage` 族、`Peer`/`Piece`/`PieceStatMan`、`bittorrent_helper::createPeerMessageString`、`StreamPieceSelector` 全族已转 span |
| 头文件 `const std::string&` 只读形参 | 672 | **575** | 阶段 4 只做可干净闭环的 60 处（净减 97 处） |
| 剩余 `std::stringstream` | 10 处 / 6 文件 | 10 处 / 6 文件 | 计划外，PLAN-007 已登记为可解释残余 |

## 实际偏差

1. **阶段 4 范围从 672 处收窄到 60 处（32 头文件）**。全量转换实测 397 处在编译期失败、143 处触达 `src/util.*` 等共享层后链接失败（58 个 undefined reference）。根因：`.cc` 自由函数无 `Class::` 前缀不被脚本覆盖；`string_view::const_iterator`（`const char*`）与 `std::string::const_iterator` 不可互换；`std::map<std::string,...>::find` 不收异构键；多处需 null-terminated 传 C API；`ColorizedStream::operator<<` 与 `OptionHandler::parse` 虚函数族整改成本远超收益。经用户选择「只做能干净闭环的一小部分」，保留整仓全绿子集，612 处逐类说明见「阶段 4 实测」。
2. **阶段 3 的 `getBitfield()`/`getFilterBitfield()` 未改返回类型**（11 处声明），仍是 `const unsigned char*`：它们是 `PieceStorage`/`Peer`/`Piece`/`BitfieldMan`/`MockPieceStorage` 的契约，改 `std::span` 要连带 34 个消费点，且 span 不持有所有权易踩悬垂。调用点改用 `{ptr, len}` 聚合初始化。
3. **阶段 3 未改 `String::g` 族**（`src/ValueBase.{h,cc}`，调用点 86 处 > 阈值 40）与 `IOFile::gets/getsn`（fgets 语义），按计划预设的阈值规则保持。
4. **`src/ColorizedStream.h` / `src/OptionHandler.h` / `src/OptionHandlerImpl.h` / `src/AbstractOptionHandler.h` 整族回滚**：`operator<<(ColorizedStream&, std::string_view)` 让 `ConsoleStatCalc.cc` 13 处 `o << "x"` 变歧义；`OptionHandler::parse` 是 30+ 子类的虚函数链，需整族同改（含 `DeprecatedOptionHandler`、`test/` 下的 handler mock），超出本阶段预算。
5. **门禁必须用 `-j8`**：`-j24` 会撞 automake 依赖竞态 `mv: cannot stat '.deps/XXX.Tpo'`（PLAN-007 阶段 4 实测两次，本计划沿用 `-j8`）。
6. **干净 worktree 复用目录会残留旧 `.o`**：`/tmp/aria2-m8-p4` 第二次 `git worktree add` 后 `test/BtRegistryTest.o` 仍是旧对象，编译报已修好的 `marked 'override', but does not override` 假错误；`make -C test <Target>.o` 单独重编即可。稳妥做法是 `rm -rf` 目标目录后再 `git worktree add`。
7. **clang-18 仍未本地验证**：本机无 `clang++`，C++23 + `std::source_location` + `std::span` + `std::string_view` 只经 g++ 14.2 验证；CI（`.github/workflows/build.yml` 用 `clang-18`）首跑若失败，最小回退是阶段 2 保留 `__FILE__` 宏。
