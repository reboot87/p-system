# AirUI — Windows 接入 Skia 后端

> 决策记录 / 操作手册 · 2026-11-07
> 对应 `docs/cross-platform.md` §10 开放问题「Skia 引入方式」与 `docs/ROADMAP.md` A 类。
> 构建总约定见 `README.md`：本地正式构建**始终是 CMake + VS2017 + GDI**。

---

## 0. 结论（先看这段）

| 问题 | 定案 |
|---|---|
| Skia 引入方式 | **首选 vcpkg `skia` port**；**`AIRUI_SKIA_ROOT` 预编译 release 为备选**；**不采用 git 子模块源码编译** |
| 是否进入默认构建 | **否**。`AIRUI_WITH_SKIA` 默认 `OFF`，默认依赖图完全不引用 Skia |
| Windows 上的渲染入口 | `ShellOptions.coreShell + skiaChrome` → `SkiaFrame` → 32bpp BGRA 光栅画布 → `AlphaBlend` 合成到 HDC |
| 本地工具链约束 | 本地官方构建仍是 **CMake + VS2017**；Skia 是 opt-in 第二后端，**推荐在 VS2022 环境开启** |
| 验证方式 | 无 SDK 的语法契约门禁 + 三平台纯映射单测；真 SDK 的编译/链接/双后端序列断言交给 GitHub Actions |

**为什么要 opt-in**：本机没有 Skia SDK（无 vcpkg / depot_tools），而 vcpkg 的 `skia` port 官方 CI 使用 VS2022 及更新工具集，VS2017（MSVC 19.16）不在其支持矩阵内。把 Skia 塞进默认构建既违反「Windows 必须 CMake+VS2017」，也会让每个没有 SDK 的人配置失败。因此做成**显式开关 + 清晰报错**。

---

## 1. 三种引入方式的评估

| 方式 | 成本 | CI 友好度 | 结论 |
|---|---|---|---|
| **vcpkg `skia` port** | `vcpkg install skia:x64-windows`，有预编译二进制缓存时几分钟；导出 `unofficial-skia` CMake config 与 `unofficial::skia::skia` target | 高，GitHub Windows runner 自带 vcpkg | ✅ **首选** |
| **预编译 release / 手动 prefix** | 需自行放置头文件与 `.lib`，路径随发行包变化 | 中，需自己上传/缓存 | ✅ **备选**，通过 `-DAIRUI_SKIA_ROOT=<prefix>` |
| **git 子模块 + 源码编译** | 需要 `depot_tools`、`git-sync-deps`、GN、Ninja、Python，Windows 上构建耗时最长且最易失败（设计稿 R5 明确列为核心风险） | 低，每次 CI 都要从头编译 | ❌ **本期不采用** |

CMake 同时支持前两种，互不排斥：先 `find_package(unofficial-skia CONFIG QUIET)`，未命中再回退到 `AIRUI_SKIA_ROOT` 的 `find_path` / `find_library`。所以「预编译」与「源码编译产物」都能通过 `AIRUI_SKIA_ROOT` 接进来，将来若真的改成子模块构建，只需把产物指向该目录，**无需改产品代码**。

---

## 2. 开启步骤（Windows / VS2022 环境）

```powershell
vcpkg install skia:x64-windows

cmake -S . -B build-skia -G "Visual Studio 17 2022" -A x64 `
  -DAIRUI_WITH_SKIA=ON `
  "-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake"

cmake --build build-skia --config Release --target airui-skia
cmake --build build-skia --config Release --target test_skia_backend
.\build-skia\Release\test_skia_backend.exe
```

手动 prefix（预编译 release 或自建产物）布局要求：

```
<prefix>/include/skia/include/core/SkCanvas.h   # SkiaApi.h 使用 <include/core/...> 形式
<prefix>/lib/skia.lib
```
```powershell
cmake -S . -B build-skia -G "Visual Studio 17 2022" -A x64 `
  -DAIRUI_WITH_SKIA=ON -DAIRUI_SKIA_ROOT=<prefix>
```

两者都没找到时，配置阶段会 **FATAL_ERROR 并打印上述两条命令**，不会静默降级成 GDI。

> ⚠️ 在 VS2017 下开启**未经验证**：Skia 较新版本的头文件可能要求更新的 MSVC，且 vcpkg 不提供 v141 triplet 的保证。本地正式产物请保持 `AIRUI_WITH_SKIA=OFF`（默认）与 `build.bat`。CI 与本地工具链允许不一致（已确认）。

---

## 3. 代码结构

```
src/airui/platform/skia/airui/
├─ SkiaTypes.h/.cpp    纯 C++ 值映射：Color→SkColor、FontRole→字号、
│                      wchar_t→UTF-8（UTF-16/UTF-32 均支持）、dt::*→对齐。
│                      零 Skia 依赖、零平台头，三平台可单测。
├─ SkiaApi.h           Skia 头的唯一入口，隔离 Skia 版本漂移（R5）。
│                      AIRUI_SKIA_MOCK 时切到 API 契约 mock。
├─ SkiaPainter.h/.cpp  IPainter → SkCanvas 的完整实现（13 个方法全实现）：
│                      圆角 / 虚线 / alpha / FontRole 字体 / 裁剪栈 / 累加平移。
│                      头文件用 pimpl，不含任何 Skia 头。
└─ SkiaSurface.h/.cpp  SkiaFrame：raster 画布 + Windows HDC 桥 + 工厂 CreateSkiaBackend()。
```

**上屏路径**：`SkImageInfo(kBGRA_8888, kPremul)` → `SkBitmap::allocPixels` → `SkCanvas(bitmap)` → 绘制 → 逐行拷进 `CreateDIBSection` 创建的 top-down 32bpp DIB → `AlphaBlend(AC_SRC_ALPHA)` 合成到 HDC。Skia 的 premultiplied BGRA 与 Win32 DIB 布局天然一致；透明像素不改动下方 GDI 内容，所以 Skia 外壳与旧 GDI 页面可以并存。

**应用接线**（`src/airui/platform/win32/Shell.cpp`）：

```cpp
#ifdef AIRUI_HAVE_SKIA
    if (m_opt.skiaChrome) {
        SkiaFrame frame(dc, Rect{...});
        m_shellCore.Paint(frame, Pal(), Mx());   // 析构时自动 Present
    } else
#endif
    { GdiPainter painter(dc); m_shellCore.Paint(painter, Pal(), Mx()); }
```

`AIRUI_HAVE_SKIA` 只在 `AIRUI_WITH_SKIA=ON` 时由 `airui-skia` 传播；默认构建里这段被预处理器剔除。`ShellOptions.skiaChrome` 默认 `false`，`coreShell` 也是默认关的——**两层默认都关，富渲染零回归**。

---

## 4. 验证矩阵

| 层 | 手段 | 需要 SDK | 状态 |
|---|---|---|---|
| 值映射 | `test_skia_types`（ctest，三平台） | ❌ | 本机 g++ `-Werror` 通过；ctest 17/17 |
| Skia 实现的语法/接口形状 | `scripts/check-skia-syntax.sh`（`-fsyntax-only` + mock 头） | ❌ | 本机通过；已接入 ubuntu/macOS CI |
| option 语义 | `cmake -DAIRUI_WITH_SKIA=ON`（无 SDK） | ❌ | 明确报错并给出两条安装命令 |
| `Shell.cpp` 的 Skia 分支（MSVC 语法/接线） | 独立以 `-DAIRUI_HAVE_SKIA=1` 编译 `airui-win32` | ❌ | VS2017 实测 EXIT=0，`Shell.obj` + `airui-win32.lib` 产出，无 error |
| 真实编译 / 链接 / 运行 + **G5 双后端序列断言** | `test_skia_backend` | ✅ | 由 `skia-windows` CI 作业承担 |
| 默认工具链零回归 | VS2017 `cmake --build` + `ctest` + `build.bat` | ❌ | 全绿 |

`test_skia_backend` 是 G5 的落地：同一场景先由 core 的 `PaintRecorder` 录成金样，再经 `TeePainter` 一边驱动真实 Skia 后端、一边记录后端**实际收到**的 `IPainter` 调用，最后断言 `SequenceEquals` / `Diff().empty()`，并校验光栅像素确为 `BGRA + premultiplied`。这是第二个后端出现前根本无法执行的断言。

GitHub Actions 的 `skia-windows` 作业用 **VS2022 + vcpkg** 跑上述最后一环，通过 `workflow_dispatch` 手动触发（避免每次 push 都拉/编 Skia），并设 `continue-on-error` 不阻塞主 CI。

---

## 5. 已知边界（不要误当成已完成）

- **没有 Skia 版 `IWindow`**：目前只有绘制后端。窗口壳、消息泵、事件注入仍由 `Win32Window` 负责；Linux/macOS 应用本体仍不可构建。
- **字体度量未对齐**：`SkFont` 用 `MakeFromName("Segoe UI")`，CJK 回退、hinting 与 GDI 的差异尚未校准（设计稿 R2/R7）。
- **像素级 GDI vs Skia 截图 diff 未做**：当前断言的是**调用序列一致性**与「Skia 确实产出像素」，不是 SSIM 截图比对（§6 计划项）。
- **VS2017 + Skia 未验证**：真 SDK 路径只在 VS2022 CI 上验证。
- **Skia API 版本漂移**：所有 Skia 头集中在 `SkiaApi.h`；若新版改了 `drawSimpleText` / `measureText` 等签名，改这一个文件即可。`SkiaApiMock.h` 只保证形状，不是实现，也不是渲染测试。

---

## 6. 下一步

1. 在 CI 手动跑一次 `skia-windows`，按真实 SDK 的报错修正 `SkiaApi.h` 的 API 适配。
2. 校准字体度量与 CJK 回退，补 GDI vs Skia 截图阈值。
3. Phase 2 剩余：Skia 版 `IWindow` / Linux 窗口壳（`cross-platform.md` §10 仍待定）。
