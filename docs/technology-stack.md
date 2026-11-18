# 当前技术栈与实现总结

> 版本：`v0.9.0`。本文汇总当前 `main` 分支用到的技术、分层与关键实现，
> 供接手者快速建立全局认识。源码在 `src/`，构建/打包/CI 在根目录各入口。

## 1. 整体分层

| 层 | 目录 | 作用 | 依赖 |
|---|---|---|---|
| UI 框架层 | `src/airui/core` | 平台无关 UI 框架（airui-core 静态库）：IPainter 抽象、控件、布局、主题、ShellCore 状态机 | 仅 HAL 接口，无平台头 |
| UI 平台层 | `src/airui/platform/win32` | Win32 后端：GDI 绘制、窗口外壳、控件、Skia 插件加载 | Win32/GDI/DWM |
| Skia 后端 | `src/airui/platform/skia` | 可选 Skia 绘制后端 + 插件（airui-skia.dll） | Skia SDK（构建时）、C ABI 插件 |
| 业务应用层 | `src/core`, `src/pages`, `src/plugins`, `src/agent` | AireRecord 应用：App 容器、页面、剪贴板监控、后台代理 | 用 UI 框架层，`namespace arecord` |
| IPC 层 | `src/ipc` | 持久命名管道双向流（PipeClient/PipeServer） | Win32 named pipe |

- **两个 `core` 的边界**（`docs/design-patterns.md` §1.1）：`src/airui/core/` = 平台无关框架；
  `src/core/` = 业务应用层。业务层用框架层，不反向。

## 2. 绘制与渲染

### IPainter 抽象（`src/airui/core/include/airui/hal.h`）
- 平台无关绘制接口：`FillRect / FillRectAlpha / FillRound / StrokeRect / StrokeRound /
  StrokeDashed / Text / TextBlock / MeasureText / PushClip / PopClip / Translate`。
- 关键：`Text/TextBlock/MeasureText` 用 **`const wchar_t* + size_t`**（P0-3 ABI v2 后），
  **不跨虚表传 `std::wstring`**——Skia 插件跨 DLL/CRT 边界时安全。
- 各后端实现：`GdiPainter`（win32）、`SkiaPainter`（skia）、`PaintFrame`（帧级路由）、
  `PaintRecorder`（录制/金样）、`CanvasPainter`（业务层适配）。

### 帧级路由（PaintFrame）
- `Shell::Paint` 开一帧 → 所有 `gfx::*` / `CanvasPainter` 经 `PaintFrame` 路由到当前后端
  （GDI 默认，Skia 可选）。文字策略运行时探测：后端能画宽字符 → `text=backend`，
  否则宿主 `DrawTextW` 重放（`text=host`）。
- GDI 优化：按 `BeginPaint` 的 `rcPaint` 脏区裁剪绘制 + 复用后缓冲（`PaintDC::BackBuffer`）
  + 按脏区跳过整条带（caption/nav/status/content）。

### Skia 插件（P0-3 ABI v2）
- `airui-skia.dll` 通过稳定 C ABI 动态加载：`airui_skia_describe/create/destroy/pixels/row_bytes`。
- `AiruiPluginDesc{ size, abi, caps, painterIfaceRev }` 分开「C 函数形状」与「C++ 虚表布局」两个契约；
  `LoadPlugin` 校验 `abi + size + painterIfaceRev`，不匹配回退 gdi；能力协商
  `caps & AIRUI_CAP_WIDE_TEXT` 替代像素探测。
- 插件路径：`LoadPlugin` 解析到 `<exe-dir>\airui-skia.dll` 绝对路径 + `SetDllDirectoryW(<exe-dir>)`
  让依赖（skia.dll、icu、harfbuzz…）也能从安装目录加载。

## 3. 进程模型与 IPC

### 进程
- **系统服务** `airecordd -service`（Session 0）：持有机器级共享库
  `%PROGRAMDATA%\AireRecord\airecord.db`、监听全局管道、为每个活动会话拉起隐藏 UI、看护会话。
- **每会话一个** `airecord.exe`（UI + 同进程剪贴板代理 AgentHost，后台线程 + 隐藏窗口）。
- 生命周期：服务启动拉 UI；UI 启动确保服务一次；服务停止/崩溃 → UI 存活进离线态
  （SCM failure-actions 崩溃自动重启）。

### IPC（持久双向命名管道，`src/ipc`）
- 管道 `\\.\pipe\AireRecordIpc`，帧协议 `TAG\nPAYLOAD\n`（REQ / RSP / EVT）。
- 持久连接 + 后台 reader 线程；断连实时检测（`ReadFile` 返回 `ERROR_BROKEN_PIPE`）；
  EVT 广播（如新剪贴板记录 → 所有 UI 刷新）。
- 多实例（`PIPE_UNLIMITED_INSTANCES`）+ 每会话一连接；`PING\t<session>` 心跳保活。

### 心跳与看护
- UI 每 3s 发 `PING\t<session>`（服务在线也发），`g_sessionSeen` 保持新鲜，服务端
  `WatchThread` 的 60s 兜底不会误判。服务端 `OnClientDisconnect` 实时重启崩溃的会话 UI。

## 4. 剪贴板监控与离线韧性

### 剪贴板监听
- `AgentHost`（`src/agent`）后台线程 + 隐藏窗口 `AddClipboardFormatListener` → `WM_CLIPBOARDUPDATE`
  → 读 `CF_UNICODETEXT` → `SAVE` 转发给服务入库（过滤应用自身 COPY）。

### 离线
- 服务不可用时：UI 存活 + 告警（状态栏红字/标题栏徽标/横幅）；剪贴板写入
  `%APPDATA%\AireRecord\pending.db`（本地 SQLite 暂存）；恢复后 `FlushPending()` 批量同步
  回服务（服务端按内容去重），清离线告警。

## 5. 数据与配置

- **共享库**：`%PROGRAMDATA%\AireRecord\airecord.db`（机器级，服务独占写）。
- **暂存库**：`%APPDATA%\AireRecord\pending.db`（用户级，离线缓冲）。
- **配置**：`machine::Get/Set` 读写 `%PROGRAMDATA%\AireRecord\service.ini`（管道名、运行模式、
  策略键如 `[ui] close_behavior`）。
- **SQLite**：`src/core/Database`（UTF-16 无缝）、`third_party/sqlite/sqlite3.c`（vendored）。

## 6. 构建 / 打包 / CI

### 构建
- `cmake_minimum_required 3.15`；`project(AireRecord VERSION 0.9.0)` 单一版本来源 →
  生成 `Version.h`（`AIRUI_VERSION_W`）。
- 目标库：`airui-core` / `airui-win32` / `airui-skia`（可选）/ `app-common` / `sqlite3`。
- `target_include_directories` 按目标精确授权（无全局 include）；`airui_console_target()` 用
  `if(MSVC)` 保护 `/SUBSYSTEM:CONSOLE`。
- 工具链：本地 VS2017 x86，CI VS2022 x64/x86；**MSVC-only**（禁 MinGW）。

### 打包（NSIS）
- `packaging/airecord.nsi`：Modern UI 2、管理员；**架构感知**（`${RunningX64}` → 装 x64 包 +
  x64 Skia 到 `$PROGRAMFILES64`；x86 → 装 x86 包（GDI-only）到 `$PROGRAMFILES32`）。
- `build-installer.ps1`：自动探测 makensis、解析 Skia bundle 导出表，**仅当 ABI v2
  （`airui_skia_describe`）才 `/DSHIP_SKIA=1`**；安装先停服务杀进程（`StopAireRecord` 宏）；
  注册 `AireRecord` SCM 服务并**修复其二进制路径**；卸载停服务删文件。

### CI（GitHub Actions）
- 作业：`airui-core`（ubuntu/macos）、`full build`（windows x64/x86）、`skia-windows`（非阻塞）。
- 门禁：`check-core-clean`（core 无平台标识符）、`check-skia-syntax`、`test_paint_recorder` 金样
  （平台无关结构断言）、`clang-format` 检查、clang-tidy 报告（非阻塞）。

## 7. 工程实践（整改沉淀）

- **门禁自动化**：约束不靠人工，CI 强制（core-clean）。
- **线程安全**：后台 IPC 走可 join 线程 + `RefreshResult` 经 `WM_APP+6` 回 UI 线程写共享状态；
  UI 线程不做阻塞 IPC。
- **RAII**：模态禁用/重绘冻结/列表批量用 RAII（`ScopedWindowDisable`/`ScopedRedrawFreeze`/
  `ScopedBatch`），异常/提前返回也正确恢复。
- **ABI 契约**：跨 DLL 的 C++ 虚表布局用 `painterIfaceRev` 显式校验。
- **测试资产**：孤儿测试归入 `tools/diagnostics/`；`ctest -C Release` 22/22；
  金样为平台无关结构断言。
- **决策记录**：`docs/adr/` 4 篇（工具链矩阵/版本号/服务-UI 生命周期/插件 ABI v2）。

## 8. 关键文档索引

- `docs/remediation-plan.md`（整改方案，各批次完成状态）
- `docs/remediation-summary.md`（整改汇总）
- `docs/adr/`（决策记录）
- `docs/build-conventions.md`（构建矩阵 + 打包）
- `docs/skia-backend.md`（Skia 后端、ABI、像素差异）
- `docs/design-patterns.md`（设计模式、两个 core 边界）