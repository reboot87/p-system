# 工具箱化与 DLL 可插拔插件 — 版本规划

> 状态：**待评审**（本轮只做规划，不改代码）
> 日期：2026-11-09　基线：`47b33b3`　现有 tag：`phase0-baseline`（建议补打 `v0.8` 作为单体态基线）
> 修订：**v2 — 加载上下文与加载时机改由插件自己声明**（前台 / 后台可单独声明，也可两者都要）
> 关联：`docs/ROADMAP.md`（历史阶段记录）、`docs/skia-backend.md`（DLL 插件范本）、`docs/design-patterns.md`（Command + Registry 约定）

---

## 0. 结论先行

项目里已经有**两套互不相干的「插件」**：

| | `src/plugins/` | `airui-skia.dll` |
|---|---|---|
| 形态 | 静态编译进 `airecord.exe` / `airecordd.exe` 的 C++ 模块（`IFeaturePlugin`） | 运行时 `LoadLibraryW` 加载的独立 DLL |
| 边界 | 直接拿 `App&`，能摸 `HWND`、控件 id、定时器 id | 纯 C 函数表 + opaque handle，零 STL 跨界 |
| 可插拔 | 否，加一个功能要重编主程序 | 是，换 DLL 文件即可 |

**工具箱 = 把第二套的宿主骨架推广到业务工具，把第一套的生命周期语义搬进去。**
不需要新发明加载框架，`PainterRegistry` / `PaintProviders.cpp` / `SkiaPluginAbi.h` / CMake SHARED target / `test_skia_plugin.cpp` 五件现成资产可直接照抄结构。

真正的工程量不在「加载 DLL」，而在三处解耦：`App&`、硬编码的页面与导航、散落在各处的宿主能力（IPC / Toast / 配置 / 定时器 / 刷新）。

**v2 修订要点**：前台 / 后台不是宿主给插件分的类，而是**插件自己在 manifest 里声明的两个正交维度** ——
① 加载上下文（`ui` / `service` / 两者都要），② 加载时机（`eager` / `lazy` / `background`）。
两个宿主扫描同一份 `plugins/` 目录，各自只加载声明包含自己上下文的插件；同一个 DLL 可以同时被两个进程加载成两个互不共享内存的实例，靠宿主代转的 peer channel 通信。

---

## 1. 目标形态与非目标

### 1.1 目标产品形态

```
airecord.exe（前台宿主：外壳 + 导航 + 设置 + 插件管理 + 自绘控件 + 绘制后端）
  └─ 扫描 plugins/*/*.dll → 只加载声明含 "ui" 的插件
       ├─ tool-clipboard/   contexts: [ui]                 纯前台
       ├─ tool-filewatch/   contexts: [ui, service]        前台页面 + 后台常驻
       └─ tool-printer/     contexts: [service]            纯后台（管理页仅显示状态）

airecordd.exe（后台宿主：采集 / 入库 / IPC 服务）
  └─ 扫描同一份 plugins/ → 只加载声明含 "service" 的插件（常驻）
```

「可插拔」的最低验收语义：**增删 DLL 文件后重启宿主即可生效，主程序零重编、零链接**；管理页能看到每个插件的版本、ABI、位数、**声明的上下文与加载时机**、前后台两个实例各自的运行状态与失败原因。v1.1 起支持按上下文分别启用/禁用/重载。

### 1.2 两个正交维度，全部由插件声明（关键架构决策）

| 维度 | 取值 | 决定什么 | 谁决定 |
|---|---|---|---|
| **① 加载上下文 `contexts[]`** | `ui` / `service` / 两者 | 加载到哪个进程、是否常驻、能拿到哪些 HostApi 能力 | **插件声明**，宿主只做过滤与能力提供 |
| **② 加载时机 `load.mode`** | `eager` / `lazy` / `background` | 什么时候加载、在哪个线程加载、是否阻塞首屏 | **插件声明**，宿主可因策略覆盖但必须记录并显示实际值 |

**维度 ① 的三种形态**

| | 纯前台 `["ui"]` | 纯后台 `["service"]` | 双上下文 `["ui","service"]` |
|---|---|---|---|
| 典型例子 | 剪贴板查看、格式转换、批量重命名 | 只上报不展示的采集器、定时清理 | **文件监控 / 打印监控**：要常驻采集，也要有页面和开关 |
| 前台进程 | 加载，有导航项 / 页面 / 卡片 | **不加载**（管理页经 IPC 显示状态） | 加载，提供页面与开关 |
| 后台进程 | **不加载**，零开销 | 加载，随服务常驻 | 加载，随服务常驻；关主窗口不停 |
| 能力表 | `ipc_request / toast / timer / config / invalidate / notify_refresh / log` | `db_* / notify_refresh / log / peer` | 各自拿到对应上下文的子集 |
| 生命周期 | 随窗口显示/隐藏，可懒加载 | 随服务常驻，主程序关闭不影响 | 两边独立，靠 peer channel 联动 |

依据：数据库 `g_db` 和剪贴板监听都在服务端进程（`src/service/main.cpp:83-87,145-167,360-365`），客户端所有数据操作走命名管道。**需要后台常驻的插件若只做在 UI 侧，关掉主程序就停止记录，直接破坏现有 C/S 架构**；反过来，纯前台工具不该被硬塞进服务进程白白占内存。所以既不能由宿主一刀切分类，也不该拆成两套插件包 —— 让插件自己声明最合适。

**维度 ② 的三种时机**

- `eager`：宿主启动即加载。服务侧采集**必须**如此，否则启动到激活之间会漏记录。
- `lazy`：首次进入该工具 / 首次触发命令才加载。纯前台小工具默认推荐，省启动开销。
- `background`：启动后在**后台线程**预加载，不阻塞首屏。适合体积大、初始化慢但很快会被点开的工具。

宿主可覆盖：省电模式把 `background` 降为 `lazy`、调试开关强制 `eager`、管理员禁用某上下文。覆盖必须在 `client.log [tool]` 与管理页写明「声明值 / 实际值」，**不能静默改**。

### 1.3 单 DLL 双实例（不是两套包）

一个插件包 = 一个 DLL + 一个 manifest，声明 `contexts: ["ui","service"]` 时：

- 两个进程各自 `LoadLibraryW` 同一份 DLL，得到**两个独立实例、各自独立的 `create/destroy` handle，不共享内存、不共享全局状态**。
- `create(host, context)` / `destroy(handle, context)` 都带上下文参数，DLL 内部按 `context` 初始化不同子系统；服务侧实例不得触碰任何 UI 依赖（见 §3.4 线程与依赖约束）。
- 两个实例通过宿主的 `notify_peer()` 通信，**插件不自己开命名管道**（见 §3.4）。
- 好处：一个版本号、一份配置、一次安装；坏处（DLL 会同时含两段代码）对纯 C ABI 而言可忽略，且服务侧只跑 `service` 分支。

### 1.4 非目标（明确不做）

- 不做进程级沙箱 / 独立插件进程（v2.0 再评估）；同进程插件崩溃会带崩宿主，v1.x 用自动禁用兜底。
- 不让插件直接拿 `HWND`、不跨 DLL 传 `std::wstring` / STL 容器 / C++ 虚表对象。
- 不改 `SkiaPluginAbi.h`，绘制后端插件与工具插件是两条独立 ABI、独立版本号。
- **不把「前台/后台」做成宿主硬编码分类，也不要求插件拆成 ui.dll / service.dll 两个包**。
- 不提前做业务模块：文件监控 / 打印监控仍遵守用户排期「业务模块最后做」，排在 v1.2。
- 不为插件化重写 TextBox / ListBox，也不碰 core 层平台无关门禁。

---

## 2. 现状盘点

### 2.1 可直接复用

| 资产 | 位置 | 复用方式 |
|---|---|---|
| `PainterRegistry` | `src/airui/core/include/airui/PaintBackend.h:58-90`、`core/src/PaintBackend.cpp:15-102` | `ToolRegistry` 照抄：按 id 注册、同 id 替换、`Available()` 探测、`LastError()` 记录降级原因；扩展到 `(id, context)` 复合键 |
| DLL 加载器 | `src/airui/platform/win32/PaintProviders.cpp:30-82,197-205` | `ToolLoader` 照抄：进程级 `PluginApi` 缓存、`Resolve<T>`、环境变量覆盖路径、必需/可选符号分级、`abi()` 严格相等、`Reload()` 故意不 `FreeLibrary` |
| 稳定 C ABI 头 | `src/airui/platform/skia/airui/SkiaPluginAbi.h` | 新头照抄形态：只 include `<stddef.h>/<stdint.h>`、`extern "C"`、`#ifdef _WIN32` 包调用约定、opaque handle 在 DLL 内生死 |
| 生命周期与事件泵 | `src/core/PluginManager.cpp:3-44` | 注册顺序分发、逆序卸载、`AttachAll` 失败即启动失败 —— 但接口要从 `App&` 换成 `HostApi* + handle + context` |
| 跨进程通知 | `src/ipc/Ipc.h:30-31`、`src/service/main.cpp::NotifyClientRefresh()`、`App.cpp:114-135` 的 `WM_COPYDATA` | peer channel 直接复用现有管道 / `WM_COPYDATA`，不必新建通信机制 |
| CMake SHARED + 部署 | `CMakeLists.txt:191-198`、`scripts/deploy-skia-plugin.cmake` | `OUTPUT_NAME` / `ARCHIVE_OUTPUT_NAME` 防撞库、产物与 exe 同目录、POST_BUILD 只拷 DLL |
| 契约测试模板 | `tests/test_skia_plugin.cpp:41-107` | 五段式：注册/可用性 → 跨模块调用 → 降级 → 裸 `LoadLibrary`+`GetProcAddress`+ABI 断言 → 能力探测；再加「双上下文」段 |
| 绘制金样基座 | core `PaintRecorder`（`IPainter` → `PaintOp`） | v1.2 自定义绘制直接复用，跨后端可比对 |

### 2.2 必须解耦

| 障碍 | 位置 | 处理 |
|---|---|---|
| `App&` 泄漏 | `src/core/Plugin.h:11-41`（全部接口）、`ManualEntryPlugin.cpp:29-64`、`SearchPlugin.cpp:5-29` | 换成纯 C `AiruiHostApi*` 能力表 + `void* host_ctx` + `AiruiContext`；插件不 include `App.h` / `windows.h` |
| 硬编码页面/导航 | `src/core/App.cpp:201-210`（`AddPage(0/1/2)`）、`:213-243`（手写 `NavGroup`）、`:246-270`（`switch` 命令 id） | 改清单/注册表驱动；`Shell.cpp:389-407` 已有自动生成路径（当前被 `SetNavGroups` 短路），可作为落点 |
| 静态链接来源 | `CMakeLists.txt:213-215` `GLOB_RECURSE src/plugins/*.cpp` 并入 `COMMON_SOURCES`，同时进两个 exe | 拆成独立 SHARED target；内置工具改为静态注册但走同一 `ToolRegistry` |
| 控件/定时器 id 硬编码 | `src/core/App.h:22-38` | 插件不感知 id，用 `command key`（字符串）订阅，宿主负责映射 |
| 命令只从 `ClipsPage` 转发 | `src/pages/ClipsPage.cpp:941-947` → `DispatchCommand` | 事件泵收口到宿主消息钩子；**两个宿主各有一个事件泵**，按 context 分发 |
| 宿主能力形态散乱 | App 成员 / Shell 成员 / 服务端全局 / 自由函数 / 裸 Win32 / 另一进程 | 收敛为单一 `AiruiHostApi` 表，并在表中标明每项能力的**进程归属**；服务侧表是 UI 侧表的子集/变体 |
| 服务进程只认自己写死的监听 | `src/service/main.cpp:360-365` | 加 `CollectorPluginHost`：扫描同一 `plugins/`，按 `contexts` 过滤，服务侧实例随服务启停 |

### 2.3 门禁约束

`scripts/check-core-clean.{sh,ps1}` 只扫 `src/airui/core/`。**新 ABI 头放 `src/pluginapi/`（core 之外）即可不触发平台标识符禁令**；但仍须满足：

- 只 include `<stddef.h> / <stdint.h>`；`__cdecl` / `__declspec` 必须 `#ifdef _WIN32` 包裹，非 Win32 分支留空。
- 能被 `g++ -std=c++17 -Wall -Wextra -Werror` 编译（`check-core-linux-smoke.sh` 同款严度），新增 `check-tool-abi.sh` 做无 SDK 语法门禁。
- 函数形状变更必须 bump `AIRUI_TOOL_ABI`，宿主严格相等校验（照抄 `LoadPlugin():79`）；结构体带 `size` 字段做前向兼容，可选能力用 capability bits，避免每加一个函数就 bump 大版本。

---

## 3. 架构设计

### 3.1 分层

```
插件 DLL（一个包，可被两个进程各加载一次）
  只依赖 airui_tool_abi.h（纯 C / UTF-8 / 零平台头）
      ⇅ 导出符号 + HostApi 函数表（唯一边界，带 AiruiContext）
┌─────────────────────────┬─────────────────────────┐
│ airecord.exe 前台宿主     │ airecordd.exe 后台宿主    │
│ ToolLoader(ctx=ui)       │ ToolLoader(ctx=service)  │
│ ToolRegistry / PageAdapter│ CollectorHost / DB 能力  │
│ 扫描 → 只取含 ui 的声明    │ 扫描 → 只取含 service 的 │
└─────────────────────────┴─────────────────────────┘
      ⇅ notify_peer()：宿主代转的跨上下文通道（复用现有 IPC）
```

### 3.2 ABI 契约（v1 形状，示意）

```c
#define AIRUI_TOOL_ABI 1
#define AIRUI_TOOL_FN_ABI      "airui_tool_abi"       /* uint32_t (*)(void) */
#define AIRUI_TOOL_FN_MANIFEST "airui_tool_manifest"  /* const char* (*)(void), UTF-8 JSON, 静态存储 */
#define AIRUI_TOOL_FN_CREATE   "airui_tool_create"    /* void* (*)(const AiruiHostApi*, AiruiContext) */
#define AIRUI_TOOL_FN_DESTROY  "airui_tool_destroy"   /* void (*)(void*, AiruiContext) */
#define AIRUI_TOOL_FN_EVENT    "airui_tool_on_event"  /* int (*)(void*, AiruiContext, const AiruiToolEvent*) */

typedef enum { AIRUI_CTX_UI = 1, AIRUI_CTX_SERVICE = 2 } AiruiContext;
typedef enum {
    AIRUI_LOAD_EAGER      = 0,  /* 宿主启动即加载；服务侧采集必须 */
    AIRUI_LOAD_LAZY       = 1,  /* 首次进入工具 / 首次触发命令才加载 */
    AIRUI_LOAD_BACKGROUND = 2   /* 启动后后台线程预加载，不阻塞首屏 */
} AiruiLoadMode;

typedef struct AiruiHostApi {
    uint32_t      abi;
    uint32_t      size;          /* 插件按 size 判断字段可用性 */
    uint64_t      caps;          /* 能力位：两个上下文各给不同子集 */
    AiruiContext  context;       /* 本实例运行在哪个上下文 */
    void*         ctx;
    void  (*log)(void*, int level, const char* utf8);
    void* (*alloc)(void*, size_t);
    void  (*free)(void*, void*);          /* 谁分配谁释放 */
    int   (*ipc_request)(void*, const char* req, char** out_utf8);
    void  (*toast)(void*, const char* title, const char* body, int kind);
    int   (*config_get)(void*, const char* key, char** out_utf8);
    int   (*config_set)(void*, const char* key, const char* value);
    uint32_t (*set_timer)(void*, uint32_t ms, void (*cb)(void*, uint32_t), void* user);
    void  (*kill_timer)(void*, uint32_t id);
    void  (*invalidate)(void*);
    void  (*notify_refresh)(void*);
    int   (*notify_peer)(void*, const char* key, const char* payload_utf8); /* 跨上下文 */
    int   (*peer_state)(void*, AiruiContext peer, int* running);            /* 对端实例是否活着 */
    /* 服务上下文追加：db_insert / db_query / setting_get ... */
    /* UI 上下文追加：paint_ops / hit_rect ...（v1.2） */
} AiruiHostApi;
```

**字符串一律 UTF-8 `const char*`**（不传 `wchar_t` / `std::wstring`）；返回字符串的缓冲区由宿主 `alloc`、插件用 `host->free` 释放；`manifest` 返回 DLL 内静态存储，宿主只读不释放；对象 `create/destroy` 全在 DLL 内且带 `context`。

事件为 POD `AiruiToolEvent { uint32_t type; AiruiContext from; const char* key; int64_t a, b; const char* payload_utf8; }`，覆盖
`SHOW / HIDE / COMMAND / TIMER / REFRESH / SETTING_CHANGED / QUERY_ROWS / CONTEXT_READY / PEER_MESSAGE / SUSPEND / RESUME`，不传窗口消息结构。

### 3.3 插件清单与发现（两宿主共用一套规则）

manifest 由 `airui_tool_manifest()` 返回，示例（**前台页面 + 后台常驻**）：

```json
{
  "schema": 1, "id": "tool.filewatch", "name": "文件监控",
  "version": "1.0.0", "abi": 1, "min_host": "1.0.0",
  "contexts": [
    { "name": "ui",      "requires": ["ipc", "toast", "timer", "config"],
      "load": { "mode": "lazy", "ui_thread": false } },
    { "name": "service", "requires": ["db", "notify_refresh", "log", "peer"],
      "load": { "mode": "eager", "persistent": true } }
  ],
  "nav": { "group": "功能模块", "label": "文件监控", "order": 20 },
  "ui": { "kind": "list" },
  "commands": ["toggle_watch", "clear"],
  "settings": [{ "key": "watch_dirs", "type": "string" }]
}
```

- **每个宿主都扫描同一份 `plugins/*/*.dll`**，先用 sidecar `manifest.json`（可选，不加载 DLL 就能列元信息）或 `manifest()` 读 `contexts[]`；**声明里不含自己上下文的插件直接跳过，根本不 `LoadLibraryW`**。
- 加载顺序：读 PE Machine 判位数 → `LoadLibraryW` → 解析符号 → `abi()` 严格相等 → `manifest()` 校验 schema / `contexts` / `requires` 与本上下文 caps → `load.mode` 排程 → `create(host, ctx)` → 注册到 `ToolRegistry(id, ctx)`。
- 任一环节失败：记录**可读原因**（位数不符 / ABI 不符 / 缺符号 / 上下文未声明 / 缺能力 / manifest 非法 / create 失败），该**上下文实例**标记禁用，另一上下文与其它插件照常。参照 `PainterRegistry::Begin()` 的回退与 `LastError()` 风格。
- `contexts[].load.mode` 缺省：UI 侧默认 `lazy`，服务侧**强制 `eager`**（不接受 `lazy`，采集不能等用户点开）。`arch` 由宿主实测，**不信任插件自报**。
- `ui-only` 插件在服务进程零开销跳过；`service-only` 插件在管理页显示「后台运行中 / 已停止」而非空白导航项。

### 3.4 跨上下文协作（双实例插件的必要机制）

- **peer channel**：`notify_peer(key, payload)` 由宿主代转，前台→后台走现有命名管道，后台→前台走 `WM_COPYDATA`（`ipc::kUiWindowClass` / `kUiToastTag` 同款机制）。对端实例收到 `EVT_PEER_MESSAGE`。插件**不自己开管道、不自己找窗口**。
- **典型联动**：前台点「启用监控」→ `notify_peer("set_enabled", "1")` → 后台实例开监听并回 `notify_peer("state", ...)` → 前台卡片刷新；后台采集到新记录 → `notify_refresh()` + `notify_peer("stats", ...)` → 前台更新计数。
- **配置归属**：双上下文时配置由**服务侧持有**（数据库/机器级 ini 在服务端），前台实例只读；要写入就经 peer 请求，避免两进程写同一文件互相覆盖。纯前台插件仍用宿主本地配置。
- **状态上报**：`peer_state()` 让前台实例知道后台是否活着；服务侧实例状态定期经 IPC 汇总到管理页。主窗口关闭时前台实例停用，**后台实例不受影响**。
- **线程与依赖约束（硬约定）**：`background` / 工作线程加载会跑 `DllMain`，因此 `create()` 必须只做纯初始化，**不得建窗口、不得碰 UI 线程资源**；UI 操作只能在宿主于 UI 线程回调的 `EVT_SHOW` / `EVT_RENDER` 里做。服务进程在 Session 0，**`service` 分支不得加载任何 UI 库、不得弹窗、不得依赖消息循环**；DLL 里 UI 相关依赖用延迟加载或按 `context` 分支隔离，否则服务启动即失败。
- 两个实例各自独立 `destroy`，互不等待；一方崩溃不阻塞另一方（宿主负责禁用的那个上下文）。

### 3.5 UI 渲染策略（分阶段，避免一上来绑死 MSVC ABI）

| 方案 | 优点 | 代价 | 结论 |
|---|---|---|---|
| A. 跨 DLL 传 `IPage` / `IPainter` C++ 虚表 | 改动最小，98 个控件直接可用 | 绑死 MSVC vtable/CRT、位数一致、`std::wstring` 跨界，重演 Skia 插件的硬约束；无法换编译器 | ❌ 不作为公开契约 |
| B. 声明式清单，宿主用现有控件渲染 | 纯 C ABI、跨编译器、天然适配设计系统、列表型工具高度同构 | 自定义界面受清单表达力限制 | ✅ **v1.0 采用** |
| C. 插件提交 `PaintOp` 绘制流 + 事件回流 | 自由绘制、可复用 `PaintRecorder` 金样、可做像素回归 | 工作量大，要设计 widget 命中与事件回流 | ✅ **v1.2 增量开放** |

理由：剪贴板 / 文件记录 / 打印记录三类工具**都是「列表 + 搜索 + 卡片 + 行内操作 + 设置开关」**，B 的表达力足够覆盖；把 C 推迟到真有非列表型工具时再做，且 core 已有 `PaintRecorder` 兜底，不会返工。

### 3.6 生命周期与「真热插拔」

| 阶段 | 语义 |
|---|---|
| v0.9 | 两个宿主都能扫描并按 `contexts` 过滤（服务侧仅装载，行为零变化） |
| v1.0 | 前台按 `eager/lazy/background` 排程；后台 `eager` 常驻；增删 DLL → 重启宿主生效 |
| v1.1 | 管理页**按上下文分别**启用 / 禁用 / 重载即时生效；状态持久化；peer channel 打通 |
| v1.1 卸载 | **软卸载**：注销命令、杀 timer、销毁页面、停止事件分发、**通知对端实例**，但**不 `FreeLibrary`**（照抄 `SkiaPluginReload():199-205`：旧对象 vtable 还在模块里） |
| v2.0 | 引用计数 + quiesce（等待在飞回调归零）后再真 `FreeLibrary`，实现内存级卸载 |

不要在 v1.x 承诺「DLL 物理卸载」——同进程 C++ 对象、回调、定时器任意一处残留都会崩。对外统一表述为「可插拔 = 免重编增删工具、可分别开关前后台」，卸载深度按版本递进。

### 3.7 失败隔离与安全

- 位数：`LoadLibraryW` 失败前先读 PE 头 `Machine`，对 x86 宿主遇 x64 DLL 给出「位数不匹配」而不是静默失败（现有 x86 包遇 x64 Skia 插件就是静默回退，见 `CMakeLists.txt:298`）。**两个进程位数必须一致**，服务插件与 UI 插件同一位数矩阵。
- 坏插件：启动期失败写入 `client.log` 的 `[tool]` 行（含 `ctx=ui/service`）+ 管理页红字；连续崩溃的插件**只禁用出问题的那个上下文**，另一个照常，并在下次启动跳过。
- 目录白名单 + 可选 hash/签名校验；不加载 exe 目录以外的 DLL；依赖 DLL 放自己子目录并显式 `AddDllDirectory`，避免 DLL 劫持。
- 前台插件只能经 `ipc_request` 碰数据，**不得直连数据库**；后台插件才能拿 `db_*` 能力。
- 服务侧插件超时/死循环要能被宿主看门狗标记，不能拖垮 `PipeServer`。

---

## 4. 硬约束（本项目实测，写进契约）

1. **位数一致是硬约束**；MSVC v141↔v143 跨代可用，但 `/MD` 与 `_ITERATOR_DEBUG_LEVEL` 必须一致。纯 C + UTF-8 只是降低风险，不消除位数限制。**前台宿主、后台宿主、插件 DLL 三者位数必须相同**。
2. 默认 `build/` 是 **VS2017 Win32 (x86)**，CI 主产物是 x64，且 **x86 包纯 GDI、没有 x86 Skia 插件**。工具插件要么双位数发布，要么在文档中钉死配套产物，否则「插上去没反应」。
3. core 门禁是**纯文本 grep（含注释）**：`src/airui/core/` 里连注释都不能出现 `windows.h / HWND / RECT / WM_* / DrawText` 等字面量，描述 native 依赖要写「native SDK / handle types」。
4. `core-unix` CI 只构建平台无关 target 且跑 `g++ -Werror` 冒烟；新 ABI 头必须能在无 Win32 头、无 Skia SDK 下通过语法门禁。
5. 每次 CI 现有 5 个 artifact；新增插件产物要与它们并列、命名带位数，别混进 x86 GDI 包。
6. 服务进程在 Session 0：**任何 `service` 上下文的代码路径都不允许出现 UI 依赖或模态弹窗**（项目已因 `MessageBoxW` 踩过坑，见项目记忆）。

---

## 5. 版本路线

> 版本按**能力递进**而非日期排期；每版都必须保持 `ctest -C Release` 全绿 + `check-core-clean.sh` PASSED + 三平台 CI 全绿才可打 tag。

| 版本 | 主题 | 用户可见变化 | 核心验收判据 |
|---|---|---|---|
| **v0.8** | 单体态基线（补 tag） | 无 | 给当前 `47b33b3` 打 tag，作为回归对照 |
| **v0.9** | 插件地基（含双上下文契约） | **无**（纯骨架） | ABI 头 + Registry + 双宿主 Loader + 三种声明的样例 + 契约测试；坏 DLL 被拒且不崩 |
| **v1.0** | 工具箱 MVP | 清单驱动导航、插件管理页、前台三种加载时机 | 删光 DLL 宿主仍可用；`ui-only` 不进服务、`service-only` 不进前台；装入样例即出现工具 |
| **v1.1** | 真可插拔 | 按上下文分别启用/禁用/重载、前后台状态可见 | 免重编增删工具；peer channel 打通；位数/ABI 错误有可读提示；坏插件只禁用对应上下文 |
| **v1.2** | 生态完整 | 自定义绘制 + 双上下文采集工具 | 文件/打印监控以前台页面 + 后台常驻接入；关掉主程序仍在记录 |
| **v2.0** | 稳定化 | 物理卸载 / 隔离 / 可选跨平台 | ABI v1 长期兼容；崩溃隔离；按需评估 Linux/macOS |

### v0.9 — 插件地基（零 UI 变化，但契约一次定死）

**交付**
- `src/pluginapi/airui_tool_abi.h`：`AiruiContext` / `AiruiLoadMode` / HostApi v1（含 `notify_peer` / `peer_state`）/ 5 个导出符号 / 事件 POD / caps / `AIRUI_TOOL_ABI 1`。**双上下文与加载时机在这版就写进 ABI，避免以后破坏性改版。**
- `ToolRegistry`：平台无关，复合键 `(id, context)`，同键替换、`Available()`、`LastError()`。
- `ToolLoader`（前台）与 `CollectorPluginHost`（后台）：共用校验链，各自只加载声明包含自己上下文的插件；后台侧本版只做「扫描 + 过滤 + 装载」，不改变任何现有服务行为。
- `airui-tool-sdk`（INTERFACE target）+ 三个同形状样例验证声明过滤：`tool-hello`（`["ui"]` / `lazy`）、`tool-beacon`（`["service"]` / `eager`）、`tool-dual`（`["ui","service"]` / `background+eager`）。
- `tests/test_tool_abi.cpp`（三平台）：注册表替换/降级、裸 `LoadLibrary` 契约断言、**同 DLL 双上下文分别 create/destroy 且 handle 不串**、`contexts` 过滤；`scripts/check-tool-abi.sh`（`g++ -Werror -fsyntax-only`）。
- `docs/toolbox-plugin-sdk.md`：如何声明 `contexts` / `load.mode`、三种形态怎么选、peer channel、内存所有权、位数矩阵、服务侧禁止事项。

**验收**：三平台 ctest 全绿、core 门禁 PASSED；`ui-only` 样例在服务进程不被 `LoadLibraryW`（用加载计数断言）、`service-only` 在前台不占用；双上下文样例在两个进程各拿到独立 handle；ABI=2 的假 DLL 被拒且不崩；`AIRUI_TOOL_DLL` 环境变量覆盖路径生效。**此版不改任何现有 UI / 服务行为。**

### v1.0 — 工具箱 MVP

**交付**
- `ToolDescriptor` 驱动的导航与页面注册：移除 `App.cpp:201-270` 的三页面硬编码、`AddPage(0/1/2)`、手写 `NavGroup` 与 `switch` 命令分发；`Shell` 用注册表生成导航（解掉 `CreateNavItems()` 被短路的问题）。
- `ToolPageAdapter`：把 `ui.kind=list` 映射到现有 `ListBox` / 卡片 / 搜索框 / 行内按钮 / 设置开关，命令用字符串 key 订阅。
- 前台三种加载时机落地：`eager` 启动加载、`lazy` 首次进入加载、`background` 后台线程预加载（**`create()` 不碰 UI，UI 只在 `EVT_SHOW/RENDER` 于 UI 线程回调**）；宿主策略覆盖要写 `[tool]` 日志。
- HostApi 实现（UI 侧）：`log / alloc / free / ipc_request / toast / config_get / config_set / set_timer / kill_timer / invalidate / notify_refresh / notify_peer / peer_state`。
- 事件泵收口到宿主消息钩子（不再经 `ClipsPage::OnCommand` 转发）；`PluginManager` 接口改为 `HostApi* + handle + context`，保留顺序分发 / 逆序卸载。
- 「插件管理」页：id / 版本 / ABI / 位数 / **声明的 contexts 与 load.mode / 实际值** / **前台实例状态 + 后台实例状态** / 失败原因 / 重载按钮。`service-only` 插件在此页显示状态，不占导航项。
- **剪贴板仍作为内置 `ui-only` 工具**，`SearchPlugin` / `ManualEntryPlugin` 逻辑搬进内置实现；外部 DLL 可用同 id 覆盖。

**验收**：`plugins/` 清空后启动，总览/设置/插件管理正常；放入三种样例 DLL 重启后前台导航/页面按声明出现，服务进程只装载含 `service` 的；`background` 样例加载期间首屏不卡顿；一个坏 DLL（缺符号 / ABI=2 / x64 混入 x86）不影响其它实例；真机回归 F3/F4、Toast、右键菜单、数据刷新不退化。

**风险**：`App&` 解耦面最大；`WM_SETREDRAW` / `RDW_ALLCHILDREN` 那套子控件重绘规则在动态建页时必须重新验证（见项目记忆）；控件 id 从编译期枚举改为运行时分配，要防止与现有 `IDC_*` 撞号；后台线程加载 DLL 的 `DllMain` 死锁风险要用最小样例先验证。

### v1.1 — 真可插拔

**交付**
- `plugins/<id>/<id>.dll` 目录约定 + 依赖隔离（`AddDllDirectory`）+ sidecar `manifest.json`。
- 管理页**按上下文分别**启用/禁用/重载（例：只停后台采集、保留前台查看页）；启用状态落配置，重启后保持。
- peer channel 打通：前台开关 → 后台采集、后台统计/状态 → 前台卡片与管理页；配置写入统一由服务侧持有。
- PE Machine 位数预检、ABI 严格校验、按上下文分别做 caps / `requires` 校验，统一错误码与中文原因。
- 软卸载（注销 + 杀 timer + 销毁页面 + 通知对端，不 `FreeLibrary`）；崩溃插件**只禁用对应上下文** + 下次启动跳过 + `client.log [tool] ctx=...`。
- SDK 完善：CMake 模板、三种形态样例补全行为、ABI 兼容测试套件、版本号与 tag 策略。
- CI：x86 / x64 双位数插件产物 artifact；`test_tool_abi` 扩成「加载→事件→peer→软卸载→重载」闭环。

**验收**：不重编主程序，只增删 DLL 文件并在管理页点「重载」，前后台工具列表即时变化；把 x64 插件放进 x86 宿主得到「位数不匹配」；停用某插件的 `service` 上下文后其后台监听停止但前台页面仍可查看历史；一个在 `create()` 里崩的插件只连累自己那个上下文。

### v1.2 — 生态完整（业务模块在此落地）

**交付**
- 自定义绘制：`ui.kind=custom`，插件提交 `PaintOp` 序列，宿主用 `IPainter` 执行；命中矩形与事件回流 POD 化；复用 `test_paint_recorder` 金样做跨后端一致性断言。
- **文件监控 / 打印监控以 `contexts: ["ui","service"]` 双上下文 DLL 交付**：服务侧实例常驻采集入库，前台实例提供列表页、开关与设置，两者经 peer channel 联动。落地 ROADMAP 里排最后的 `FileWatchPlugin` / `PrintWatchPlugin`（此时才动业务，遵守用户排期）。另可交一个 `service-only` 纯后台插件验证「不需要前台」的形态。
- IPC 协议版本化；`entries.source` 区分工具来源；客户端刷新通知按工具粒度而非全量重建。
- 插件间依赖与顺序（`requires_tool`）、设置页分区。

**验收**：**关闭主窗口后仍在记录**，重开能看到期间的数据；管理页分别显示该插件「前台已加载 / 后台运行中」；前台停用不影响后台；自定义绘制工具在 GDI 与 Skia 两个后端下 `PaintOp` 序列一致。

### v2.0 — 稳定化（按需启动）

- 引用计数 + quiesce 后真 `FreeLibrary`；崩溃隔离评估（独立进程 + IPC / 共享内存，或 Windows 作业对象）。
- 跨编译器支持验证（纯 C + UTF-8 已在 v0.9 铺路，可试 clang-cl / MinGW 编译插件）。
- ABI v1 长期支持策略：`size` + caps 渐进；只在破坏性变更时出 v2 并同时加载两代。
- Linux/macOS 工具箱本体：依赖 Skia 版 `IWindow` 与跨平台窗口壳（ROADMAP 已标「当前不可行」），**不作为 v1.x 目标**。

---

## 6. 现有代码迁移映射

| 现在 | 目标版本 | 去向 |
|---|---|---|
| `src/plugins/SearchPlugin.*` | v1.0 | 内置剪贴板工具（`ui-only`）内部的搜索防抖，改 `host.set_timer` |
| `src/plugins/ManualEntryPlugin.*` | v1.0 | 内置剪贴板工具的录入逻辑；`SetWindowSubclass((App*)dwRefData)` 改成命令订阅 |
| `src/core/Plugin.h` `IFeaturePlugin` | v1.0 | 废弃；`PluginManager` 作宿主事件泵，接口换 `HostApi* + handle + context`，**前后台各一个实例** |
| `src/pages/ClipsPage.*` | v1.0 | 内容搬进剪贴板工具；`OnCommand` 的插件转发删除 |
| `src/pages/OverviewPage.*` / `ConfigPage.*` | v1.0 | 宿主内置；`ConfigPage` 的「功能模块（插件注册）1/3」假文案改成读真实 `ToolRegistry` 并按上下文统计 |
| `src/core/App.cpp:213-270` 导航硬编码 | v1.0 | 注册表 + manifest 的 `contexts[ui].nav` 驱动 |
| `src/service/main.cpp` 剪贴板监听 | v0.9 / v1.2 | v0.9 加 `CollectorPluginHost` 扫描过滤；v1.2 与新采集插件并列运行，原监听保持 |
| `src/ipc/Ipc.h` / `NotifyClientRefresh()` | v1.1 | 复用为 peer channel 的后台→前台通路，不新建管道 |
| `PainterRegistry` / `PaintProviders.cpp` / `SkiaPluginAbi.h` | — | **保持不动**，仅作为结构范本 |
| `ROADMAP.md` §1 B 文件/打印监控 | v1.2 | 从「规划中」改为「以双上下文插件交付」 |

---

## 7. 构建与 CI 改动清单

- `CMakeLists.txt`：新增 `airui-tool-sdk` INTERFACE、`tool-hello` / `tool-beacon` / `tool-dual` SHARED target，`OUTPUT_NAME` / `ARCHIVE_OUTPUT_NAME` 防撞库，产物与 exe 同目录；`src/plugins/*.cpp` 的 `GLOB_RECURSE` 停止并入 `COMMON_SOURCES`；`airecordd` 链 `CollectorPluginHost` 但不链任何 airui UI 库。
- `src/pluginapi/` 加入 `include_directories`；**不要放进 `src/airui/core/`**。
- 新增 `check-tool-abi` 门禁 target（三平台），`test_tool_abi` 加入 ctest；`core-unix` 作业加一步语法门禁。
- CI 产物：在现有 5 个 artifact 之外，按位数出 `tool-sample-windows-x86` / `-x64`；另加一个「服务进程加载断言」测试，验证 `ui-only` 插件没被 `LoadLibraryW`。
- 本地开发固定 `env -u https_proxy -u http_proxy -u all_proxy -u no_proxy` + `cmake --build build --config Release -- -m:2`；不要重新 configure 既有 `build/`（VS2017 Win32 缓存）。

---

## 8. 风险登记

| 风险 | 影响 | 缓解 |
|---|---|---|
| 位数混装（默认 x86 / CI x64，且要乘以前后台两进程） | 插件静默不加载，最难排查 | PE Machine 预检 + 管理页显示位数 + 双位数产物 + `[tool] ctx=` 日志 |
| 同进程插件崩溃带崩宿主 | 数据丢失风险 | 只禁用出问题的上下文；后台采集在服务进程与主窗口隔离；v2 评估沙箱 |
| **双实例写同一配置** | 前后台互相覆盖设置 | 配置归服务侧持有，前台只读 + 经 peer 请求写入；纯前台插件用本地配置 |
| **`background` 线程加载 DLL 的 `DllMain` 死锁 / UI 线程资源** | 启动卡死或随机崩 | 契约规定 `create()` 不碰窗口，UI 只在宿主 UI 线程回调；最小样例先验证 |
| **服务进程误链 UI 依赖 / 弹模态框** | Session 0 里服务启动失败或线程阻塞 | `service` 分支禁用 UI 库与弹窗；依赖延迟加载；CI 加服务侧加载断言 |
| 动态建页触发重绘/闪烁回归 | `WM_SETREDRAW` + `RDW_ALLCHILDREN` 规则被绕过 | v1.0 真机回归展开/收起/切页；沿用 `screen_shot.py` 截图比对 |
| 控件 id 运行时分配撞号 | 命令串台 | 宿主统一分配段位，插件只用字符串 key |
| ABI 提前固化不足 | 后续每加能力都要全量重编插件 | `size` + caps + `requires` 三重前向兼容；**上下文与时机维度在 v0.9 一次定死** |
| 过早做自定义绘制 | 工期膨胀、绑死 MSVC ABI | v1.0 只做声明式，v1.2 再开 `PaintOp` 流 |

---

## 9. 待拍板（默认已给出，可直接按默认执行）

1. **剪贴板是否也 DLL 化？** 默认：**保留宿主内置 `ui-only` 工具**，走同一 `ToolRegistry`，允许外部 DLL 同 id 覆盖。
2. **首个外部样例插件？** 默认：v0.9 三个最小样例分别验 `ui-only` / `service-only` / 双上下文；文件/打印监控压到 v1.2。
3. **位数策略？** 默认：x86 / x64 双发，前台 exe、后台 exe、DLL 三者位数一致。
4. **是否需要第三方（跨编译器）插件？** 默认：**按「需要」设计** —— v0.9 起锁死纯 C + UTF-8 + 固定调用约定。
5. **「可插拔」是否要求运行时物理卸载？** 默认：v1.1 软卸载即算达标，物理卸载放 v2.0。
6. **宿主能否覆盖插件声明的 `load.mode`？** 默认：**能，但必须显式记录**（省电 / 调试 / 管理员禁用），管理页同时显示「声明值 / 实际值」；`service` 侧一律 `eager`，不接受 `lazy`。
7. **双上下文是否必须单 DLL？** 默认：**是**，一个包一个 DLL 一个版本号，用 `context` 参数分支；若将来出现服务侧绝不能含 UI 代码的插件，再允许可选的第二 DLL，但 ABI 不变。

---

## 10. 判据（写进代码评审清单）

- 插件 DLL 只 include `airui_tool_abi.h`，**不 include `App.h` / `windows.h` / 任何 airui C++ 头**。
- 边界上只有 POD 与 `const char*`；**谁分配谁释放**；`create/destroy/on_event` 全部带 `AiruiContext`。
- **前台/后台与加载时机只由 manifest 的 `contexts[]` / `load.mode` 决定**，宿主不得硬编码插件分类；覆盖加载策略必须写日志并显示实际值。
- 两个宿主各自过滤：声明不含自己上下文的插件**不调用 `LoadLibraryW`**。
- 加载必过「位数 → ABI → 符号 → manifest/contexts → 按上下文 caps → create」六道校验，任一失败只禁用该上下文且有可读原因。
- 双实例不共享内存、不自己开管道；跨上下文一律走 `notify_peer()`；配置由服务侧持有。
- `service` 上下文代码路径零 UI 依赖、零模态弹窗；`background` 加载的 `create()` 不碰 UI 线程资源。
- 任何宿主能力都经 `AiruiHostApi` 表取得；前台插件不得直连数据库。
- 增删工具不重编主程序；`ToolRegistry` 的降级与 `LastError()` 行为与 `PainterRegistry` 一致。
- 每个版本合入前：三平台 CI 全绿、`ctest -C Release` 全绿、`check-core-clean.sh` PASSED、`check-tool-abi.sh` PASSED。
