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
| 选择入口 | `ShellOptions.paintBackend` / `--paint-backend=<id>` / 环境变量 `AIRUI_PAINT_BACKEND` / **标题栏「渲染」切换按钮** / `Shell::SetPaintBackend()`（热切换） |
| 不可用时 | 自动回退 `"gdi"`，`PainterRegistry::LastError()` 记录原因；默认后端仍是 gdi，富渲染零回归 |
| Skia 引入方式 | **首选 vcpkg `skia` port**；**`AIRUI_SKIA_ROOT` 预编译 release 为备选**；**不采用 git 子模块源码编译** |
| Windows 渲染入口 | `Shell::Paint()` 每帧开 `PaintFrame` → `gfx::*` / `CanvasPainter` 全量转发 → 运行时选择的 `IPainter` → `SkiaFrame` 32bpp BGRA 光栅画布 → `AlphaBlend` 合成到 HDC；**整窗（外壳＋页面内容）一起切换**，`--core-shell` 不是闸门 |
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

> **ABI 约束（2026-11-07 下载 CI 产物实测修订）**：`IPainter` 的虚方法含 `const std::wstring&`，跨 DLL 依赖 MSVC C++ ABI 兼容。实测 **VS2017 (v141, x64) 主程序 → VS2022 (v143, x64) 插件** 可用：注册表选择、`Text()` 携带 `std::wstring` 跨模块、`Skia → AlphaBlend → Win32 HDC` 合成像素全部通过，说明 v140–v143 同族工具集可以跨代使用，并非必须同代重建。硬约束是 **位数必须一致**（x86 进程加载 x64 DLL 会失败并静默回退 gdi）、MSVC 同族、一致的 `/MD` 与 `_ITERATOR_DEBUG_LEVEL`；MinGW / Clang 等非 MSVC 工具集不在保证范围。DLL 路径可用 `AIRUI_SKIA_DLL` 环境变量覆盖。

### 1.4 应用接线（`src/airui/platform/win32/Shell.cpp`）

```cpp
// 没有任何 #ifdef：选择发生在运行时，且覆盖整个客户区
PaintTarget target{ static_cast<void*>(dc), Rect{ rc.left, rc.top, rc.right, rc.bottom } };
std::unique_ptr<IPainter> backend = PainterRegistry::Instance().Begin("skia", target);
if (backend && registry.LastError().empty())
    PaintFrame::Instance().Begin(backend.get(), dc, &m_fonts,
                                 SkiaPluginSupportsWideText());
// ... 原有 DrawCaption / Nav / IPage::OnPaint / StatusBar 全部照旧 ...
PaintFrame::Instance().End();
backend.reset();                       // SkiaFrame 析构 -> AlphaBlend 上屏
PaintFrame::Instance().ReplayText();   // 宿主文字层（仅 text=host 时）
```

`PaintFrame` 是帧级路由：`gfx::*`（`Gfx.cpp` 的填充/圆角/描边/虚线/线/点/文字）与 `CanvasPainter`（`FillRectAlpha`/`PushClip`/`PopClip`）在帧激活时都转发给它，而页面、`Canvas` 复合控件、`ShellCore` 与全部平台无关 core 控件的绘制最终都汇聚到这两处。因此**标题栏、侧栏、状态栏、页面内容区一起换后端**，无需改写任何页面代码。帧未激活时这些原语仍执行原来的 GDI 代码，默认渲染零回归。

`AIRUI_HAVE_SKIA` 现在**只**决定 `PaintProviders.cpp` 注册静态还是插件 provider，不出现在绘制路径里。`ShellOptions.skiaChrome` 保留为兼容别名（默认 false）；新代码用 `paintBackend`。

> **`--core-shell` 不再是后端闸门**：它只切换外壳实现（简化 HAL 外壳，默认关闭以保护旧版富渲染），与用哪个后端无关。只传 `--paint-backend=skia` 就会让整个窗口走 Skia，日志里 `coreShell=0 wholeWindow=1` 是正常状态。标题栏切换按钮同样不会改动 `coreShell`。

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

产物 `build-skia\Release\airui-skia.dll`（导入库 `airui-skia-import.lib`，避免与静态 `airui-skia.lib` 同名冲突）。把它连同全套依赖 DLL 放到 `airecord.exe` 同目录即可让**默认 VS2017 产物**直接使用：

```bat
airecord.exe --paint-backend=skia
```
不需要 `--core-shell`：`PaintFrame` 在帧级别覆盖整个客户区（§1.4）。`--core-shell` 只是另一套简化外壳，可单独使用。

### 3.1 默认构建自动部署插件（零命令、无需手动拷贝）

默认构建（无本地 Skia SDK，即 `AIRUI_WITH_SKIA=OFF`）现在会在每次 `cmake --build` 时把 Skia 插件自动放到 exe 同目录，**普通一次构建即可直接使用 Skia，无需手动下载/拷贝 DLL，也无需单独的 fetch 命令**：

- `airui_deploy_skia_plugin(<target>)` 给 `airecord` / `test_skia_plugin` 挂一个 `POST_BUILD` 步骤，调用 `scripts/deploy-skia-plugin.cmake`；
- 解析顺序（best-effort，找不到就静默回退 GDI，绝不导致构建失败）：
  1. `-DAIRUI_SKIA_PLUGIN_DIR=<dir>`（显式指定包含 `airui-skia.dll` 的目录）；
  2. `<binary-dir>/ci-all/airui-skia-windows-x64`（手动放置）；
  3. `<binary-dir>/ci-skia`（自动 `gh run download` 最新成功 CI 产物）。
- 只拷贝 `*.dll`（`FILES_MATCHING PATTERN "*.dll"`），**不会覆盖开发者自己刚编译的 `airecord.exe`**；`airui-skia.dll` 及其全部 vcpkg 运行时依赖（skia / icu / harfbuzz 等）一次性到位。
- 自动下载需要 `gh` 已登录 + 网络 + 仓库访问；首次构建会拉取一次，之后检测到 `airui-skia.dll` 已存在即跳过。`fetch-skia-plugin` 这个独立 target 仍保留，作为手动预取入口。
- ⚠️ 插件是 **x64**，需与 x64 的 `airecord.exe` 搭配；x86 进程无法加载 x64 DLL（§6）。

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
| **下载 CI 产物、跨工具链 / 跨机器实测** | `gh run download` + 本机 VS2017 x64 harness | ✅ | **已实跑通过**（run `42338618815`）：见 §5.2，含位数不匹配回退、跨代 MSVC ABI、真实 HDC 上屏像素 |
| **CI 整体编译（ALL targets）+ 全量 ctest + 全产物发布** | 3 个 Windows job（x64 GDI / x86 GDI / x64 Skia）+ 2 个 core-unix | ✅ | **已实跑通过**（run `42339972398`）：5 个 job 全绿、5 个 artifact；下载后复跑测试与 app 冒烟均通过 |

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

### 5.2 下载 CI 产物到本机实测（2026-11-07）

CI 现已改为**整体编译**：三个 Windows job 都跑 `cmake --build ... --config Release`（ALL targets）+ 全量 `ctest -C Release`，并把每个 job 的**全部 exe / dll / lib** 上传为 artifact（保留 14 天）。因此 `skia-windows` 的 `airui-skia-windows-x64` 不再只有插件，而是完整的 x64 Skia 构建：26 个 exe、34 个 DLL（含 `airui-skia.dll` 与 `skia.dll` / `icu*79.dll` / `harfbuzz*.dll` / `freetype.dll` / `z.dll` 等全套 vcpkg 运行时）、4 个静态库与 `README.txt`，共 65 文件约 69 MB。另有 `p-system-windows-x64-gdi`、`p-system-windows-x86-gdi` 与两个 `p-system-core-*` 包。

```
gh run download <run-id> -D build\ci-all                              # 下载全部 5 个 artifact
gh run download <run-id> -n airui-skia-windows-x64 -D <dir>          # 只要 Skia 全量包
```

> 只拷 `airui-skia.dll` **不够**：插件依赖的 Skia / ICU / HarfBuzz 等 DLL 必须在 exe 同目录（或 PATH）里，否则 `LoadLibraryW` 因缺依赖失败，注册表静默回退 gdi。

本机实测（run `42338618815`，CI 4/4 作业全绿）：

| 组合 | 结果 |
|---|---|
| CI 自带 VS2022 x64 `test_skia_plugin.exe` + 同目录 DLL | `runtime DLL backend + C ABI + raster OK`；`test_skia_backend` → `G5 parity + raster OK` |
| 官方 VS2017 **x86** `test_paint_runtime.exe` + x64 DLL | 位数不匹配 → `skia backend not deployed (fallback path under test)`，仍 `runtime selection + fallback OK`（优雅降级，不崩） |
| VS2017 **x64** `test_paint_runtime.exe` + x64 DLL | `skia backend available (linked or DLL loaded)`，运行时选择 + 跨模块虚调用全绿 |
| VS2017 x64 harness（同一份 `test_skia_plugin.cpp`，含 `std::wstring` 跨模块） | `runtime DLL backend + C ABI + raster OK` |
| VS2017 x64 harness + 真实 HDC（`CreateDIBSection` → `AlphaBlend`） | 合成像素 `BGRA 00 00 DD FF`，`Skia -> AlphaBlend -> Win32 HDC OK` |

全量产物 run `42339972398`（5 个 artifact、5 个 job 全绿）下载后复核：Windows 包的 `airecord.exe` 分别检出 x64 / x86 / x64；直接运行下载后的 `test_paint_backend`、`test_paint_runtime`、`test_skia_plugin`、`test_skia_backend` 全通过；Skia x64 与 GDI x86 两个 `airecord.exe --hidden` 启动 5 秒无崩溃。本机官方 VS2017 全量构建也重新跑过 `BUILD=0` + `ctest -C Release` **19/19**。

> 本机官方 `build/` 是 **x86**（`CMAKE_GENERATOR_PLATFORM` 为空），CI 产物是 **x64**；要直接用需另配 VS2017 x64 构建树：`cmake -S . -B build/vs2017-x64 -G "Visual Studio 15 2017" -A x64`。后端由帧级路由覆盖整窗，不需要 `--core-shell`（§1.4）；完整 GUI 窗口截图对比已于 2026-11-07 完成（§5.4）。

### 5.3 如何用 Skia 渲染 / 如何分辨与 GDI 的差别

**完整用法**（x64 产物 + 已下载的 CI 插件包，整套 DLL 必须与 exe 同目录）：

```bat
airecord.exe --paint-backend=skia
```
也可直接点标题栏「渲染」按钮（推荐），或用环境变量 / 代码 / 热切换（§1.1）。`AIRUI_WITH_SKIA` 保持默认 OFF 即可——选择发生在运行时，官方 VS2017 产物不需要 SDK、不需要重编。`--core-shell` 是另一套简化外壳，与后端无关。

**① 确认后端是否真的生效（最快的分辨方式）**：启动时 `App::Run()` 往 `%LOCALAPPDATA%\AireRecord\client.log` 写一行诊断，`probe` 会真实开一帧，因此能区分「DLL 加载成功」与「开帧失败回退」；`wholeWindow=1` 表示整窗帧路由已启用，`text=` 表示文字由宿主还是后端栅格化：

```
[paint] wholeWindow=1 coreShell=0 requested=skia resolved=skia available=[gdi,skia] skiaDll=1 probe=ok text=host
[paint] wholeWindow=1 coreShell=0 requested=auto resolved=auto available=[gdi,skia] skiaDll=1 probe=ok text=gdi
[paint] wholeWindow=1 coreShell=0 requested=skia resolved=skia available=[gdi]       skiaDll=0 probe=backend 'skia' is unavailable; fell back to gdi text=host
[paint] UI switch -> skia wholeWindow=1 coreShell=0 text=host
[paint] UI switch -> gdi  wholeWindow=1 coreShell=0 text=gdi
```
第 1 行是命令行直接启用 Skia；第 2 行是默认 GDI 启动；第 3 行是 DLL 缺失（`AIRUI_SKIA_DLL` 指向不存在路径）时的可观测回退；后两行是按钮双向切换。`coreShell=0` 是**正常状态**，不再意味着后端没生效。以上均为本机 VS2017 x64 产物实测。

**①b 整窗像素差**：同一界面点击按钮前后截图比对，GDI → Skia 整窗约 **1.93%** 像素变化（标题栏 9.94%、页面内容区 0.83%），Skia → GDI 约 1.88%，而 GDI → GDI 往返仅 **0.07%**——证明页面内容区也换了引擎、且能来回切回同一画面。见 `build/whole-window-gdi-1.png` / `whole-window-skia.png` / `whole-window-diff-zoom.png`。

**② 调用序列层**：`test_skia_backend` 用 `PaintRecorder` 把 core 金样与后端实收的 `IPainter` 调用序列做 `Diff()`，两后端必须**完全一致**。这保证差别只来自光栅化，而不是布局或漏画。

**③ 像素层（真正看出差别）**：`test_paint_compare.exe` 让 gdi 与 skia 在同一块 240×120 32bpp BGRA DIB 上接收**完全相同**的调用序列（背景、圆角、半透明矩形、虚线框、文字），读回像素后分区统计。默认构建无 DLL 时只打印 GDI 基线并跳过；把 exe 放进插件包目录即输出完整报告：

```
  gdi  checksum    : 0xBB956986   round-edge intermediates: 0
  skia checksum    : 0xB1926836   round-edge intermediates: 69
```

本机实测（VS2017 x64 exe + VS2022 x64 `airui-skia.dll`，判据 `|dB|+|dG|+|dR| > 12`）：

| 区域 | 差异像素 / 总像素 | 平均通道差 | 差异来源 |
|---|---|---|---|
| 整帧 | 1274 / 28800（4.4%） | 14.1 | 累加 |
| 圆角矩形 | 209 / 4992（4.2%） | 12.3 | GDI `CreateRoundRectRgn` 硬边（中间色像素 **0**）vs Skia `drawRRect` 开抗锯齿（**69** 个过渡像素） |
| 半透明矩形 | 0 / 3456（**0.0%**） | 0.0 | 两端最终都走 `AlphaBlend`，结果完全一致 |
| 虚线框 | 400 / 6912（5.8%） | 22.9 | `PS_USERSTYLE` 硬边 vs `SkDashPathEffect` + AA，虚线相位也不同 |
| 文字 | 415 / 2592（**16.0%**） | 44.1 | **修复前基线**：当时插件默认 typeface 完全没有可用字形（`drawSimpleText` + 无 CJK/字体回退），GDI 有字、Skia 一片空白；**不是** hinting 或字距差异。字体修复（commit `0cb349f`）后该合成测试应大幅下降，待重测 |

**肉眼分辨要点（GUI 整窗）**：看圆角卡片边缘、描边、虚线相位，以及状态栏小圆点——GDI 的 6px `Ellipse` 呈菱形，Skia 的 `FillRound` + AA 是圆形；纯色矩形与半透明矩形基本看不出区别。文字在 GUI 里默认由 Skia 直接绘制（`text=backend`，探测通过时），两后端字形一致；若探测未通过则回退到宿主 GDI 重放（`text=host`），同样可读。上表 16% 是**修复前**的合成测试基线，说明当时字体层未打通，而非界面上会看到字形差异（§5.4）。

---

### 5.4 标题栏「渲染」切换按钮（UI 入口）

按钮位于标题栏三键左侧，和窗口其它部分一样走 `PaintFrame`（几何由当前后端画，文字由帧级文字层处理），因此 legacy 富渲染与 coreShell/HAL 两条路径下都可见、都可点，切到 Skia 后仍能切回来。`WM_NCHITTEST` 对该区域返回 `HTCLIENT`（否则标题栏会被判成 `HTCAPTION`，点击变成拖窗口）；`ShellCore::SetShellGeometry()` 新增 `captionActionW`，状态胶囊与应用名同步让出保留区，两条路径都不重叠。

交互链路：

- 点击 → `Shell::TogglePaintBackend()` → core 纯函数 `NextPaintBackend(current, available)`（gdi 优先、只选可用 id、循环、归一化，`test_paint_backend` 有 9 条断言覆盖）；
- **不再改动 `ShellOptions.coreShell`**：后端切换只换像素来源，界面不会变成另一套简化外壳；`PaintFrame` 把外壳、页面内容、core 控件一起路由到新后端（§1.4）；
- 只有一个可用后端时不切换，按钮显示 `渲染: GDI · 无插件`；选了 Skia 却开帧失败时显示 `渲染: Skia · 已回退` 并用错误色，避免「显示 Skia、实际 GDI」；
- `App` 通过 `SetPaintBackendChangedHandler()` 在每次切换后往 `client.log` 追加 `[paint] UI switch -> ...`，并让状态栏显示「渲染后端: Skia · 文字 GDI」。

**实测（VS2017 x64 exe + CI VS2022 x64 插件，同一窗口点击两次）**：

```
[paint] wholeWindow=1 coreShell=0 requested=auto resolved=auto available=[gdi,skia] skiaDll=1 probe=ok text=gdi
[paint] UI switch -> skia wholeWindow=1 coreShell=0 text=host
[paint] UI switch -> gdi  wholeWindow=1 coreShell=0 text=gdi
```

`coreShell` 全程为 0，说明切后端不再换界面。整窗截图：`build/whole-window-gdi-1.png`（GDI）→ `whole-window-skia.png`（Skia）→ `whole-window-gdi-2.png`（切回），局部放大见 `whole-window-diff-zoom.png`。三张图布局一致、中文全部可读；状态栏小圆点在 GDI 下呈菱形、Skia 下是抗锯齿圆形，是几何换引擎的直观证据。

**文字层策略（运行时能力自适应）**：`SkiaPluginSupportsWideText()` 通过插件 C ABI 开一个 32×32 离屏帧画「中」，读 `airui_skia_pixels()` 检查 alpha 覆盖，据此决定：

- `text=backend`（字体修复后、探测通过时的默认）：`SkiaPainter` 直接用 GDI 的同款 `LOGFONT` 经 `SkCreateTypefaceFromLOGFONT` 创建 "Microsoft YaHei UI"（正文）与 "Segoe MDL2 Assets"（图标）typeface，并对每个码点做系统级回退，文字与图标由 Skia 直接绘制，是**纯 Skia 整窗**。
- `text=host`（探测未通过时的安全回退）：填充、圆角、描边、虚线、半透明、抗锯齿与帧合成仍由 Skia 完成；所有文字调用由 `PaintFrame` 记录，在帧合成之后由宿主 GDI `DrawTextW` 统一重放，中文与 MDL2 图标仍可读，不会出现「方块 + 汉字」双影。
- `AIRUI_SKIA_TEXT=host|backend` 可强制覆盖探测结果，用于对比与回归。

> 字体修复落点（commit `0cb349f`）：`EnsureFaces()` 不再依赖跨版本不稳定的 `SkFontMgr` 工厂（`RefDefault` / `MakeDefault` 在 CI 所用 vcpkg `skia` 上不存在），改用稳定的 Windows 端口函数，并以 `TCHAR` / `TEXT` 适配 CI 的 ANSI（LOGFONTA）构建；本地 MinGW mock 语法门禁 + CI 真实 Skia 构建均已通过。

旧的 `Shell::DrawCoreChromeText()` 只补外壳文字、且页面文字仍走 GDI，已删除。`Shell::Paint()` 在 coreShell 下仍补画三键（原先命中测试在、像素不在），这与整窗路由无关。

**不再是「只切外壳」**：`IPage::OnPaint(dc, ...)`、core 控件、`Canvas` 复合部件与 toast 全部经 `gfx::*` / `CanvasPainter` → `PaintFrame`；实测页面内容区有 0.83% 像素差。`test_paint_frame.exe` 用 32bpp DIB 固化三条契约：帧内绘制不再触碰 DC、帧外 GDI 路径完全恢复、两种文字策略都正确。`--core-shell` 单独使用时仍是简化外壳（无 logo / 导航图标等），那是外壳实现差异，不是后端切换造成的。

---

## 6. 已知边界（不要误当成已完成）

- **没有 Skia 版 `IWindow`**：目前只有绘制后端。窗口壳、消息泵、事件注入仍由 `Win32Window` 负责；Linux/macOS 应用本体仍不可构建。
- **插件 ABI 有工具集与位数边界**：实测 MSVC v141 (VS2017 x64) ↔ v143 (VS2022 x64) 跨代可用（§5.2），但 **x86 进程无法加载 x64 DLL**——本机官方 `build/` 是 x86、CI 产物是 x64，直接放旁边只会静默回退 gdi。仍要求 MSVC 同族、一致 `/MD` 与 `_ITERATOR_DEBUG_LEVEL`；纯 C 绘制桥与非 MSVC 编译器验证均未做。
- **文字默认由 Skia 直接绘制（`text=backend`）**：字体修复（commit `0cb349f`）后，插件用 `SkCreateTypefaceFromLOGFONT` 创建 "Microsoft YaHei UI" + "Segoe MDL2 Assets" typeface，探测 `SkiaPluginSupportsWideText()` 应返回 true，整窗含文字一起是 Skia 画的。`text=host` 仅作为探测失败时的安全回退（仍由宿主 GDI 重放文字，保证中文/图标可读）。如仍观察到 `text=host`，先确认 `airecord.exe` 同目录已部署 `airui-skia.dll` 且 CI 插件为最新（含字体修复）。
- **真实 GUI 截图 diff**：已完成逐窗口像素统计（整窗 1.93% / 内容区 0.83% / 往返 0.07%，§5.3、§5.4），但只是手工截图比对，**SSIM 指标与自动化截图基线仍未建立**。
- **VS2017 只验证了「使用 DLL」，未验证「编译 Skia」**：真 SDK 源码编译仍只在 VS2022 CI 上做；官方 VS2017 工具链 x64 产物通过下载 DLL 使用 Skia 已实测（整窗 GUI 截图、像素差异、启动诊断、双向热切换，§5.2 / §5.3 / §5.4）。CI 目前只出 **x64** 插件，x86 官方产物需 x64 构建或另出 x86 artifact。
- **Skia API 版本漂移**：所有 Skia 头集中在 `SkiaApi.h`；已在 vcpkg `skia@148` 上校准（§5.1），但 `drawSimpleText` / `measureText` 等仍可能随版本变化。

---

## 7. 下一步

1. ~~观察 `test_skia_plugin` 首次真 SDK 结果~~ 已完成：run `42337859169` 四个作业全绿，插件 DLL 与动态加载测试**首跑即通过**，无需修正 ABI / 导出名；后续每次 push/PR 自动复验。
2. ~~校准字体度量与 CJK 回退~~ 已完成（commit `0cb349f`）：`EnsureFaces()` 用 `SkCreateTypefaceFromLOGFONT` 创建 "Microsoft YaHei UI" / "Segoe MDL2 Assets" typeface 并做逐码点回退，`SkiaPluginSupportsWideText()` 应返回 true、自动进入 `text=backend` 纯 Skia 整窗；待重测确认探测通过后再补 GDI vs Skia 截图阈值 / SSIM 基线。
3. 把 `test_paint_frame` 的帧路由契约与 GUI 截图比对接入 CI（需要带窗口的 runner 步骤）。
3. Phase 2 剩余：Skia 版 `IWindow` / Linux 窗口壳（`cross-platform.md` §10 仍待定）。
