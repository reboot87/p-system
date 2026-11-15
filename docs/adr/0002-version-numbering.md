# ADR-0002: 版本号策略（`project(VERSION)` 单一来源）

- **状态**：生效
- **日期**：2026-11-15
- **关联**：`CMakeLists.txt`、`cmake/Version.h.in`、`res/app.rc`、`CHANGELOG.md`、`src/core/App.h`、`src/airui/platform/win32/Responsive.cpp`

## 背景

历史版本号散落多处且互相矛盾：`App.h` 页脚 `L"v1.3"`、`App.cpp` 页脚 `L"AireRecord v1.3.1 ..."`、`Responsive.cpp` 页脚 `L"AireRecord v1.0"`，而 git tag 却是 `v0.8`。`CMakeLists.txt` 的 `project(AireRecord)` 不带 VERSION，`res/app.rc` 无版本块。P1-2 要求在 UI 文案、资源、发布 tag 之间收敛到一处。

## 决定

- **单一来源**：`project(AireRecord VERSION 0.9.0)`。所有展示层版本都从这里派生，不再硬编码 `vX.Y`。
- **生成头**：`configure_file(cmake/Version.h.in ${CMAKE_BINARY_DIR}/generated/airui/Version.h @ONLY)` 产出 `Version.h`，暴露窄 / 宽宏：
  ```c
  #define AIRUI_VERSION   "0.9.0"
  #define AIRUI_VERSION_W L"0.9.0"
  ```
  所有用到版本的目标通过 `target_include_directories(... ${CMAKE_BINARY_DIR}/generated)` 可见该头（core 与 win32 均为 PUBLIC 传播）。
- **UI 文案唯一拼接点**：`App.cpp` 定义 `const wchar_t* kAppFooterPrefix = L"AireRecord " AIRUI_VERSION_W L" · ";`；三处页脚（服务已连接 / 未连接、服务不可用）都用它拼后缀，不再各自写死版本。`Responsive.cpp` 用同一宏 `L"AireRecord " AIRUI_VERSION_W`。`App::Version()` 返回 `AIRUI_VERSION_W`（保持既有 API 可用）。
- **资源块**：`res/app.rc` 增加 VERSIONINFO 块，`FileVersion` / `ProductVersion` 与 `project(VERSION)` 保持同步（当前手工同步；流水线注入是后续项）。
- **版本号序列**：git tag 采用工具箱序列 `v0.8` → `v0.9.0`。历史 UI 页脚 `v1.3` / `v1.3.1` 是 UI 层旧编号，与工具箱 tag 无关；本 ADR 起**退役**它们——页脚统一显示 `AireRecord v0.9.0`，旧的 `v1.x` 不再出现。

## 后果

- 收益：改版本只改 `project(VERSION)` 一处；UI 页脚、资源、tag、CHANGELOG 不再漂移；`git grep "v1\.3\|v1\.0" src/` 应为空。
- 代价：页脚文案从 `v1.3.1` 变为 `v0.9.0`，用户可见的版本号变化；`res/app.rc` 尚需手工与 CMake 同步（非单一来源，待流水线注入）。

## 被替代的旧决定

- `App.h` / `App.cpp` / `Responsive.cpp` 各自硬编码 `v1.3` / `v1.3.1` / `v1.0` 的旧做法被单一来源宏取代。
- `project(AireRecord)` 无 VERSION、`res/app.rc` 无版本块的旧状态被本 ADR 取代。