# ADR-0001: 工具链矩阵（本地 VS2017 / CI VS2022 / Skia）

- **状态**：生效
- **日期**：2026-11-15
- **关联**：`build.bat`、`.github/workflows/ci.yml`、`docs/skia-backend.md`

## 背景

历史上 CI 与本地工具链不一致，README 与文档用过「唯一支持」这类措辞，且 `windows-latest` 的运行时行为会被 GitHub 迁移到新 VS 镜像而破坏，导致 CI 与文档描述的验证承担者错位（P1-3）。

## 决定

三条工具链并存，各自有明确职责：

- **本地官方构建**（`build.bat`）：CMake + **Visual Studio 2017**（x86），优先用 `"Visual Studio 15 2017"` 生成器，2019 / 2022 只作为找不到 2017 时的回退。产出 x86 GDI 版本（`airecord.exe` / `airecordd.exe` / 库）。
- **CI 主门禁**（`.github/workflows/ci.yml` 的 `windows` 与 `windows-x86` 作业）：**固定 pin 到 `runs-on: windows-2022`**，用 `"Visual Studio 17 2022"` 生成器分别以 `-A x64` 与 `-A Win32` 构建并跑 ctest。`windows-2022` 是当前仍带 VS2022 的镜像；**不要**用 `windows-latest`（2026 年 6 月起已迁移到 VS2026 镜像，"Visual Studio 17 2022" 生成器在它上面配置失败）。
- **Skia 后端**：`skia-windows` 作业同样是 VS2022 + vcpkg，**非阻塞**（`continue-on-error: true`）；Skia 是**运行时**后端选择，不是编译期开关，默认 GDI 产品不依赖它。

用词上放弃「唯一支持」这类措辞，改为描述「各路径用什么生成器与位数、为什么这么选」。

## 后果

- 收益：三套工具链职责清晰，CI 门禁可重复且不随 GitHub 镜像漂移；x86 / x64 两个位数的产物都可发布。
- 代价：需维护两套 CI 作业（x64 与 x86）；Skia 作业非阻塞意味着它失败不阻断合并，文档需明确标注（见 P1-3 拆分计划）。

## 被替代的旧决定

无（本 ADR 是把既有「本地 VS2017 + CI VS2022」事实与约束固定成文字，并取代 README / 文档中关于 CI 承担验证责任却未标注非阻塞的旧描述）。