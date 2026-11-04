# Phase 0 Sign-off

**Date:** 2026-11-05  
**Branch:** main  
**Baseline tag:** phase0-baseline (034bdc2)

## 退出标准检查

1. **airui_gallery Windows 行为完全不变**
   - 编译通过：`cmake --build . --config Release` → 0 errors
   - 进程可启动并保留标题 `AirUI Gallery`，窗口可最大化/最小化/关闭，拖动边框可缩放，布局自动适配
   - 截图/DrawCall 回归：`test_draw_calls` 通过，gallery 交互（hover/pressed）行为保持

2. **core/ 无平台头**
   - `scripts/check-core-clean.ps1`  → PASSED (11 core files, 0 platform references)
   - `scripts/check-core-clean.sh`  → PASSED (WSL Ubuntu 20.04)

3. **Linux 编译与烟测**
   - `scripts/check-core-linux-smoke.sh` → PASSED
     - g++ 9.4 (Ubuntu 20.04) 编译 `Theme.cpp Layout.cpp Animation.cpp ControlsCore.cpp`
     - 链接 `libairui-core.a`
     - HAL 可用性检查（Rect/Point/Color/Layout/Animation/Observable）运行通过
     - `test_controls_core` 通过
   - 基线平台要求 Ubuntu 22.04 + GCC 11 已在脚本中说明；WSL 20.04 作为占位验证通过；CI 可升级

4. **控件 HAL 定稿（Phase 0）**
   - `airui/ControlsCore.h/.cpp`：`ButtonStyle` + `ControlState` + `PaintButton/PaintCheckbox/PaintRadio`
   - 自绘控件实现完全在 core，Win32 后端通过 `CanvasPainter` 适配
   - `Canvas::Button` 委托到 core 并保持 DrawCall 高层记录，Checkbox/Radio 委托到 core 保持逐原语录制
   - 画廊已改用 `g.Checkbox`/`g.Radio`（无内联绘制），按钮保持逐原语像素一致

5. **CI 门禁**
   - `ctest -C Release`  → 6/6 通过
   - `test_core_layout`  → 行/列/网格/边距 通过
   - `test_controls_core` → Button 4 种样式 + Checkbox + Radio 通过
   - `test_theme_manager`, `test_draw_calls`, `test_locale`, `test_component_gallery` 通过
   - `test_ui` 未注册为 ctest（端到端脚本，需运行 airecord.exe + IPC）

6. **构建拆分**
   - `airui-core` STATIC + `airui-win32` STATIC
   - 所有可执行文件链接 `airui-win32`，不再直接编译 `${AIRUI_SOURCES}`
   - `pthread` 在非 Windows 下链接

## 变更清单（节选）

- 目录拆分 `git mv` → `src/airui/core/include/airui` / `core/src` / `platform/win32/airui` / `platform/win32`
- `types.h` / `hal.h` 新增；`Rect/Point/Size/Color/TextFormat` 平台无关
- `Animation` 注入 `IClock`（`SetClock/Now()`），去 `GetTickCount64`
- `Layout` 解耦：`ILayoutItem`，修复叶节点未填充、容器递归、`forward` 隐式根、`m_widgetRects`
- `Observable.h` 剥离 `ListBoxBinder` → `platform/win32/airui/ObservableListBox.h`
- `ControlsCore` 新增并纳入 `airui-core`
- `CanvasPainter` 适配 `IPainter`→GDI
- `Canvas::Button/Checkbox/Radio` 委托 core，画廊改为调用
- `Fonts.h` 改为包含 `hal.h` 复用 `FontRole`
- `BtnStyle` 统一为 `using BtnStyle = ButtonStyle`
- `CMakeLists` 拆分目标，增加 `test_core_layout/test_controls_core`，`enable_testing`
- 脚本：`check-core-clean.{ps1,sh}`、`check-core-linux-smoke.sh`
- `.gitignore` 增加 `build-linux-smoke/ .phase0_backup/ test_output* *.json *.png *.log`

## 已刻意不做（Phase 0 范围外）

- TextBox/ListBox 行为重写（保持 Win32 原生）
- Skia 引入、跨平台窗口/绘制（Phase 1/2）
- `RECT/COLORREF` 全量替换（仅 core 层禁止平台，Win32 层保留）

## 下一步

- Phase 0 step 5+：NavBar/Caption/StatusBar 迁入 ShellCore HAL，推进 `IWindow` 完整事件注入
- Phase 1：Win32 HAL 化（`GdiPainter`/`Win32Window`），开始 Linux Skia 原型
