# AirUI — Windows 接入 Skia 后端（运行时动态选择）

> 决策记录 / 操作手册 · 2026-11-07
> 对应 `docs/cross-platform.md` §10 开放问题「Skia 引入方式」与 `docs/ROADMAP.md` A 类。
> 构建总约定见 `README.md`：本地正式构建**始终是 CMake + VS2017 + GDI**。

---

## 0. 结论（先看这段）

| 问题 | 定案 |
|---|---|
| **Skia 如何被选中** | **运行时动态选择，不是编译期选择**。`PainterRegistry`（core 层 Strategy + Registry）按 id 解析 `IPainter`；应用代码里没有任何 `#ifdef AIRUI_HAVE_SKIA` 决定绘制路径 |
| Skia 代码从哪来 | ① `airui-skia.dll` 运行时 `LoadLibrary` 加载（**默认构建也支持**，无需 SDK、无需重编）；② 有 SDK 的构建可把 provider 静态链入，注册同一个 id `"skia"` |
| `AIRUI_WITH_SKIA` 的语义 | **只决定本次构建是否产出 Skia 代码**（插件 DLL、静态 provider、真 SDK 测试），**不决定运行时用哪个后端**。默认 OFF 时应用仍带动态加载器 |
| 选择入口 | `ShellOptions.paintBackend` / `--paint-backend=<id>` / 环境变量 `AIRUI_PAINT_BACKEND` / `Shell::SetPaintBackend()`（热切换） |
| 不可用时 | 自动回退 `"gdi"`，`PainterRegistry::LastError()` 记录原因；默认后端仍是 gdi，富渲染零回归 |
| Skia 引入方式 | **首选 vcpkg `skia` port**；**`AIRUI_SKIA_ROOT` 预编译 release 为备选**；**不采用 git 子模块源码编译** |
| Windows 渲染入口 | `coreShell` + 运行时选择 → `SkiaFrame` → 32bpp BGRA 光栅画布 → `AlphaBlend` 合成到 HDC |
| 本地工具链约束 | 官方构建仍是 **CMake + VS2017**；Skia 代码推荐在 VS2022 环境或 CI 中构建。本机实测 `vswhere` 有 **VS2022 Professional 17.14（`C:\2022`）** 与 VS2017（`C:\2017\Community`）并存 |
| 验证方式 | 无 SDK 的注册表/回退测试 + 语法契约门禁 + 三平台纯映射单测 + 本机 VS2017 全量构建/ctest；真 SDK 的编译/链接/**动态加载**由 push/PR 自动触发的 GitHub Actions 承担 |

**为什么要改成动态选择**：原方案把 `AIRUI_WITH_SKIA` 当作「是否使用 Skia」的编译开关，`Shell.cpp` 靠 `#ifdef AIRUI_HAVE_SKIA` 把 Skia 分支整个剔除——想用 Skia 必须带 SDK 重新编译整个 app，且 VS2017 官方产物永远无法用 Skia。改成运行时注册表 + 插件 DLL 后，SDK 只影响「是否构建插件」，**后端选择发生在进程运行时**，把 DLL 放到 exe 旁边即可启用，部署与工具链彻底解耦。

> 本机补装 vcpkg 的结论（2026-11-07）：`vcpkg install skia:x64-windows` 的内置下载器在本机代理下报 `curl error 35 (SSL connect error)`，无法拉取工具包/Skia 源码。按既定策略「不好编译就交给 GitHub Actions」，真 SDK 一环由自动触发的 CI 承担；本机只用 VS2017/VS2022 做无需 SDK 的编译验证。

---

## 1. 运行时选择（本文件的重点）

### 1.1 配置优先级

```
ShellOptions.paintBackend  ← 代码 / --paint-backend=<id>（main.cpp 解析）
        ↓ 为空时
AIRUI_PAINT_BACKEND        ← 环境变量
        ↓ 仍为 auto 时
ShellOptions.skiaChrome    ← 旧字段兼容别名（true → "skia"）
        ↓ 都没设置
"auto" → PainterRegistry 默认 → "gdi"
```

`ResolvePaintBackend()`（core 纯函数）实现该优先级；`NormalizeBackend()` 负责大小写/空白归一。可选值：

| 值 | 行为 |
|---|---|
| `gdi` | 始终可用，默认 |
| `skia` | 静态 provider 或 `airui-skia.dll`；不可用自动回退 gdi |
| `auto` | 取注册表默认（默认 gdi，可用 `SetDefault` 改） |
| 其他已注册 id | 由 provider 决定；未注册时回退 gdi 并写 `LastError()` |

`Shell::SetPaintBackend("skia")` 是**热切换**：改字段后 `Invalidate()`，下一帧就换后端，不需要重建窗口、更不需要重新编译。

### 1.2 注册表与 provider

- `src/airui/core/include/airui/PaintBackend.h` + `core/src/PaintBackend.cpp`：平台无关的 `PaintTarget`（不透明 surface 指针 + Rect）、`IPainterProvider`、`PainterRegistry`。core 不认识任何平台头。
- `src/airui/platform/win32/PaintProviders.cpp`：注册 `gdi`（总是）与 `skia`：
  - 有 SDK（`AIRUI_HAVE_SKIA`）：注册静态 provider，直接调 `CreateSkiaBackend()`；
  - 无 SDK：注册插件 provider，第一次 `Available()` 时 `LoadLibraryW(L"airui-skia.dll")` 并解析稳定 C ABI。
  - `RegisterSkiaPluginProvider()` 可强制切到 DLL 路径（测试/显式 opt-in 用），`SkiaPluginLoaded()` / `SkiaPluginReload()` 用于诊断。
- 两种来源注册**同一个 id `"skia"`**，应用侧代码完全一致；`Register()` 按 id 替换，所以插件可覆盖内置 provider。

### 1.3 插件 ABI（跨模块边界）

`src/airui/platform/skia/airui/SkiaPluginAbi.h` 是主程序与 DLL 唯一共享的头，**零 Skia 头、零 STL 容器、零 C++ 类**，只有 C 函数指针形状与 `AIRUI_SKIA_PLUGIN_ABI` 版本号。

| 导出符号 | 作用 |
|---|---|
| `airui_skia_abi()` | 返回 ABI 版本；不匹配则拒绝加载，避免崩溃 |
| `airui_skia_create(surface,l,t,r,b)` | 建帧，返回不透明 `IPainter*` 句柄；`surface=nullptr` 为离屏帧 |
| `airui_skia_destroy(handle)` | **在 DLL 内** `delete`，触发虚析构 → `SkiaFrame::Present()` 上屏 |
| `airui_skia_pixels/row_bytes` | 离屏光栅像素，供测试校验 |

主程序侧的 `PluginPainter` 把 13 个 `IPainter` 方法转发给 DLL 内的对象，析构回调 `airui_skia_destroy`——**对象所有权不跨 CRT / 模块边界**。

> **ABI 约束**：`IPainter` 的虚方法含 `const std::wstring&`，跨 DLL 依赖 MSVC C++ ABI 兼容（v140–v143 相同内存布局，需一致的 `/MD`、一致的 `_ITERATOR_DEBUG_LEVEL`）。CI 中 app 与 DLL 由同一 VS2022 构建；若用其它工具集编译 DLL，请同工具集重建。DLL 路径可用 `AIRUI_SKIA_DLL` 环境变量覆盖。

### 1.4 应用接线（`src/airui/platform/win32/Shell.cpp`）

```cpp
// 没有任何 #ifdef：选择发生在运行时
PaintTarget target{ static_cast<void*>(dc), Rect{ rc.left, rc.top, rc.right, rc.bottom } };
std::unique_ptr<IPainter> painter =
    PainterRegistry::Instance().Begin(m_paintBackend, target);
if (painter) m_shellCore.Paint(*painter, Pal(), Mx());   // 析构时 Skia 上屏
else        { GdiPainter fallback(dc); m_shellCore.Paint(fallback, Pal(), Mx()); }
```

`AIRUI_HAVE_SKIA` 现在**只**决定 `PaintProviders.cpp` 注册静态还是插件 provider，不出现在绘制路径里。`ShellOptions.skiaChrome` 保留为兼容别名（默认 false）；新代码用 `paintBackend`。`coreShell` 仍是 HAL 外壳的开关（默认关，富渲染零回归）。

---

## 2. 三种 SDK 引入方式的评估

| 方式 | 成本 | CI 友好度 | 结论 |
|---|---|---|---|
| **vcpkg `skia` port** | `vcpkg install skia:x64-windows`，有预编译二进制缓存时几分钟；导出 `unofficial-skia` CMake config 与 `unofficial::skia::skia` target | 高，GitHub Windows runner 自带 vcpkg | ✅ **首选** |
| **预编译 release / 手动 prefix** | 需自行放置头文件与 `.lib`，路径随发行包变化 | 中，需自己上传/缓存 | ✅ **备选**，通过 `-DAIRUI_SKIA_ROOT=<prefix>` |
| **git 子模块 + 源码编译** | 需要 `depot_tools`、`git-sync-deps`、GN、Ninja、Python，Windows 上构建耗时最长且最易失败（设计稿 R5 明确列为核心风险） | 低，每次 CI 都要从头编译 | ❌ **本期不采用** |

CMake 同时支持前两种，互不排斥：先 `find_package(unofficial-skia CONFIG QUIET)`，未命中再回退到 `AIRUI_SKIA_ROOT` 的 `find_path` / `find_library`。所以「预编译」与「源码编译产物」都能通过 `AIRUI_SKIA_ROOT` 接进来，将来若真的改成子模块构建，只需把产物指向该目录，**无需改产品代码**。

---

## 3. 构建 Skia 插件（Windows / VS2022 环境）

```powershell
vcpkg install skia:x64-windows

cmake -S . -B build-skia -G "Visual Studio 17 2022" -A x64 `
  -DAIRUI_WITH_SKIA=ON "-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake"

cmake --build build-skia --config Release --target airui-skia-plugin
cmake --build build-skia --config Release --target test_skia_plugin
cmake --build build-skia --config Release --target test_skia_backend
.\build-skia\Release\test_skia_plugin.exe     # 运行时 DLL 选择 + C ABI + 光栅
.\build-skia\Release\test_skia_backend.exe    # G5 双后端序列断言
```

产物 `build-skia\Release\airui-skia.dll`（导入库 `airui-skia-import.lib`，避免与静态 `airui-skia.lib` 同名冲突）。把它放到 `airecord.exe` 同目录即可让**默认 VS2017 产物**直接使用：

```bat
airecord.exe --paint-backend=skia
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

两者都没找到时，配置阶段会 **FATAL_ERROR 并打印上述两条命令**；但这只影响「构建插件」，**不会**让默认 GDI 构建失败，也不会改变运行时选择能力。

> ⚠️ 在 VS2017 下编译 Skia 代码**未经验证**（vcpkg 不提供 v141 triplet 保证）。本地正式产物请保持 `AIRUI_WITH_SKIA=OFF`（默认）与 `build.bat`；插件由 VS2022 / CI 产出。CI 与本地工具链允许不一致（已确认）。

---

## 4. 代码结构

```
src/airui/core/
├─ include/airui/PaintBackend.h   运行时注册表 / PaintTarget / 解析函数（平台无关）
└─ src/PaintBackend.cpp           Begin 解析 + gdi 回退 + LastError

src/airui/platform/win32/
├─ airui/PaintProviders.h/.cpp    gdi / 静态 skia / DLL skia 三种 provider + 插件加载器
└─ Shell.cpp                       纯运行时选择，无 #ifdef

src/airui/platform/skia/airui/
├─ SkiaTypes.h/.cpp    纯 C++ 值映射：Color→SkColor、FontRole→字号、
│                      wchar_t→UTF-8（UTF-16/UTF-32 均支持）、dt::*→对齐。
│                      零 Skia 依赖、零平台头，三平台可单测。
├─ SkiaApi.h           Skia 头的唯一入口，隔离 Skia 版本漂移（R5）。
│                      AIRUI_SKIA_MOCK 时切到 API 契约 mock。
├─ SkiaPainter.h/.cpp  IPainter → SkCanvas 的完整实现（13 个方法全实现），
│                      pimpl，头文件不含任何 Skia 头。
├─ SkiaSurface.h/.cpp  SkiaFrame：raster 画布 + Windows HDC 桥 + CreateSkiaBackend()。
├─ SkiaPluginAbi.h     跨模块稳定 C ABI（主程序与 DLL 共享，零 Skia 依赖）
└─ SkiaPlugin.cpp      airui-skia.dll 的导出实现（create/destroy/pixels/row_bytes）
```

**上屏路径**：`SkImageInfo(kBGRA_8888, kPremul)` → `SkBitmap::allocPixels` → `SkCanvas(bitmap)` → 绘制 → 逐行拷进 `CreateDIBSection` 创建的 top-down 32bpp DIB → `AlphaBlend(AC_SRC_ALPHA)` 合成到 HDC。Skia 的 premultiplied BGRA 与 Win32 DIB 布局天然一致；透明像素不改动下方 GDI 内容，所以 Skia 外壳与旧 GDI 页面可以并存。

---

## 5. 验证矩阵

| 层 | 手段 | 需要 SDK | 状态 |
|---|---|---|---|
| 值映射 | `test_skia_types`（ctest，三平台） | ❌ | 通过；本机 g++ `-Werror` 通过 |
| Skia 实现的语法/接口形状 | `scripts/check-skia-syntax.sh`（`-fsyntax-only` + mock 头） | ❌ | 通过；已接入 ubuntu/macOS CI |
| core 纯净性（新增注册表） | `check-core-clean.ps1` / `check-core-linux-smoke.sh` | ❌ | 48 个 core 文件 0 平台引用；g++ `-Werror` 全绿 |
| **运行时选择 / 热切换 / gdi 回退** | `test_paint_backend`（ctest，三平台，假 provider） | ❌ | 通过；VS2017 + Linux smoke + CMake/ctest 均跑 |
| **默认构建的动态加载路径** | `test_paint_runtime`（ctest，Windows，无 SDK） | ❌ | 通过：`skia backend not deployed (fallback path under test)`，请求 skia 仍产出 painter 并回退 gdi |
| 插件源 / 两种 provider 分支的 MSVC 语法 | VS2017 OBJECT-library 编译门禁（本机临时工程） | ❌ | `SkiaPlugin.cpp` / `PaintProviders.cpp`（含 `AIRUI_HAVE_SKIA=1` 分支）/ `test_skia_plugin.cpp` 全部编译通过 |
| `AIRUI_WITH_SKIA=ON` 无 SDK | CMake configure | ❌ | 明确报错并给出两条安装命令 |
| 真实编译 / 链接 / **动态加载** + C ABI + 光栅像素 | `airui-skia.dll` + `test_skia_plugin` | ✅ | **已实跑通过**（run `42337859169`）：CI 产出 `airui-skia.dll`，跨模块虚调用 + C ABI 光栅像素全绿，日志 `test_skia_plugin: runtime DLL backend + C ABI + raster OK` |
| 真实编译 / 链接 / 运行 + **G5 双后端序列断言** | `test_skia_backend` | ✅ | **已实跑通过**：`G5 parity + raster OK`（run `42335700042`） |
| 默认工具链零回归 | VS2017 全量 `cmake --build` + `ctest -C Release` | ❌ | **19/19 通过**（新增 2 个用例） |

`test_skia_backend` 是 G5 的落地：同一场景先由 core 的 `PaintRecorder` 录成金样，再经 `TeePainter` 一边驱动真实 Skia 后端、一边记录后端**实际收到**的 `IPainter` 调用，最后断言 `SequenceEquals` / `Diff().empty()`，并校验光栅像素确为 `BGRA + premultiplied`。

GitHub Actions 的 `skia-windows` 作业用 **VS2022 + vcpkg** 跑上述真 SDK 两环，**push / PR 到 `main` 时自动触发**。vcpkg 的 `skia` port 是 `vcpkg_from_github` + GN/Ninja **源码编译**、不下载预编译库，所以作业用 `VCPKG_BINARY_SOURCES` 的 files 缓存 + `actions/cache` 的 `restore`/`save` 拆分缓存（install 成功后立即保存）缓存 `installed`/`downloads`：首次付编译成本，后续直接还原（约 2 分钟）。另有独立并发组、`timeout-minutes: 120`，并保留 `continue-on-error` 不阻塞核心 CI。

> CI 的两个 Windows 作业固定 `runs-on: windows-2022`：`windows-latest` 自 2026-06 起指向 VS2026 镜像（无 VS2022，且自带 CMake < 4.2 不支持 VS18 生成器），硬写 `Visual Studio 17 2022` 会在 Configure 阶段直接失败。

### 5.1 真 SDK 首次编译实测（2026-11-07，vcpkg `skia:x64-windows@148`）

CI 首次跑完 vcpkg 源码编译（约 35 分钟）并成功 Configure，Build 阶段暴露两类问题，都不是业务逻辑错误：

| 现象 | 根因 | 修法 |
|---|---|---|
| Skia 自带头 `SkRect.h` / `SkSamplingOptions.h` / `SkTArray.h` 报 MSVC `C2589` | `SkiaSurface.h` 引入 `<windows.h>`，`min/max` 宏把 Skia 头里的 `std::numeric_limits<>::max()` / `std::max(...)` 展开成非法表达式 | `SkiaApi.h`（唯一 Skia 头入口）在 `_WIN32 && !AIRUI_SKIA_MOCK` 下定义 `NOMINMAX` 并 `#undef min/max` |
| `SkTypeface::MakeFromName` 不是成员 | 现代 Skia 移除该工厂，字体查找改走 `SkFontMgr` | 后端改用 `SkFont` 的平台默认 typeface，仅按 `FontRole` 设字号 |
| `SkPath::addRect` 不是成员 | `SkPath` 已不可变，轮廓改由 `SkPathBuilder` 构造 | `SkPathBuilder builder; builder.addRect(...); SkPath path = builder.detach();` |
| `SkDashPathEffect::Make` 不接 3 参数 | 新签名 `Make(SkSpan<const SkScalar> intervals, SkScalar phase)` | 传 `SkSpan<const SkScalar>(intervals, 2), 0` |
| `SkiaSurface.cpp` 报 `C2171` | 新版 `SkBitmap::allocPixels` 返回 `void` | 调用后用 `bitmap.getPixels()` 判空；mock 同步改成 `void` |

配套改动：`SkiaApi.h` 增加 `SkPathBuilder.h` / `SkSpan.h`；`tests/skia_contract/SkiaApiMock.h` 同步成新契约（否则无 SDK 的语法门禁会一直验证一套已死的 API）；CI 增加 `Record real Skia API signatures` 诊断步骤。

> **约定**：`SkiaApi.h`（真 SDK 入口）与 `SkiaApiMock.h`（契约门禁）必须**同改**。真 SDK 的权威结论以 `skia-windows` 的 Build + 测试运行为准，本地 mock 只保证形状。

---

## 6. 已知边界（不要误当成已完成）

- **没有 Skia 版 `IWindow`**：目前只有绘制后端。窗口壳、消息泵、事件注入仍由 `Win32Window` 负责；Linux/macOS 应用本体仍不可构建。
- **插件 ABI 依赖 MSVC 兼容工具集**：`IPainter` 虚方法带 `std::wstring`，跨 DLL 要求同代 MSVC ABI 与一致的 CRT 选项（§1.3）。尚未做纯 C 绘制桥，也未做跨编译器验证。
- **字体度量未对齐**：后端用 `SkFont` 的平台默认 typeface，与 GDI 的 "Segoe UI" 族、CJK 回退、hinting 的差异尚未校准（设计稿 R2/R7）。
- **像素级 GDI vs Skia 截图 diff 未做**：当前断言的是**调用序列一致性**与「Skia 确实产出像素」，不是 SSIM 截图比对。
- **VS2017 + Skia 代码未验证**：真 SDK 路径只在 VS2022 CI 上验证；官方 VS2017 产物通过 DLL 使用 Skia 的端到端效果尚未在真机截图确认。
- **Skia API 版本漂移**：所有 Skia 头集中在 `SkiaApi.h`；已在 vcpkg `skia@148` 上校准（§5.1），但 `drawSimpleText` / `measureText` 等仍可能随版本变化。

---

## 7. 下一步

1. ~~观察 `test_skia_plugin` 首次真 SDK 结果~~ 已完成：run `42337859169` 四个作业全绿，插件 DLL 与动态加载测试**首跑即通过**，无需修正 ABI / 导出名；后续每次 push/PR 自动复验。
2. 校准字体度量与 CJK 回退，补 GDI vs Skia 截图阈值。
3. Phase 2 剩余：Skia 版 `IWindow` / Linux 窗口壳（`cross-platform.md` §10 仍待定）。
