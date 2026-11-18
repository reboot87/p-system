# AirUI 库化契约（UI Library Contract）

> 版本：`v0.9.0`。本文声明 `airui-core` / `airui-win32` 作为**可复用 UI 库**的公共 API
> 表面、分层、稳定契约与使用规则。应用（AireRecord / gallery）与第三方程序都按此契约 link。
> 后续声明式布局（Android 模型）将在此库之上实现。

## 1. 库与分层

| 库 | 位置 | 平台 | 链接 |
|---|---|---|---|
| `airui-core`（STATIC） | `src/airui/core/include` | 平台无关 | 必选 |
| `airui-win32`（STATIC） | `src/airui/platform/win32/airui` | Win32 | 必选（Windows 应用） |
| `airui-skia`（可选） | `src/airui/platform/skia` | Win32/Skia | 可选后端 |

- 依赖：`airui-core` 仅依赖 HAL 接口（无平台头）；`airui-win32` 依赖 `airui-core` + Win32。
- CMake 已按目标精确 `target_include_directories`（无全局 include），见 `docs/build-conventions.md`。

## 2. 公共 API 表面（稳定层）

### 2.1 core 层（平台无关）
- **HAL / IPainter**：`hal.h` —— 绘制接口 `IPainter`（`FillRect/…/Text/TextBlock/MeasureText/
  PushClip/PopClip/Translate`）；文字用 `const wchar_t* + size_t`（不跨虚表传 `std::wstring`）。
- **类型**：`types.h` —— `Rect / Point / Size / Color / TextFormat(dt::*)`。
- **主题**：`Theme.h` —— `Palette / Metrics / MakeColor`。
- **ShellCore**：`ShellCore.h` —— 平台无关外壳状态机（Caption/NavBar/StatusBar），后端渲染。
- **控件**：`ControlsCore.h` + `ControlsCoreClasses.h`（基础控件）、`ControlsAdvanced/Extra/
  Enterprise/Studio*.h`（扩展控件）—— 核心控件绘制走 `IPainter`，可在任一后端渲染。
- **布局**：`Layout.h` —— 命令式 Flex/Grid 布局引擎（现成控件摆位置）。
  声明式节点树见 `Declarative.h`（`LayoutNode` + measure/layout 两趟，Android 模型，见
  `docs/declarative-layout.md`）。
- **框架**：`FrameworkCore.h`（文本编辑/状态机）、`Interaction.h`、`Observable.h`、
  `Animation.h`、`OverlayCore.h`、`PaintRecorder.h`（录制/金样）、`PaintBackend.h`（后端选择）、
  `UiText.h`、`WidgetControls.h`。

### 2.2 win32 层（Windows）
- **入口**：`AirUi.h` —— 汇总头（include 即可）。
- **窗口外壳**：`Shell.h` —— 无边框窗口 + 页面框架（消息循环、导航、主题切换）。
- **页面**：`Page.h` —— `IPage` 接口。
- **控件宿主**：`Widgets.h` / `Controls.h`（Win32 控件）、`Components.h`。
- **绘制**：`Gfx.h`（GDI 绘制）、`GdiPainter.h`（IPainter 的 GDI 实现）、`Canvas.h` /
  `CanvasPainter.h`（业务层适配）、`PaintFrame.h`（帧级路由）、`PaintProviders.h`
  （GDI 总是可用，Skia 插件可选）。
- **字体**：`Fonts.h`。**主题**：`ThemeManager.h` / `ThemeJson.h`。**浮层**：`OverlayWindow.h`。
- **可访问性**：`Accessibility.h`（部分为 TODO，见 §5）。

## 3. 稳定契约

### 3.1 渲染
- 所有绘制经 `IPainter`（或 `PaintFrame` 路由）；核心控件不直接依赖 GDI/Skia。
- 后端在运行时选择（`PaintBackend.h` + `PaintProviders`）：gdi 默认、skia 可选；不可用回退 gdi。
- 文字策略运行时探测（`text=backend` / `text=host`）。

### 3.2 平台隔离
- `airui-core` 零平台头，`check-core-clean` CI 门禁强制。
- 核心层只依赖 HAL 接口；Win32 是唯一后端（Skia 也是 Win32 上的可选绘制后端）。

### 3.3 版本
- 单一版本来源：`project(AireRecord VERSION ...)` → 生成 `Version.h`
  （`AIRUI_VERSION` / `AIRUI_VERSION_W`）。库与应用共用。

### 3.4 插件 ABI
- Skia 插件 C ABI v2（`AiruiPluginDesc{size, abi, caps, painterIfaceRev}`），跨 DLL 虚表布局
  显式校验。见 `docs/skia-backend.md`。

## 4. 库的使用

```cmake
# 在你的 CMake 中
add_subdirectory(airui)   # 或 target_link_libraries
target_link_libraries(app PRIVATE airui-core airui-win32 user32 gdi32 shell32 comctl32)
```

```cpp
#include "airui/AirUi.h"          // win32 汇总头
#include "airui/Shell.h"          // 窗口外壳
#include "airui/ControlsCore.h"   // 核心控件（走 IPainter）
```

- **MSVC 编译选项**：头文件注释为 UTF-8，消费方需加 `/source-charset:utf-8`
  （建议同时 `/execution-charset:utf-8`）；airui 自身 CMake 已内置。
- 所有 API 在 `namespace air::ui`。

- 应用：`Shell` 建窗 → `AddPage` 页面 → 页面 `OnPaint(Canvas&, client)` 里用 `CanvasPainter`
  画核心控件。
- 自定义后端：实现 `IPainter`，经 `PainterRegistry` 注册即可被 `PaintFrame` 路由使用。

## 5. 已知边界（稳定化关注点）

- **无障碍**：`Accessibility.h` 的 `SetAccessibleName/…` 为 TODO（真实 IAccessible 未实现），
  见 `docs/technology-stack.md` §9.5 与 `docs/ROADMAP.md`。
- **声明式布局（进行中）**：节点模型 + measure/layout 引擎已完成（`Declarative.h`，
  `docs/declarative-layout.md`）；JSON inflate、数据绑定、渲染桥仍在计划内。本版日常仍以
  过程式 API（代码建控件 + `Layout`）为主。
- **win32 是唯一后端**：Linux/macOS 仅有 `airui-core`（平台无关测试），无窗口壳。
- **Skia 跨 DLL**：`airui-skia.dll` 需与 exe 同目录且位数一致（架构感知安装器已处理）。

## 6. 稳定性保障

- CI 门禁：`check-core-clean`（core 平台隔离）、`check-skia-syntax`、`test_paint_recorder`
  结构金样、`clang-format`、clang-tidy（非阻塞）。
- `ctest -C Release` 23/23。
- 公共头应**自包含**（单独 include 可编译）——由 `check-public-headers` 门禁强制：
  configure 时对每个公共头生成仅含它的最小 TU 单独编译（`AIRUI_CHECK_PUBLIC_HEADERS`，
  默认 ON）；`cmake --build <build> --target check-public-headers` 可在 CI/本地强制重新
  校验。任一公共头依赖其它头先被 include 即为失败（库 bug），须在头内补齐 `#include` /
  前向声明。