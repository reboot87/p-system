# 构建约定（Build Conventions）

本文档收录本项目的**构建矩阵与工具链约定**。README 只给出使用速查，
完整规则（生成器、位数、为什么这样选）以本文档为准。

## 一句话约定

- **本地推荐**：Windows = CMake + Visual Studio 2017（MSVC），走 `build.bat`。
- **CI**：Windows 走 VS2022（`windows-2022` runner，固定 `-A x64` / `-A Win32`）。
- **Skia 路径**：VS2022 + vcpkg（`skia:x64-windows`）。
- **完整应用必须用 MSVC，禁止 MinGW / g++**；`g++` 仅用于平台无关核心的可选
  CI 冒烟测试（`scripts/check-core-linux-smoke.sh`），**不是**官方构建方式。

## 构建矩阵

| 路径 | 生成器 | 架构 | 说明 |
|---|---|---|---|
| 本地 `build.bat` | CMake + **VS2017**（`Visual Studio 15 2017`） | `-A x64`（`BUILD_DIR=build-cmake`） | 官方本地产物；`build.bat` 依次尝试 2017 → 2019 → 2022，任一 MSVC 可用即配置成功 |
| 本地手动（Windows） | CMake + **VS2017** | `-A x64` | `cmake -S . -B build-cmake -G "Visual Studio 15 2017" -A x64` |
| CI `windows` | **VS2022**（`windows-2022` runner，`Visual Studio 17 2022`） | `x64` | 全量构建 + `ctest -C Release` |
| CI `windows-x86` | **VS2022** | `Win32` | 覆盖官方 x86 架构全量 |
| CI `skia-windows` | **VS2022** + vcpkg `skia:x64-windows` | `x64` | 构建插件 DLL + 双后端一致性断言；**`continue-on-error` 非阻塞**，不阻断合并 |
| Linux / macOS | CMake + Unix Makefiles/Ninja | 平台无关 | **仅**构建 `airui-core`（`build.sh`），完整 exe 依赖 Win32/GDI/DWM |

## 打包 / 发布（NSIS）

Windows 安装包由 **NSIS**（Modern UI 2、管理员权限）生成。它是可选 / 尽力而为的流程，
不参与 `ALL` 构建，也不阻塞 `ctest`；仅当你显式调用时才会构建。

- **前提**：本机装有 NSIS，`makensis.exe` 在 `PATH` 上，或在标准安装路径
  （`C:\Program Files (x86)\NSIS\makensis.exe` / `C:\Program Files\NSIS\makensis.exe`）。
  也可用 `-Makensis <path>` 或环境变量 `MAKENSIS` 指定。
- **构建命令**（在仓库根目录）：
  ```
  powershell -File packaging\build-installer.ps1
  ```
  或直接调 makensis（脚本内部等价地以 `/DROOT=<仓库根目录>` 运行
  `packaging\airecord.nsi`）。
- **打包内容**：`build\Release\airecord.exe`、`build\Release\airecordd.exe`；
  若存在 Skia 插件包 `build\ci-all\airui-skia-windows-x64\*.dll` 则一并打入
  （否则应用运行时静默回退 GDI）。
- **安装行为**：安装到 `$PROGRAMFILES64\AireRecord`；通过 `airecordd -install`
  注册 `AireRecord` SCM 服务；创建开始菜单 / 桌面快捷方式；写出卸载程序与注册表项；
  卸载时 `airecordd -uninstall` 停止并移除该服务。
- **产物路径**：`build\installer\AireRecord-0.9.0-setup.exe`
  （`build\` 已被 gitignore，故产物保持未跟踪）。
- **说明**：这是标准的 NSIS Modern-UI 安装器；AirUI 框架不会被嵌入
  （NSIS 无法承载自绘框架），因此安装器使用 NSIS 自带的 UI。

## 门禁与位数

- **位数必须一致**：exe 与 `airui-skia.dll` 插件位数必须一致，否则静默回退 GDI。
  `build.bat` 固定 `-A x64`；CI Skia 插件为 x64；x86 请直接用 `p-system-windows-x86-gdi` 产物。
- **必跑门禁**：`scripts/check-core-clean.{sh,ps1}`、`scripts/check-core-linux-smoke.sh`、
  `scripts/check-skia-syntax.sh`（详见 `docs/remediation-plan.md` §6）。

## 环境坑

- 本机同时存在 `HTTPS_PROXY` / `https_proxy` 会让 MSBuild 报 `MSB6001`，
  configure 与 build 均需 `env -u https_proxy -u http_proxy -u all_proxy -u no_proxy`。
- GitHub `windows-latest` 已迁到 VS2026 镜像（无 VS2022），故 CI 固定 `windows-2022`。

> 规则若有歧义，禁止"猜测工具链"——以本文档（与 `README.md`）为准。