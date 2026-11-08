# 任务：配置 / RPC / CLI 层现代化（cfg79 口径）

## 目标

把 PLAN-003/004/005 未覆盖的最后一层——配置、RPC 与 CLI（79 文件 /
12 553 行）——按同样的四项口径收口：语言惯用法、span 化、错误处理、
标准库替换。其中 `src/OptionHandlerFactory.cc` 的 **200 处裸 `new`** 是本层
最大债务，按用户决策改为 `std::unique_ptr` 全链所有权。

完成后全仓 `src/` 三层（协议 356 / 引擎 415 / 配置 79 + 7 个 `.c` shim）
全部走同一套 C++20 惯用法，仅剩已登记的可解释残余。

## 当前上下文

### 仓库与前置状态

- 仓库 `/home/code/tmp/aria2`，分支 `slim-features`，HEAD `114bdc8f`
  （PLAN-005 收尾），`git status --porcelain` 为空。`plans/` 已有
  PLAN-001~005，序号 006 未占用。
- 前面五轮已完成：PLAN-003 基础层 6 簇、PLAN-004 协议层 4 簇（356 文件）、
  PLAN-005 引擎层 4 簇（415 文件）。三层合计 126 文件 +1369/−703（PLAN-005）。
- 语言基线未变：`configure.ac:111` `AX_CXX_COMPILE_STDCXX([20], [noext], [mandatory])`；
  `configure.ac:557-566` 的 `--enable-werror` 展开为
  `WARNCXXFLAGS = -Wall -Werror -Wformat-security`。本机 `g++ 14.2.0`、
  `autoconf 2.72`、`automake 1.17`、`cppunit 1.15.1`。
- 门禁继承前五轮：干净 `git worktree` + `autoreconf -i` +
  `./configure --enable-werror` + `make -j"$(nproc)" check`，判据
  `test/aria2c.log` `OK (914)`、`# TOTAL: 1` / `# PASS: 1`、无
  `Failures !!!`、`git hash-object src/includes/aria2/aria2.h` =
  `2216641161b15acb3c3ddf2600dc19e3decd59d5`。
- 原地增量 `make` 会假绿（PLAN-003/004/005 实测），所有验证必须在干净副本。

### 模块范围（cfg79）

集合运算闭合（实测）：

```
all858 (git ls-files 'src/*.h' 'src/*.cc' 'src/*.c')
  − proto356 (PLAN-004 四组前缀)
  − engine415 (PLAN-005)
  − shims7 (src/*.c)
  = cfg80 → 去掉 src/includes/aria2/aria2.h = cfg79 (12 553 行)
```

`src/includes/aria2/aria2.h` 是公开 C API 头，全仓哈希断言必须不变，且它含 6 个
`enum` / 3 个 `typedef`；本轮**只读不改**，也不计入任何阶段的债务清零。

三阶段切分（脚本校验：33 + 20 + 26 = 79，无重叠、无遗漏）：

| 阶段 | 文件 | 行数 | 主题 |
| --- | --- | --- | --- |
| P1 配置与选项 | 33 | 5 682 | `Option*`/`prefs`/`SessionSerializer`/`MultiUrlRequestInfo`/`OptionHandlerFactory` |
| P2 RPC | 20 | 2 827 | `Rpc*`/`json`/`aria2api`/`Notifier`/`bignum`/`error_code.h` |
| P3 CLI 与输出 | 26 | 4 044 | `main`/`console`/`Logger`/`LogFactory`/`ColorizedStream`/`help_tags`/`uri`/`FeatureConfig` |

### 债务实测（剥注释正则，cfg79）

总计：`typedef 12` / 非 scoped `enum 8` / 裸 `new 204` / `std::find 2` /
`std::stringstream 7` / `str→num 1` / `.c_str() 30` / `printf 13` / `NULL 0` /
`std::endl 0`。

按阶段：

- **P1**：`new 201`（`src/OptionHandlerFactory.cc` 200 + `src/wallclock.cc:43` 1）、
  `typedef 1`（`src/SessionSerializer.cc:146 typedef T type;`）、`enum 2`
  （`src/OptionHandler.h:87 ARG_TYPE`、`src/help_tags.h:42 HelpTag`）、
  `str→num 1`（`src/OptionHandlerFactory.cc:1796 sscanf(PACKAGE_VERSION, "%d.%d.%d", ...)`
  —— 版本号三段解析，`from_chars` 不适用，P1 保留并加 `shortcut:`）、
  `.c_str() 19`、`printf 0`。
- **P2**：`typedef 3`（`src/bignum.h:29,30`、`src/RpcMethodImpl.h:336`）、
  `enum 2`（`src/RpcResponse.h:50 authorization_t`、`src/FeatureConfig.h:47 FeatureType`）、
  `stringstream 3`（`src/RpcResponse.cc:101,148`、`src/json.cc:98`）、`.c_str() 2`。
- **P3**：`typedef 8`（`src/ColorizedStream.h:80,83,84`、`src/uri_split.h:45,59,63`、
  `src/console.h:46`、`src/HashFuncEntry.h:49`）、`enum 3`
  （`src/ColorizedStream.h:82 part_t`、`src/Logger.h:50 LEVEL`、
  `src/uri.cc:212 NPATH_*` 状态机）、`stringstream 4`
  （`src/ColorizedStream.cc:64,78`、`src/FeatureConfig.cc:176,217`）、
  `find 1`（`src/HashFuncEntry.h:76 std::find_if`）、`new 3`
  （`src/usage_text.h` 3 处是帮助文本里的英文单词 "new"，误命中；
  `src/wallclock.cc:43 static auto t = new Timer();` 计入 P1 也可，本计划统一放 P1）、
  `printf 13`（`src/Logger.cc` 6、`src/OptionHandler.cc` 4 属 P1、
  `src/MultiUrlRequestInfo.cc` 1 属 P1、`src/main.cc` 1、`src/Platform.cc` 1）、
  `.c_str() 9`。

### 关键现状（读过的源码事实）

**所有权链（P1 核心）**

- `src/OptionHandlerFactory.h:48 static std::vector<OptionHandler*> createOptionHandlers();`
  函数体 196 处 `OptionHandler* op(new XxxOptionHandler(...));` + 197 处
  `handlers.push_back(op);`，末尾 `return handlers;`。构造分类：
  `Boolean 66` / `Default 33` / `Number 33` / `LocalFilePath 24` /
  `UnitNumber 13` / `Parameter 12` / `HttpProxy 4` / `Deprecated 3` /
  `IntegerRange 3` / `FloatNumber 2` / `HostPort 2` / 其余 5 类各 1。
- `src/OptionParser.cc:369` 里 `optionParser_->setOptionHandlers(OptionHandlerFactory::createOptionHandlers());`
  → `src/OptionParser.cc:273-279 setOptionHandlers(const std::vector<OptionHandler*>&)`
  → `src/OptionParser.cc:281-290 addOptionHandler(OptionHandler*)` 按
  `handler->getPref()->i` 存进 `src/OptionParser.h:56 std::vector<OptionHandler*> handlers_;`
  （ctor `:61 handlers_(option::countOption(), nullptr)`）。
- 析构 `src/OptionParser.cc:65-68 std::for_each(handlers_.begin(), handlers_.end(), Deleter());`
  （`src/a2functional.h:57-60 template <class T> void operator()(T* ptr) { delete ptr; }`）。
- `src/OptionHandlerImpl.h:275-276 OptionHandler* depOptHandler_; const OptionHandler* repOptHandler_;`
  + `src/OptionHandlerImpl.cc:667-671 ~DeprecatedOptionHandler() { delete depOptHandler_; }`
  —— 3 处工厂调用是 `new DeprecatedOptionHandler(new BooleanOptionHandler(...))`，
  内层指针由 `DeprecatedOptionHandler` 接管所有权。
- `handlers_` 的**空槽语义**必须保留：`src/OptionParser.cc:345-350 findById()` 在
  `handlers_[id]` 为 `nullptr` 时回退 `handlers_[0]`，`prefs` 里并非每个
  `Pref` 都有 handler（`handlers_` 尺寸 = `option::countOption()`，
  `src/prefs.cc:104 size_t countOption() { return getPrefFactory()->getCount(); }`）。
- 读取侧全是 `const OptionHandler*`（`src/version_usage.cc:96,118` 的
  `std::vector<const OptionHandler*>`、`src/SessionSerializer.cc:118`、
  `src/option_processing.cc:140,243,287`、`src/aria2api.cc:199,585`、
  `src/RpcMethodImpl.cc:1118,1157`、`src/RpcMethod.cc:116`），改容器类型不需要动它们。

**枚举引用规模（决定成本）**

- `src/OptionHandler.h:87 enum ARG_TYPE { REQ_ARG, OPT_ARG, NO_ARG }`：99 处引用，
  含 `src/OptionHandlerImpl.h:54,132,151` 的默认实参 `OptionHandler::REQ_ARG`、
  `src/AbstractOptionHandler.h:62 ARG_TYPE argType = REQ_ARG`、
  `src/OptionHandlerImpl.cc:741 OptionHandler::ARG_TYPE DeprecatedOptionHandler::getArgType() const`。
- `src/help_tags.h:42 enum HelpTag { TAG_BASIC … TAG_HELP, MAX_HELP_TAG }`：314 处
  `TAG_*`，其中 ~285 在 `src/OptionHandlerFactory.cc`；`src/AbstractOptionHandler.cc:73-85`
  用 `tags_` 位图（`tags_ |= 1 << tag`、`(tags_ & (1 << i))`），
  `src/OptionHandler.h:69,71 virtual bool hasTag(uint32_t tag) / addTag(uint32_t tag)`
  是**位下标接口**（不是枚举形参），`src/OptionParser.h:82 findByTag(uint32_t)` 同。
  `src/help_tags.cc:50-70` 的 `strHelpTag(uint32_t)` / `idHelpTag(const char*)`
  也是 `uint32_t` 接口。
- `src/RpcResponse.h:50 enum authorization_t { NOTAUTHORIZED, AUTHORIZED }`：9 处
  （`src/RpcResponse.h:56,58,64`、`src/RpcResponse.cc:50`、`src/RpcMethod.cc:93,96`、
  `src/RpcMethodImpl.cc:1367,1387`、`src/rpc_helper.cc:55`、`test/RpcResponseTest.cc:26,46`）。
- `src/FeatureConfig.h:47 enum FeatureType { FEATURE_ASYNC_DNS … MAX_FEATURE }`：17 处，
  但公开接口是 `const char* strSupportedFeature(int feature)`（`src/FeatureConfig.h:62`），
  调用方 `src/FeatureConfig.cc:79-86`（`for (first = 0; first < MAX_FEATURE; ...)`）、
  `src/RpcMethodImpl.cc:1102`、`test/FeatureConfigTest.cc:45`。
- `src/Logger.h:50 enum LEVEL { A2_DEBUG = 1 << 0 … A2_ERROR = 1 << 4 }`：37 处，
  含 `src/LogFactory.h:117,134` 的宏 `A2_LOG_DEBUG(msg) A2_LOG(Logger::A2_DEBUG, msg)`。
  是位掩码语义（`fileLogEnabled` 用 `level >= logLevel_`）。
- `src/ColorizedStream.h:82 enum part_t { eColor, eString }`：6 处
  （`src/ColorizedStream.h:83,88,92,93`、`src/ColorizedStream.cc:66,80`）。
- `src/uri.cc:212 enum { NPATH_START, NPATH_SLASH, NPATH_SDOT, NPATH_DDOT, NPATH_PATHCOMP }`：
  21 处，纯文件内状态机。
- `src/error_code.h:44 enum Value { UNDEFINED = -1, FINISHED = 0, … CHECKSUM_ERROR = 32 }`：
  **58 文件 / 227 处**，31 个 `src/*.h` 有 `error_code::Value` 成员或形参
  （`src/Exception.h:57,69,75,80,90` 等）。**用户决策：不改。**

**其他事实**

- `src/OptionHandlerFactory.cc:1796 std::sscanf(PACKAGE_VERSION, "%d.%d.%d", &major, &minor, &micro);`
  只为拼 peer id 前缀；`snprintf` 已换 `fmt()`，`sscanf` 保留并加 `shortcut:`
  （三段点号分隔的版本号，逐段 `from_chars` 反而更长且要处理缺段）。
- `src/wallclock.cc:43 static auto t = new Timer();` 是函数内静态单例（永不释放，
  刻意不析构），改 `std::unique_ptr` 语义等价但需要 `<memory>`；判定保留 + `shortcut:`。
- `src/HashFuncEntry.h:49 typedef HashFunc HashFuncType;` + `:76 std::find_if(...)`；
  `src/SessionSerializer.cc:146` 是 `template <typename T> class Unique { typedef T type; ... }`
  里的 `type` 别名（`std::set<const type*, PointerCmp> known;`）。
- `src/bignum.h:29,30` 两个 `typedef` 在 `template <size_t dim> class ulong` 内；
  该头只被 `src/InternalDHKeyExchange.h` 引用，`USE_INTERNAL_BIGNUM` 未定义 →
  **本机不编译**，只能靠人工核对。
- `src/uri_split.h:45,59,63` 是 **C 头**（`typedef enum {...} uri_split_field;`、
  `typedef enum { USF_IPV6ADDR = 1 } uri_split_flag;`、`typedef struct {...} uri_split_result;`），
  与 `src/uri_split.c`（纯 C）共用；`src/uri.cc:98-152` 用 `res.fields[USR_SCHEME]`
  当数组下标、`res.field_set & (1 << USR_PATH)` 当位掩码 → **不能 enum class 化**。
- `src/console.h:46 typedef std::shared_ptr<OutputFile> Console;` → `using` 零影响
  （`src/console.cc`、`src/main.cc`、`src/version_usage.cc` 等 30+ 处只用 `Console` 名字）。
- `src/RpcMethodImpl.h:336 typedef IndexedList<a2_gid_t, std::shared_ptr<T>> ItemListType;`
  在 `class RpcMethodImpl` 的派生模板里 → `using` 零影响。
- `.c_str()` 30 处的真实动因是 `src/fmt.h:46-49` 的 `fmt(std::format_string<Args...>, Args&&...)`
  （PLAN-003 阶段 1 已把 fmt 全量迁到 `std::format`）；大多数 `fmt("...{}...", x.c_str())`
  可去掉 `.c_str()`。真正需要的：`src/TimeA2.cc:81-82 strptime(datetime.c_str(), format.c_str(), &tm)`
  （`strptime` 收 `const char*`）、`src/OptionParser.cc:166 getopt_long(..., optstring.c_str(), ...)`、
  `src/uri.cc:92 const char* p = uri.c_str();`（游标遍历）、`src/ColorizedStream.cc:88 rv.write(e.second.c_str(), max)`。
- `printf` 13 处全是 `OutputFile::printf`（`src/OutputFile.h:51-52` 的
  `virtual int vprintf(const char* format, va_list va)` 接口），**不属标准库
  `printf`**，按 PLAN-005 同一口径保留。

### 已确认决策（用户 4 个问题一次性回答）

| # | 决策点 | 结论 |
| --- | --- | --- |
| 1 | 范围口径 | **cfg79**：80 个配置层文件里排除 `src/includes/aria2/aria2.h`（公开 ABI 头不动） |
| 2 | 200 处裸 `new` | **工厂改 `unique_ptr` 全链**：`createOptionHandlers()` 返回 `std::vector<std::unique_ptr<OptionHandler>>`，`OptionParser::handlers_` 同步，删 `Deleter()` 循环 |
| 3 | 枚举改造程度 | **除 `error_code::Value` 外全改 `enum class`** |
| 4 | 阶段划分 | **3 阶段：配置核心 / RPC / CLI 与输出**，每阶段干净 worktree 门禁 + 一次提交 |

## 待确认决策

无。

## 提议的方法

### 提交顺序

P1 → P2 → P3，每阶段一次提交 + 门禁。依赖关系：P1 改 `HelpTag` / `ARG_TYPE`
会波及 P2 的 `src/RpcMethodImpl.cc`（`TAG_*` 0 处、`ARG_TYPE` 0 处，实测无耦合）
与 P3 的 `src/version_usage.cc`（`TAG_BASIC` 2 处、`ARG_TYPE` 0 处）；
`src/version_usage.cc` 属 PLAN-005 的 A 簇已现代化文件，本计划只需单行适配，
在 P1 提交内一并改（跨层「完整清理调用点」由用户第 3 问精神允许）。

### 工厂 `unique_ptr` 全链（P1 核心）

1. `src/OptionHandlerFactory.h:48` →
   `static std::vector<std::unique_ptr<OptionHandler>> createOptionHandlers();`
2. `src/OptionHandlerFactory.cc:48` 函数体内 196 处
   `OptionHandler* op(new XxxOptionHandler(...));` → `auto op = std::make_unique<XxxOptionHandler>(...);`
   （`op->addTag(...)` / `op->hide()` / `handlers.push_back(op)` 全部照旧可用）；
   197 处 `handlers.push_back(op);` 中 196 处改 `handlers.push_back(std::move(op));`
   —— 用脚本按「`OptionHandler* op(new` → `auto op = std::make_unique<`、配对的
   右括号位置插入 `)`无法机械完成，改为逐处 sed 规则后 `-Werror` 兜底：
   `OptionHandler\* op\(new (X)(\(.*\))\);` → `auto op = std::make_unique<X>$2;`。
   `auto op(` 换行形式（`src/OptionHandlerFactory.cc:216,332,339,380,454` 等）单独处理。
3. 3 处 `new DeprecatedOptionHandler(new BooleanOptionHandler(...))`：
   `DeprecatedOptionHandler` 的 `depOptHandler_` 改
   `std::unique_ptr<OptionHandler>`，构造参数改 `std::unique_ptr<OptionHandler>`；
   `repOptHandler_` 保持 `const OptionHandler*`（不接管所有权）并加注释。
   `src/OptionHandlerImpl.cc:667` 的析构体整体删除（`= default`）。
4. `src/OptionParser.h:56` → `std::vector<std::unique_ptr<OptionHandler>> handlers_;`
   `:77-79` → `void setOptionHandlers(std::vector<std::unique_ptr<OptionHandler>> handlers);`
   （按值移动）、`void addOptionHandler(std::unique_ptr<OptionHandler> handler);`
   `src/OptionParser.cc:65-68` 删 `std::for_each(..., Deleter())`。
   `addOptionHandler` 里 `handlers_[optId] = std::move(handler);`。
5. 读取侧必须显式 `.get()` 的实测清单（`std::unique_ptr` 不能隐式转成
   `const OptionHandler*`；`if (h && ...)` 因 `explicit operator bool` 仍可用）：
   - `src/OptionParser.cc:305`、`:316-317`、`:331` 的 `result.push_back(h)`
     → `result.push_back(h.get())`；
   - `src/OptionParser.cc:348 const OptionHandler* h = handlers_[id];`
     → `handlers_[id].get()`；
   - `src/OptionParser.cc:346`、`:350` 的 `return handlers_[0];`
     → `return handlers_[0].get();`；
   - `src/OptionParser.cc:293`、`:303`、`:315`、`:330` 的 `for (const auto& h : handlers_)`
     与 `countPublicOption`/`putOptions`/`createOptstring` 三个模板
     （`:72-155`，被 `:160,163,164` 以迭代器调用）里的 `*first &&` 与
     `(*first)->getName()` 无需改（`explicit operator bool` + `operator->`）。
6. `src/OptionParser.cc:369-372 getInstance()` 的 `setOptionHandlers(createOptionHandlers())` 不变。

### 枚举改造规则（P1~P3 通用）

- `enum` → `enum class`，常量去公共前缀（`REQ_ARG`→`ArgType::REQ_ARG` 保留原名更省改动：
  **保留原常量名，只加作用域**，除非原名与其它 enum 撞名）。
- 位掩码类（`Logger::LEVEL`、`HelpTag`）显式给底层类型：
  `enum class LEVEL : uint8_t { ... }`，运算点写 `static_cast<uint8_t>(...)`。
- **`uint32_t` 位下标接口保持**：`hasTag(uint32_t)` / `addTag(uint32_t)` /
  `findByTag(uint32_t)` / `strHelpTag(uint32_t)` / `idHelpTag` 不改签名
  （`tags_` 是位图，`1 << tag` 语义要求整数），调用点用
  `static_cast<uint32_t>(HelpTag::TAG_FILE)`。上一轮 `Command::Status` 已证明
  `enum class` 形参化会牵出 16 文件；这里只做「常量加作用域」，收益/成本比最好。
- `src/uri_split.h` 的 C 枚举**不动**（C 头 + 数组下标 + 位掩码）。
- `src/error_code.h` 的 `enum Value` **不动**（用户决策）。
- `src/uri.cc:212` 的 `NPATH_*` 状态机留 `shortcut:`（21 处 switch，纯文件内）。

### 标准库替换

- `std::find_if` → `std::ranges::find_if`（`src/HashFuncEntry.h:76`），`<algorithm>` 已够用。
- `std::stringstream` 7 处：
  - `src/json.cc:98 std::ostringstream out; return encode(out, json).str();` ——
    `encode(std::ostream&, const ValueBase*)` 的重载是公共格式化路径，`GZipEncoder`
    也走它 → 保留并加 `shortcut:`（要换得改 `src/json.h:56-106` 整族 `operator<<`）。
  - `src/RpcResponse.cc:101,148` 与 `GZipEncoder` 共用同一个
    `encodeJsonAll(OutputStream&, ...)` 模板 → 保留并加 `shortcut:`。
  - `src/ColorizedStream.cc:64,78` 两处 `std::stringstream rv;` 只做字符串拼接 →
    改 `std::string rv;` + `rv += e.second;`（`rv.write(...)` 改 `rv.append(...)`）。
  - `src/FeatureConfig.cc:176 `std::stringstream rv`（`usedCompilerAndPlatform`）
    与 `:217 std::stringstream ss`（`getOperatingSystemInfo`）→ 改 `std::string` + `+=`。
- `snprintf(prefix, sizeof(prefix), "A2-%d-%d-%d-", ...)` →
  `auto prefix = fmt("A2-{}-{}-{}-", major, minor, micro);`
  （`src/OptionHandlerFactory.cc:1797-1799`）；`sscanf` 保留并加 `shortcut:`。
- `.c_str()` 30 处：`fmt("...{}...", s.c_str())` → `fmt("...{}...", s)` 可删 20 处左右；
  保留 `strptime` / `getopt_long` / `uri.c_str()` 游标 / `rv.write(c_str(), n)`。
- `printf` 13 处全是 `OutputFile::printf` 接口，保留（与 PLAN-005 同口径）。

### span 化

cfg79 没有真正的网络/磁盘缓冲接口。实测 ptr+len 命中只有：
`src/uri_split.h` 的 `uri_split_parse(const char* buf, size_t len, ...)`（C 接口，
不改）、`src/OptionHandlerFactory` 的 `snprintf`/`sscanf`（C 库语义）。
**本计划不做 span 化**，在「实际偏差」里登记原因。

### 错误处理

cfg79 的错误处理已经比较统一（`OptionHandlerException` / `UnknownOptionException`
/ `DlAbortEx`），实测没有 `bool`+出参 或 `ssize_t& error` 形态。
唯一可改进项已做：`snprintf` → `fmt()`；`sscanf` 三段版本号解析保留
（`shortcut:` 注明）。
**不做**大范围错误处理重构（收益低、跨层风险高）。

## 分步计划

1. [x] **阶段 1 — 配置与选项（33 文件 / 5 682 行）**
   - `src/OptionHandlerFactory.cc`：196 处 `OptionHandler* op(new X(...))` →
     `auto op = std::make_unique<X>(...)`；197 处 `handlers.push_back(op)` →
     `handlers.push_back(std::move(op))`；`createOptionHandlers()` 返回类型改
     `std::vector<std::unique_ptr<OptionHandler>>`。
   - `src/OptionHandlerImpl.h/.cc`：`DeprecatedOptionHandler::depOptHandler_` →
     `std::unique_ptr<OptionHandler>`，析构 `= default`，3 处工厂调用改 `std::make_unique`。
   - `src/OptionParser.h/.cc`：`handlers_` → `std::vector<std::unique_ptr<OptionHandler>>`，
     `setOptionHandlers` 按值移动，`addOptionHandler` 收 `unique_ptr`，
     删 `std::for_each(handlers_.begin(), handlers_.end(), Deleter())`，4 处遍历改 `h && h->`。
   - `src/OptionHandler.h:87 ARG_TYPE` → `enum class ArgType`（99 处引用加 `ArgType::`）；
     `src/help_tags.h:42 HelpTag` → `enum class HelpTag`（314 处加 `HelpTag::`，
     `uint32_t` 位下标接口保持签名，调用点 `static_cast<uint32_t>(HelpTag::X)`）；
     `src/help_tags.cc` 与 `src/AbstractOptionHandler.cc` 同步。
   - `src/OptionHandlerFactory.cc:1796` 的 `snprintf(prefix, ...)` → `fmt()`；
     同一处的 `sscanf` 保留并加 `shortcut:` 注释。
   - `src/SessionSerializer.cc:146 typedef T type;` → `using type = T;`。
   - `.c_str()`：`src/OptionHandler.cc:72,75,77` 的 `printf` 保留；
     `src/OptionParser.cc:98,254,268`、`src/MultiUrlRequestInfo.cc:209,213,223,318,322`、
     `src/HashFuncEntry.h:79`、`src/UnknownOptionException.cc:46,55` 的
     `fmt(...c_str())` → `fmt(...)`；`src/OptionParser.cc:166 optstring.c_str()` 保留。
   - `src/wallclock.cc:43` 静态 `new Timer()` 保留 + `shortcut:`。
   - `src/version_usage.cc:140` 的 `strHelpTag(TAG_BASIC)` → `static_cast<uint32_t>(HelpTag::TAG_BASIC)`。
   - 回归网：`test/OptionTest.cc`、`test/OptionHandlerTest.cc`（`:191-197` 用
     `addTag(TAG_ADVANCED)` / `hasTag(TAG_BASIC)`）、`test/OptionParserTest.cc:102`
     （`findByTag(TAG_FILE)`）、`test/SessionSerializerTest.cc`、`test/UriListParserTest.cc`。
   - 门禁通过后提交 `PLAN-006 阶段1: 配置与选项现代化`。

   **实测（2026-11-08）**

   - `src/OptionHandlerFactory.cc/.h`：196 处 `OptionHandler* op(new X(...))` 全部改为
     `auto op = std::make_unique<X>(...)`（含 23 处跨行形态与 3 处嵌套
     `new DeprecatedOptionHandler(new X(...))`），197 处 `handlers.push_back(op)` 改
     `push_back(std::move(op))`；返回类型与局部变量改
     `std::vector<std::unique_ptr<OptionHandler>>`。11 处 `ParameterOptionHandler`
     的 `{...}` 实参显式包成 `std::vector<std::string>{...}`（`make_unique` 的模板
     实参无法从花括号初始化列表推导 vector）；`OptimizeConcurrentDownloadsOptionHandler`
     的多余第 4 实参（它没有 `argType` 形参，原来那个 `OPT_ARG` 从未生效）删掉。
   - `src/OptionHandlerImpl.h/.cc`：`depOptHandler_` → `std::unique_ptr<OptionHandler>`，
     析构 `= default`，`delete depOptHandler_` 与注释一并删除；`class DeprecatedOptionHandler`
     新增 `<memory>` include。
   - `src/OptionParser.h/.cc`：`handlers_` → `std::vector<std::unique_ptr<OptionHandler>>`；
     **`addOptionHandler` 整段删除**（`setOptionHandlers` 改为按值收 vector、内部直接
     移动进 `handlers_`，唯一调用方就是它自己），`setOptionHandlers` 形参按值；
     析构 `= default`，删 `std::for_each(..., Deleter())`；`findByTag` /
     `findByNameSubstring` / `findAll` 的 `push_back(h)` → `push_back(h.get())`；
     `findById` 的 `handlers_[0]` / `handlers_[id]` 加 `.get()`；
     `handlers_(option::countOption(), nullptr)` → `handlers_(option::countOption())`
     （`unique_ptr` 不可拷贝填充）。
   - `src/OptionHandler.h:88 ARG_TYPE` → `enum class ArgType { REQ_ARG, OPT_ARG, NO_ARG }`
     （99 处引用加 `ArgType::`，`OptionHandler::ArgType::` 全限定写全）。
   - `src/help_tags.h:42 HelpTag` → `enum class HelpTag : uint32_t`（314 处 `TAG_*` 加
     `HelpTag::`）；**同时把 `strHelpTag` / `idHelpTag` / `OptionHandler::hasTag` /
     `addTag` / `OptionParser::findByTag` 的形参从 `uint32_t` 换成 `HelpTag`** ——
     原计划说「位下标接口保持 `uint32_t` 签名」，实测调用点全是 `TAG_*` 常量
     （没有一个是运行时算出来的整数），换成枚举后 `static_cast<uint32_t>` 反而
     不需要；只有 `AbstractOptionHandler::toTagString()` 与
     `OptionHandlerFactory` 的 help 选项构造需要 `static_cast` 往返。
   - `src/SessionSerializer.cc:146 typedef T type;` → `using type = T;`。
   - `.c_str()`：`src/OptionHandlerImpl.cc:158,389,554,674,680,685`、
     `src/OptionParser.cc:95,251,265`、`src/MultiUrlRequestInfo.cc:209,213,223,318,322`
     的 `fmt(..., s.c_str())` → `fmt(..., s)`；保留 `src/OptionParser.cc:163` 的
     `optstring.c_str()`（`getopt_long` 收 `const char*`）、`src/SessionSerializer.cc:81,86`
     的 `tempFilename.c_str()`（`GZipFile`/`BufferedFile` 构造收 `const char*`）、
     `src/OptionHandler.cc:72,75,77` 的 `OutputFile::printf`。
   - `src/OptionHandlerFactory.cc:1799` `snprintf(prefix, sizeof(prefix), "A2-%d-%d-%d-", ...)`
     → `auto prefix = fmt("A2-{}-{}-{}-", major, minor, micro);`；`sscanf` 保留 +
     `shortcut:` 注释（build-time 常量、三段点号格式，逐段 `from_chars` 更长）。
   - `src/wallclock.cc:43` `static auto t = new Timer();` → `static Timer t;`
     （`Timer` 是 `std::chrono::steady_clock` 的薄包装、可平凡析构，全局单例无需堆分配）。
   - `src/version_usage.cc:140` / `src/option_processing.cc:204,260` 的
     `strHelpTag(TAG_*)` → `strHelpTag(HelpTag::TAG_*)`（不需要 `static_cast`）。
   - `test/OptionParserTest.cc` 的 `setUp()`：4 个 `OptionHandler* x(new ...)` +
     `oparser_->addOptionHandler(x)` → `auto x = std::make_unique<...>()` + 一个
     `std::vector<std::unique_ptr<OptionHandler>>` + 一次 `setOptionHandlers(std::move(...))`。
   - `test/OptionHandlerTest.cc:304,312` 的 `new DefaultOptionHandler(...)` →
     `std::make_unique<DefaultOptionHandler>(...)`。
   - 全部改动文件过 `clang-format`（仓库有 `.clang-format`，`ColumnLimit: 80`）。
   - 债务复核（P1 21 文件，剥注释）：`typedef 0`、`enum 0`、裸 `new 0`。

2. [x] **阶段 2 — RPC（20 文件 / 2 827 行）**
   - `src/RpcResponse.h:50 authorization_t` → `enum class authorization_t`
     （9 处加 `RpcResponse::`）；`src/RpcMethod.cc:93,96`、`src/RpcMethodImpl.cc:1367,1387`、
     `src/rpc_helper.cc:55`、`test/RpcResponseTest.cc:26,46` 同步。
   - `src/FeatureConfig.h:47 FeatureType` → `enum class FeatureType`；
     `strSupportedFeature` 形参保持 `int`（`for (first = 0; first < MAX_FEATURE; ++first)`
     的自增语义），内部 `switch` 的 `case (FEATURE_X):` → `case (FeatureType::FEATURE_X):`
     需先 `static_cast<FeatureType>(feature)`；`src/RpcMethodImpl.cc:1102` 与
     `test/FeatureConfigTest.cc:39,45` 的 `FEATURE_HTTPS` / `MAX_FEATURE` 加作用域。
   - `src/bignum.h:29,30` 两个 `typedef` → `using`（`USE_INTERNAL_BIGNUM` 未定义，
     本机不编译，靠人工核对）。
   - `src/RpcMethodImpl.h:336 typedef ... ItemListType;` → `using`。
   - `src/ColorizedStream`/`FeatureConfig` 的 `stringstream` **不在本阶段**
     （FeatureConfig 归 P3，ColorizedStream 归 P3）。
   - `src/json.cc:98`、`src/RpcResponse.cc:101,148` 的 `stringstream` 保留 + `shortcut:`。
   - `src/RpcMethodImpl.h:111`、`src/rpc_helper.cc:82` 的 `fmt(...c_str())` → `fmt(...)`。
   - `src/error_code.h` **不动**。
   - 回归网：`test/RpcMethodTest.cc`、`test/RpcResponseTest.cc`、`test/JsonTest.cc`、
     `test/ValueBaseJsonParserTest.cc`、`test/FeatureConfigTest.cc`、`test/Aria2ApiTest.cc`。
   - 门禁通过后提交 `PLAN-006 阶段2: RPC 层现代化`。

   **实测（2026-11-08）**

   - `src/RpcResponse.h:50`：`enum authorization_t { NOTAUTHORIZED, AUTHORIZED }` →
     `enum class Authorization { NOTAUTHORIZED, AUTHORIZED }`（去掉 `_t` 后缀，仓库里
     `_t` 只用于 C 兼容名）；成员 `authorized`、构造形参、`not_authorized()` 与
     `src/RpcMethod.cc:93,96`、`src/RpcMethodImpl.cc:1367,1387`、`src/rpc_helper.cc:55`、
     `src/RpcResponse.cc:50`、`test/RpcResponseTest.cc:26,46` 同步为
     `RpcResponse::Authorization::X`。
   - `src/FeatureConfig.h:47 FeatureType` → `enum class FeatureType`，常量去 `FEATURE_`
     前缀（`ASYNC_DNS` … `MAX_FEATURE`）；**`strSupportedFeature` 形参直接改成
     `FeatureType`**（原计划说保持 `int`，实测唯一需要整数的只有两处
     `for (int i = 0; i < MAX_FEATURE; ++i)` 计数循环，改成
     `static_cast<int>(FeatureType::MAX_FEATURE)` 边界 + `static_cast<FeatureType>(i)`
     更直白）；`switch` 的 `case (FeatureType::X):`、`src/RpcMethodImpl.cc:1102`、
     `test/FeatureConfigTest.cc:39,45` 同步。
   - `src/bignum.h:29,30` 两个 `typedef` → `using`（`USE_INTERNAL_BIGNUM` 未定义，
     本机不编译，人工核对：`char_t`/`uchar_t` 只在 `src/InternalDHKeyExchange.*` 用）。
   - `src/RpcMethodImpl.h:336` `typedef ... ItemListType;` → `using`。
   - `.c_str()`：`src/RpcMethodImpl.cc` 22 处 `fmt(..., x.c_str())` → `fmt(..., x)`
     （其中 12 处是 `GroupId::toHex(gid)` 临时 `std::string`，`fmt` 支持
     `std::string` 实参）；`src/RpcMethodImpl.h:111`、`src/rpc_helper.cc:82` 同步。
     保留 2 处：`src/RpcMethodImpl.cc:201` 的 `GroupId::expandUnique(n, str->s().c_str())`
     与 `:562` 的 `entryDict->put(KEY_GID, GroupId::toHex(...).c_str())` —— 前者收
     `const char*`，后者会命中 `put(std::string, String::ValueType)` 重载歧义。
   - `src/json.cc:98`、`src/RpcResponse.cc:101,148` 的 `std::ostringstream` /
     `std::stringstream` **保留**：`encodeJsonAll` / `encodeJsonBatchAll` 是
     `template <typename OutputStream>`，与 `HAVE_ZLIB` 下的 `GZipEncoder` 共用同一个
     `operator<<` 接口（`GZipEncoder` 没有 `str()` 之外的 string 能力），换成
     `std::string` 需要为两条分支写两套实现。已在三处加 `shortcut:` 注释。
   - `src/error_code.h` 未动（用户决策）。
   - `make check` 914 例全绿（本机增量构建），干净 worktree 门禁见提交记录。

3. [x] **阶段 3 — CLI 与输出（26 文件 / 4 044 行）**
   - `src/ColorizedStream.h:80,82,83,84`：`typedef std::char_traits<char> traits_t;` →
     `using`；`enum part_t { eColor, eString }` → `enum class Part { Color, String }`
     （6 处）；`elem_t` / `elems_t` → `using`；`src/ColorizedStream.cc:64,78` 的
     `stringstream` → `std::string` + `+=` / `append`，`:66,80` 的 `eColor`/`eString`
     改 `Part::Color`/`Part::String`，`:88 rv.write(e.second.c_str(), max)` 改
     `rv.append(e.second, 0, max)`。
   - `src/Logger.h:50 LEVEL` → `enum class LEVEL : uint8_t`（37 处，含
     `src/LogFactory.h:117,134` 的 `A2_LOG_DEBUG` 等宏体里的 `Logger::A2_DEBUG`）；
     `src/Logger.cc:89,91,96` 的 `level >= logLevel_` 保留（enum class 支持关系比较）。
   - `src/FeatureConfig.cc:176,217` 两处 `stringstream` → `std::string` + `+=`；
     `src/FeatureConfig.h:47` 的枚举已在 P2 改，本阶段只做 stringstream。
   - `src/console.h:46 typedef std::shared_ptr<OutputFile> Console;` → `using`。
   - `src/HashFuncEntry.h:49 typedef HashFunc HashFuncType;` → `using`；
     `:76 std::find_if` → `std::ranges::find_if`；`:79` 的 `fmt(...c_str())` → `fmt(...)`。
   - `src/uri_split.h:45,59,63` 三个 `typedef` **不动**（C 头，被 `src/uri_split.c` 与
     `src/uri.cc` 用）；`src/uri.cc:212 NPATH_*` 保留 + `shortcut:`；
     `src/uri.cc:92 const char* p = uri.c_str();` 保留。
   - `src/TimeA2.cc:81-82 strptime(datetime.c_str(), format.c_str(), &tm)` 保留。
   - `src/main.cc:72-73`、`src/Platform.cc:108`、`src/Logger.cc:136-204`、
     `src/MultiUrlRequestInfo.cc:155` 的 `printf` 全是 `OutputFile::printf`，保留。
   - 回归网：`test/TimeTest.cc`、`test/UriTest.cc`、`test/UriSplitTest.cc`、
     `test/FeatureConfigTest.cc`、`test/OptionHandlerTest.cc`。
   - 门禁通过后提交 `PLAN-006 阶段3: CLI 与输出层现代化`。

   **实测（2026-11-08）**：实际改动 10 文件（`src/ColorizedStream.{h,cc}`、
   `src/Logger.{h,cc}`、`src/LogFactory.{h,cc}`、`src/FeatureConfig.cc`、
   `src/console.h`、`src/HashFuncEntry.h`、`src/uri.cc`）。
   - `Logger::LEVEL` 改 `enum class`（**未加 `: uint8_t`**：实际引用只有 4 个文件
     的 37 处，且 `level >= logLevel_` 的关系比较在 `enum class` 下照常成立，
     加底层类型没有收益）；`src/LogFactory.h:117,134-147` 的 10 个 `A2_LOG_*` 宏体
     同步改 `Logger::LEVEL::A2_*`。
   - `src/ColorizedStream` 两个 `str()` 重载去掉 `std::stringstream`，
     `rv.write(e.second.c_str(), max)` → `rv.append(e.second, 0, max)`；
     include `<sstream>` → `<deque> <ostream> <string>`。
   - `src/FeatureConfig.cc` 的 `usedCompilerAndPlatform()` 与
     `getOperatingSystemInfo()` 改 `std::string` + `+=`（`strcmp`/`strstr` 仍走
     C 语义，保留）；`#include <sstream>` 删除。
   - `src/HashFuncEntry.h` 的 `std::find_if` → `std::ranges::find_if`，
     `fmt(..., hashType.c_str())` → `fmt(..., hashType)`。
   - `src/uri.cc:212` 的 `NPATH_*` 保持裸 `enum`（21 处两重嵌套 `switch`，加
     `shortcut:` 注释说明）；`src/uri_split.h:45,59,63` 三个 `typedef` 未动（C 头）。
   - `src/Logger.cc`、`src/main.cc`、`src/Platform.cc`、`src/MultiUrlRequestInfo.cc`
     的 `OutputFile::printf` 全部保留（输出接口契约）。
   - 干净 worktree `/tmp/aria2-m6-p3`（base `61201d68`）+ `autoreconf -i` +
     `./configure --enable-werror`（`CXX = g++ -std=c++20`、
     `WARNCXXFLAGS = -Wall -Werror -Wformat-security`）+ `make -j24 check`
     → `# TOTAL: 1` / `# PASS: 1` / `FAIL 0` / `ERROR 0`、`test/aria2c.log`
     `OK (914)`、`grep -c 'error:'` = 0、`git hash-object src/includes/aria2/aria2.h`
     = `2216641161b15acb3c3ddf2600dc19e3decd59d5`。

4. [ ] **收尾**：勾选全部阶段、加状态行、补「实测门禁结果」与「实际偏差」，
   记录原始债务表与结束债务表对比；提交
   `PLAN-006: 勾选阶段3、标记计划完成并记录实测门禁与偏差`。

## 预计修改的文件

**P1（33 文件全量，改动集中在 7 个）**：`src/OptionHandlerFactory.cc`（1655 行改动）、
`src/OptionHandlerFactory.h`、`src/OptionParser.h`、`src/OptionParser.cc`、
`src/OptionHandler.h`、`src/OptionHandlerImpl.h`、`src/OptionHandlerImpl.cc`、
`src/help_tags.h`、`src/help_tags.cc`、`src/AbstractOptionHandler.h`、
`src/AbstractOptionHandler.cc`、`src/SessionSerializer.cc`、`src/version_usage.cc`、
`src/OptionHandler.cc`、`src/MultiUrlRequestInfo.cc`、`src/UnknownOptionException.cc`、
`src/option_processing.cc`（`TAG_*` 2 处）、`src/wallclock.cc`（仅注释）。

**P2（6 个改动）**：`src/RpcResponse.h`、`src/RpcResponse.cc`、`src/RpcMethod.cc`、
`src/RpcMethodImpl.cc`、`src/RpcMethodImpl.h`、`src/rpc_helper.cc`、
`src/FeatureConfig.h`、`src/FeatureConfig.cc`（`switch` 的 `case` 加作用域）、
`src/bignum.h`、`src/json.cc`（仅注释）。

**P3（7 个改动）**：`src/ColorizedStream.h`、`src/ColorizedStream.cc`、
`src/Logger.h`、`src/LogFactory.h`、`src/FeatureConfig.cc`、`src/console.h`、
`src/HashFuncEntry.h`、`src/uri.cc`（仅注释）。

**测试同步**：`test/OptionHandlerTest.cc`、`test/OptionParserTest.cc`、
`test/RpcResponseTest.cc`、`test/FeatureConfigTest.cc`、`test/JsonTest.cc`。

**不动**：`src/includes/aria2/aria2.h`、`src/error_code.h`、`src/uri_split.h`、
`src/uri_split.c`、其余 6 个 `src/*.c`、`configure.ac`、`src/Makefile.am`、
`test/Makefile.am`、`.github/workflows/build.yml`、`README.rst`。

## 测试与验证

每阶段在干净 worktree 上跑同一套门禁：

```sh
cd /home/code/tmp/aria2
git worktree prune && rm -rf /tmp/aria2-m6-p<N>
git worktree add --detach /tmp/aria2-m6-p<N> HEAD
git diff > /tmp/p6-stage<N>.patch
cd /tmp/aria2-m6-p<N> && git apply /tmp/p6-stage<N>.patch
autoreconf -i
./configure --enable-werror
grep -E '^CXX =|^WARNCXXFLAGS' Makefile      # 期望 g++ -std=c++20 / -Wall -Werror -Wformat-security
make -j"$(nproc)" check
grep -E '^(# TOTAL|# PASS|# FAIL|# ERROR)|Failures !!!' test/test-suite.log
tail -3 test/aria2c.log                       # 期望 OK (914)
git hash-object src/includes/aria2/aria2.h    # 期望 2216641161b15acb3c3ddf2600dc19e3decd59d5
```

通过标准：`make check` rc=0、`# TOTAL: 1` / `# PASS: 1` / `# FAIL: 0` /
`# ERROR: 0`、`test/aria2c.log` 末行 `OK (914)`、无 `Failures !!!`、
`grep -c 'error:'` 为 0、公开 ABI 哈希不变。

阶段专项验证：

- P1：`git grep -nE 'OptionHandler\* op\(new|new .*OptionHandler' -- src/OptionHandlerFactory.cc`
  0 命中；`git grep -nE 'std::vector<OptionHandler\*>' -- src` 0 命中；
  `git grep -nE '\bsnprintf\s*\(' -- src/OptionHandlerFactory.cc` 0 命中
  （`sscanf` 保留，见「可解释残余」）；
  `git grep -nE '^\s*enum\s+\w*\s*\{' -- src/OptionHandler.h src/help_tags.h` 0 命中。
- P2：`git grep -nE '^\s*enum\s+\w*\s*\{' -- src/RpcResponse.h src/FeatureConfig.h` 0 命中；
  `git grep -nE '\btypedef\b' -- src/bignum.h src/RpcMethodImpl.h` 0 命中。
- P3：`git grep -nE '^\s*enum\s+\w*\s*\{' -- src/ColorizedStream.h src/Logger.h` 0 命中；
  `git grep -nE '\bstd::(i|o)?stringstream\b' -- src/ColorizedStream.cc src/FeatureConfig.cc`
  0 命中；`git grep -nE '\btypedef\b' -- src/console.h src/HashFuncEntry.h src/ColorizedStream.h` 0 命中。

**假绿陷阱**（前五轮实测）：
- 原地增量 `make` 不重编所有 `.lo`，`-Werror` 的新失败会被掩盖 → 必须干净 worktree。
- 只跑 `make` 不跑 `make check` 时测试二进制未重编，`aria2c.log` 可能是旧的
  `OK (898)` → 必须检查 `# TOTAL: 1` 与日志末行。
- `git worktree add` 前必须 `git worktree prune`，否则报
  `fatal: '/tmp/aria2-m6-pN' is a missing but already registered worktree`。
- 未启用编译路径（`USE_INTERNAL_BIGNUM`、`HAVE_LIBUV`、`ENABLE_LIBARIA2`、
  `--disable-bittorrent`）编译器兜不住 → 人工 diff 核对。

## 验收标准

1. `src/OptionHandlerFactory.cc` 的 200 处裸 `new` 全部消失，
   `createOptionHandlers()` 返回 `std::vector<std::unique_ptr<OptionHandler>>`，
   `OptionParser::handlers_` 同步，`Deleter()` 调用点删除。
2. cfg79 的 `typedef` 12 处中 9 处清零（`src/ColorizedStream.h` 3、`src/bignum.h` 2、
   `src/HashFuncEntry.h` 1、`src/RpcMethodImpl.h` 1、`src/SessionSerializer.cc` 1、
   `src/console.h` 1）；`src/uri_split.h` 的 3 个 C typedef 按设计保留并在偏差登记。
3. cfg79 的非 scoped `enum` 8 处中 7 处改 `enum class`（`ARG_TYPE`、`HelpTag`、
   `authorization_t`、`FeatureType`、`LEVEL`、`part_t`，以及 `src/uri_split.h` 的
   `uri_split_field`/`uri_split_flag` 中**只登记不改**的 2 个 C 枚举 —— 实际改造
   6 处 + `src/uri.cc` 的 `NPATH_*` 保留）；`src/error_code.h` 的 `Value` 与
   `src/uri_split.h` 的 C 枚举、`src/uri.cc` 的 `NPATH_*` 在偏差登记。
4. `std::stringstream` 7 → 0 或带 `shortcut:` 注释（保留项须写明限制与升级时机）。
5. 每阶段干净 worktree 门禁全绿，公开 ABI 哈希不变。
6. 3 次代码提交 + 1 次文档收尾提交，提交信息 `PLAN-006 阶段N: ...`，无 `Co-authored-by`。

## 风险与权衡

- **工厂 `unique_ptr` 全链**是本计划最大风险面：`createOptionHandlers()` 的 196 处
  构造 + 197 处 `push_back` 机械改写，容易漏 `std::move`（`unique_ptr` 拷贝被删，
  编译器会报错，`-Werror` 兜得住）或漏某处 `op->` 使用。
  `handlers_` 的空槽语义（`findById` 回退 `handlers_[0]`）必须保留：`nullptr`
  在 `unique_ptr` 里仍是有效状态。
- **`HelpTag` 314 处 `TAG_*`** 机械加作用域，其中 285 处在同一文件；
  建议用正则 `\bTAG_([A-Z_]+)\b` → `HelpTag::TAG_\1` 后统一校对，注意
  `#define` 与注释里可能误命中。
  位下标接口保持 `uint32_t`，调用点加 `static_cast<uint32_t>(HelpTag::X)`。
- **`FeatureType` 是 `int` 形参的公开函数**（`strSupportedFeature(int)`），
  `enum class` 化后 `switch (feature)` 的 `case` 必须先转换；
  `MAX_FEATURE` 作为哨兵与 `for (int i = 0; i < MAX_FEATURE; ++i)` 共存，
  保持 `int` 循环是刻意选择，加注释说明。
- **`Logger::LEVEL` 是位掩码**（`1 << n`）+ `fileLogEnabled` 用 `>=`；
  `enum class` 后 `Logger::A2_DEBUG` → `Logger::LEVEL::A2_DEBUG`，
  `A2_LOG_DEBUG` 等 6 个宏在 `src/LogFactory.h` 展开，宏体改了会波及所有
  `A2_LOG_*` 调用点（但只是文本展开，不需要改调用方）。
- **`src/uri_split.h` 与 `src/error_code.h` 不改**：前者是 C ABI（与 `uri_split.c`
  共用，`uri.cc` 用数组下标与位掩码），后者 227 处跨层引用且纯编号语义。
- **`bignum.h` 本机不编译**：`USE_INTERNAL_BIGNUM` 未定义，改动只能靠人工核对；
  它的两个 `typedef` 在模板类内，`using` 是纯语法替换，风险低。
- **`json.cc` / `RpcResponse.cc` 的 `stringstream` 保留**：它们与 `GZipEncoder`
  共用 `encodeJsonAll(OutputStream&, ...)` 模板，换 `std::string` 要重写
  `src/json.h:56-106` 的整族 `operator<<`，收益为零。

## 自查（写完后对照代码逐条核验）

1. **引用真实**：所有文件名/行号/函数名已用 `grep -n` 实测（`cfg_now.txt` 79 文件、
   `OptionHandlerFactory.cc:1796` 的 `sscanf`、`OptionHandler.cc:72,75,77` 的
   `printf`、`help_tags.cc:50-70`、`Logger.h:50`、`uri.cc:212`、`RpcResponse.h:50`、
   `FeatureConfig.h:47`、`ColorizedStream.h:80-84`、`console.h:46`、`HashFuncEntry.h:49,76,79`、
   `SessionSerializer.cc:146`、`bignum.h:29,30`、`RpcMethodImpl.h:336`、
   `OptionParser.cc:65-68,273-290,345-350,369`、`OptionHandlerImpl.h:275-276`、
   `a2functional.h:57-60`）。
2. **阶段可编译、测试不假绿**：每个枚举的引用计数已 `grep -rnoE` 统计；
   每个阶段的门禁是干净 worktree 全量重编译；`test/*.cc` 名字全部用
   `git ls-files 'test/*.cc'` 核实（`test/TimeTest.cc`、`test/UriSplitTest.cc` 存在，
   `test/TimeA2Test.cc`、`test/HelpTagsTest.cc`、`test/ColorizedStreamTest.cc`、
   `test/LogFactoryTest.cc`、`test/MultiUrlRequestInfoTest.cc`、
   `test/PlatformTest.cc`、`test/UnknownOptionExceptionTest.cc`、
   `test/OptionHandlerFactoryTest.cc` 不存在）。
3. **覆盖所有分支与消费点**：`createOptionHandlers()` 的生产者 1 处、消费者
   `OptionParser::setOptionHandlers` 1 处、`handlers_` 读者 6 处（`findById` /
   `find` / `findByTag` / `findAll` / `countPublicOption` 遍历）；
   `OptionHandler*` 在 cfg79 内 30+ 处已列，cfg79 外只有
   `src/version_usage.cc` 与 `src/option_processing.cc` 的 `const OptionHandler*`
   读取（不受容器类型影响）。

## 备注

- 用户授权：一次性回答 4 个决策问题，全部采纳推荐项；范围 cfg79、
  工厂 `unique_ptr` 全链、除 `error_code::Value` 外全改 `enum class`、3 阶段。
- 与 PLAN-005 的口径差异：PLAN-005 用「簇」而非「层」，本计划按职责分三层
  （配置/RPC/CLI），因为 cfg79 天然按这三块聚集且互相独立。
- `src/fmt.h` 已是 `std::format` 包装（PLAN-003 阶段 1 `132bb4ad`），
  本计划**不做** fmt 迁移，只删多余的 `.c_str()`。
