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