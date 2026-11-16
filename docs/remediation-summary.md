# p-system 整改总结（2026-11-15 ~ 2026-11-17）

> 按 `docs/remediation-plan.md` 完成 P0/P1/P2/P3 全部批次，并修复了审查期间暴露的
> 「主窗口最小化后周期性置顶」运行期 bug（根因是服务端 60s 心跳兜底因 UI 停发心跳而触发）。
> 本文是整改的**汇总视图**：已完成项、当前架构、运行期修复、验收与剩余事项。

## 1. 整改总览

| 阶段 | 目标 | 批次 | 状态 |
|---|---|---|---|
| P0 | 消除崩溃/泄漏，让约束第一次生效 | P0-1..P0-5 | ✅ 全部完成 |
| P1 | 工程化收敛 | P1-1..P1-6 | ✅ 全部完成 |
| P2 | 架构边界 | P2-1..P2-5 | ✅ 全部完成 |
| P3 | 测试/文档/清理 | P3-1..P3-5 | ✅ 全部完成 |
| 运行期修复 | 审查后发现的真实 bug | 多个 fix 提交 | ✅ 完成 |

核心判断：问题不是单点缺陷，而是「文档写下的约束没有自动化环节执行」。本次整改先让
**约束生效（P0-1 门禁）→ 消除运行时风险（P0-2/4/5）→ 收敛架构（P1/P2）→ 对齐文档（P3）**。

## 2. P0 —— 阻断项（已闭合）

- **P0-1 CI 门禁**：`check-core-clean` 接入 `core-unix` / `windows` / `windows-x86` 三个 job；
  脚本加固（`set -eu`、grep rc 区分、正则抽到 `scripts/core-forbidden.regex` 共享）。
  → `src/airui/core/` 的平台隔离从「人工维持」变为「CI 强制」。
- **P0-2 线程/竞争**：`std::thread(...).detach()` → 可 join 具名线程（`m_reconnectThread` /
  `m_refreshThread`），`~App()` 置位 `m_shuttingDown` 后 join；共享状态（`m_stats` /
  `m_entries` / `m_svcState` / `m_svcText`）只由 UI 线程写（后台线程堆分配 `RefreshResult`
  经 `WM_APP+6` 回投）；UI 控件读取留在 UI 线程。
- **P0-3 插件 ABI v2**：新增 `src/pluginapi/airui_plugin_abi.h`（`AiruiPluginDesc{size, abi,
  caps, painterIfaceRev}`）；`IPainter::Text/TextBlock/MeasureText` 改为 `const wchar_t* + size_t`
  （消除跨 DLL `std::wstring`）；`LoadPlugin` 校验 `abi+size+painterIfaceRev`，不匹配回退 gdi；
  能力协商（`caps & AIRUI_CAP_WIDE_TEXT`）替代像素探测。
- **P0-4 A11y 泄漏**：`SetAccessibleName/Description/Value` 的 `SetPropW(new std::wstring)` 泄漏
  → 空实现 + `TODO(a11y)`，不再泄漏；`LiveRegion::Announce` 仍触发 WinEvent。
- **P0-5 RAII 锁**：新增 `airui/RaII.h`（`ScopedWindowDisable` / `ScopedRedrawFreeze`）；
  `RunModalDialog` 的 `EnableWindow` 对、`SwitchPage` 的 `WM_SETREDRAW` 冻结、`ListBox::ScopedBatch`
  批量重建全部改 RAII；`WM_DESTROY` 的 `PostQuitMessage` 在模态嵌套时不误投。

## 3. P1 —— 流程与工程化（已收敛）

- **P1-1/P1-4**：`.gitignore` 补 `build-core/` 等 5 个构建目录 + `artifacts/`；`build.bat`/
  `build.sh` 默认跑测试（`notest`/`--no-test` 显式跳过）。
- **P1-2/P1-6**：版本单一来源 `project(AireRecord VERSION 0.9.0)` → 生成 `Version.h`
  （`AIRUI_VERSION`/`AIRUI_VERSION_W`），页脚/`Responsive.cpp`/`res/app.rc` 收敛；新增
  `CHANGELOG.md`；`docs/adr/` 4 篇决策记录（工具链矩阵/版本号/服务-UI 生命周期/插件 ABI v2）。
- **P1-3/P1-5 ①**：`ci.yml` 删重复注释；文档标注 `skia-windows` 非阻塞（required checks =
  core-unix + windows）；新增根 `vcpkg.json`（`builtin-baseline` + `overrides` 锁 skia m148），
  缓存 key 用 `hashFiles('vcpkg.json')`；新增 `.clang-format`/`.editorconfig` + CI「Format
  check」（只查 PR 改动文件，非阻塞）+ `scripts/requirements.txt`。
- **P1-5 ②（clang-tidy）**：新增 `.clang-tidy`（`bugprone-*` + `clang-analyzer-*`，排除两个
  过于嘈杂的检查，`WarningsAsErrors: ''` 报告不失败）+ CI「Static analysis」非阻塞步骤 +
  `CMAKE_EXPORT_COMPILE_COMMANDS ON`。

## 4. P2 —— 架构边界（已落地）

- **P2-2**：业务层（CORE+IPC+PAGES+PLUGINS+AGENT）抽 `app-common` 静态库、`sqlite3.c` 抽独立
  `sqlite3` 库（编译一次），所有 exe/测试改链接；`airui_console_target()` 用 `if(MSVC)` 保护
  `/SUBSYSTEM:CONSOLE`（这是 Linux CI 编不了测试目标的技术根因）；`cmake_minimum_required` 3.15。
- **P2-1**：按目标 `target_include_directories` 精确授权，然后删除全局 `include_directories`；
  `platform/skia`、`third_party/sqlite` 只在需要的目标可见；core 编译期隔离由 CMake 强制。
- **P2-3**：`docs/design-patterns.md` §1.1 明确两个 `core` 边界；业务层类包进 `namespace arecord`
  （消费者用 `using namespace arecord`）；目录重命名延后 v1.1。
- **P2-4**：`FrameSlot::busy` → `std::atomic<bool>` + `compare_exchange` 认领；文档化
  「换肤/开帧只允许 UI 线程」；`PaintFrame::AssertUiThread`（release 空操作）。
- **P2-5**：修 `Gfx.h` `UsePen(const UseBrush&)` → `UsePen(const UsePen&)` 拷贝构造 typo；
  `Ipc.cpp` `WideToUtf8`/`Utf8ToWide` 空输入 `n<=0` 早退；`Database.cpp` DDL 失败补
  `sqlite3_close`（修句柄泄漏）；`SaveDC` 守卫；`WM_ERASEBKGND` 补前置条件注释（不改擦除行为）。

## 5. P3 —— 测试/文档/清理（已完成）

- **P3-1**：11 个孤儿测试整理（8 个移到 `tools/diagnostics/` + README，3 个删除）；
  `test_font_lab` 注明为手动工具；ctest 列表严格化（缺目标 FATAL）；`test_maximize` 返回码修正。
- **P3-2**：`PaintRecorder::ToString()` 整场景规范化序列化，落盘 `tests/golden/paint_recorder.golden`
  （2255B ASCII），测试对照基线（`UPDATE_GOLDEN=1` 重写）；跨平台（core-unix/windows 各平台对照
  同一份提交金样，平台差异使该平台 required ctest 失败）。
- **P3-3**：按清单修订 README/ROADMAP/design-patterns/cross-platform/v0.8-baseline/
  toolbox-plugin-plan/skia-backend，新增 `docs/build-conventions.md`；`scripts/shot.py` 路径修正。
- **P3-4**：`git rm --cached` 7 个 `gallery_*.txt` + `.gitignore` 补 `gallery_*.txt`/`artifacts/`；
  `third_party/sqlite/README.md`（SQLite 3.45.1、来源、Public Domain、编译选项）。
- **P3-5**：散文注释欠账统一 `TODO(<领域>): ...`（Layout/gallery/Accessibility/ThemeManager/计划）；
  汇总进 ROADMAP「代码内 TODO 检索」；CI 加「TODO debt count」报告步骤（仅统计不失败）。

## 6. 运行期修复（审查/用户反馈暴露）

### 6.1 主窗口最小化后周期性置顶（根因 + 数据）
- **数据**：`client.log` 中 `[second-instance] existing=... hidden=1` **每 ~60s 一次**。
- **根因**：服务端心跳兜底 `kHeartbeatBackstopMs=60000`，`WatchThread` 每 2s 检查
  `g_sessionSeen[session]`；UI 只在**启动时** `RegisterSession()` 发一次 `PING\t<session>`，
  `TIMER_SVC_RECONNECT` 有 `if(!m_offline){KillTimer;return;}` —— **服务在线时停发心跳**，
  `g_sessionSeen` 60s 过期 → 服务端每分钟拉重复 `airecord --hidden` → 单实例守卫
  `SW_RESTORE`+`SetForegroundWindow` 把最小化窗口弹回置顶。
- **修复**：`TIMER_SVC_RECONNECT` 去掉 `!m_offline` 早退，**服务在线也每 3s 发 `PING\t<session>`
  心跳**（后台线程），`g_sessionSeen` 持续刷新，60s 兜底永不触发；`WM_APP+7` 加 `wParam==2`
  例程心跳分支（只重排定时器，不弹「服务已恢复」Toast）。另保留 `src/main.cpp` 单实例守卫
  `!startHidden` 防御层（重复实例不再打扰最小化窗口）。

### 6.2 离线切页/点击卡顿
- 若干 fix：`PipeClient::Request` 离线快速失败（不阻塞 UI）；`RefreshData` 异步化（后台线程 +
  `WM_APP+6` 用消息回 UI）；`SwitchPage` 异步 `InvalidateRect`（不再同步整帧）；`ApplyEntries`
  数据未变跳过 ListBox 逐行重建；离线重连探测挪后台线程（消除 3s 一次 ~1s UI 卡）。
- 这些后续由 P0-2 的「可 join 线程 + 结果结构」系统化根治。

## 7. 当前架构速览

- **进程**：系统服务 `airecordd -service`（Session 0，持有机器级共享库 `%PROGRAMDATA%\AireRecord\
  airecord.db` + 全局管道 `\\.\pipe\AireRecordIpc`）+ 每会话一个 `airecord.exe`（UI + 同进程
  剪贴板代理 AgentHost，后台线程 + 隐藏窗口）。
- **生命周期**：服务启动拉每会话隐藏 UI；UI 启动确保服务一次；服务停止/崩溃 → UI 存活进离线态
  （告警 + 剪贴板暂存 `%APPDATA%\AireRecord\pending.db`）；SCM failure-actions 崩溃自动重启；
  UI 恢复后同步 pending → 清离线。
- **IPC**：持久双向命名管道，帧协议 `TAG\nPAYLOAD\n`（REQ/RSP/EVT），断连实时检测
  （`ERROR_BROKEN_PIPE`）。
- **渲染**：`IPainter` 接口（P0-3 后 Text 系 `wchar_t*+len`）；GDI 默认，Skia 插件可选（ABI v2）。
- **构建**：`cmake_minimum_required 3.15`；VS2017 本地 / VS2022 CI；MSVC-only。

## 8. 验收

- `cmake --build build --config Release --clean-first` 全绿（exit 0）。
- `cd build; ctest -C Release` **22/22**。
- `scripts/check-core-clean.ps1` PASSED（50 core 文件，0 平台引用）。
- `git diff --check` 干净；`main...origin/main` 同步。
- 运行冒烟：服务启动→拉 UI；服务停止→UI 存活响应（离线延迟 16ms 内，无 >500ms 尖峰）。

## 9. 剩余事项 / 后续

- 目录重命名 `src/core/` → `src/app/`（P2-3 延后到 v1.1）。
- clang-tidy 的完整分析需在有 clang-tidy + compile_commands 的 CI runner 上跑（本地无二进制）。
- Skia 插件侧（`SkiaPlugin.cpp`/`SkiaPainter`）的 ABI v2 需 CI Skia job 用真 SDK 编译验证。
- 渲染像素/字体度量（GDI vs Skia）按计划保持不动，属既有结论。