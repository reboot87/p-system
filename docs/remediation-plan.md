# p-system 整改方案（2026-11-15 代码审查）

| 项目 | 内容 |
|---|---|
| 状态 | 待评审 |
| 基线 | `aa9d8b2`（`main`，工作区干净） |
| 来源 | 2026-11-15 全量代码审查（开发流程 / 架构 / 代码 / 测试 / 文档） |
| 关联文档 | `docs/v0.8-baseline.md`、`docs/toolbox-plugin-plan.md`、`docs/ROADMAP.md`、`docs/design-patterns.md` |
| 适用范围 | `main` 后续所有提交；`v0.8` tag 及已冻结基线内容不改 |

> **行号说明**：本文所有 `file:line` 均基于基线 `aa9d8b2`。代码改动后行号会漂移，请按**符号名 / 函数名**定位，并在合并时同步更新本文的引用。
>
> **核心判断**：本项目的问题不是单点缺陷，而是「文档写下的约束没有自动化环节执行」。因此整改顺序是 **先让约束生效 → 再消除运行时风险 → 最后收敛架构与文档**，而不是反过来先做局部优化。

---

## 0. 使用约定

每个整改项固定包含五段：**证据 / 影响 / 改法 / 验收 / 风险与回滚**。执行时遵守：

1. **一次一批**，每批单独提交，提交信息带批次号（如 `P0-1: run core cleanliness gate in CI`）。
2. 每批结束必须跑完 §6 对应命令，全绿才进入下一批。
3. **先改门禁，再改行为**：P0-1 没落地前不要动 `src/airui/core/`，否则没有安全网。
4. 破坏性改动（ABI v2、目录重命名、版本来源）必须单独 PR，不与其他修复混在一起。
5. 文档中若出现「待评审」「计划项」，整改完成后要改成可验证的事实陈述，不要留下新的「声称 ≠ 实现」。

---

## 1. 批次总览

| 批次 | 目标 | 条目 | 完成判据 |
|---|---|---|---|
| **P0** | 消除崩溃 / 泄漏风险，让约束第一次真正生效 | P0-1 ~ P0-5 | CI 两平台门禁跑起来；detach 线程可 join；A11y 无泄漏；模态锁状态 RAII |
| **P1** | 工程化收敛：目录、版本、CI、静态检查 | P1-1 ~ P1-6 | 单一构建树命名、单一版本来源、`build.bat`/`build.sh` 默认跑测试、lint 入 CI |
| **P2** | 架构边界与插件 ABI | P2-1 ~ P2-5 | core 头隔离由 CMake 强制、Linux 能构建全部平台无关测试、ABI v2 带 caps |
| **P3** | 测试资产、文档、仓库清理 | P3-1 ~ P3-5 | 无孤儿测试、金样跨平台比对、文档与实现一致 |

**依赖关系**：`P0-1 → P2-1`（先有门禁再动 CMake 边界）；`P2-2 → P1-2`（先修 LINK_FLAGS 才能扩大 Linux 测试面）；`P0-3` 必须在 v1.1 可插拔落地**之前**完成。

---

## 2. P0 —— 阻断项

### P0-1 core 纯净性门禁从未执行

- **证据**：`CMakeLists.txt:719-724` 的 `check-core-clean` 是 `add_custom_target`，**无 `ALL`、无 `add_dependencies`、无 `add_test`**；`.github/workflows/ci.yml` 全文无 `check-core-clean`（`core-unix` 只跑 `check-skia-syntax.sh` 与 `check-core-linux-smoke.sh`，`windows` 只跑 build + ctest）。但 `docs/design-patterns.md:16` 写「在 CI 中强制校验」、`docs/ROADMAP.md:16` 写「由门禁保证」、`docs/toolbox-plugin-plan.md:276/412` 把 `PASSED` 列为合入门槛、`v0.8` tag 说明也声称已通过。
- **影响**：`src/airui/core/` 现在确实干净，但这是**人工维持的结果**，不是被保证的。任何人加一个 `#include <windows.h>` 或 `target_link_libraries(airui-core user32)`，CI 全绿。
- **改法**（两步，分两个提交）：

  **① 先接入 CI，不改脚本语义**：

  ```yaml
  # .github/workflows/ci.yml — core-unix，插在 "Skia backend API contract gate" 之前
  - name: Core cleanliness gate
    run: bash scripts/check-core-clean.sh

  # windows / windows-x86，插在各自 "Build ALL targets" 之前
  - name: Core cleanliness gate
    shell: pwsh
    run: |
      powershell -NoProfile -ExecutionPolicy Bypass -File scripts/check-core-clean.ps1
      if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  ```

  **② 再修脚本本身**（`scripts/check-core-clean.sh:10/22`）：`set -u` 改 `set -eu`；`|| true` 会吞掉 grep 的错误码，改为区分「无匹配 rc=1」与「执行失败 rc≥2」：

  ```bash
  if hits=$(grep -rnE "$pattern" --include='*.h' --include='*.cpp' "$core"); then
      printf '%s\n' "$hits"
      echo "check-core-clean: FAILED" >&2
      exit 1
  else
      rc=$?
      if [ "$rc" -ne 1 ]; then
          echo "check-core-clean: grep failed (rc=$rc)" >&2
          exit 2
      fi
  fi
  ```

  把正则抽到 `scripts/core-forbidden.regex`（单行文件），`.sh` 用 `pattern=$(cat "$here/core-forbidden.regex")`、`.ps1` 用 `(Get-Content -Raw).Trim()` 读取，消除两份手工复制的 pattern。**扩展 pattern 单独再提一个 PR**：先跑一次基线看现有 core 是否命中，再补 `_WIN32|WINAPI|#pragma\s+comment\s*\(\s*lib|LoadLibrary|GetProcAddress|CreateFileW|\bHANDLE\b|\bHKEY\b|<dwmapi\.h>|#include\s*[<"]windows` 等；注意 `\bPOINT\b` / `\bMSG\b` 在注释里易误伤，扩展时同步评估。

- **验收**：`bash scripts/check-core-clean.sh` 输出 PASSED；往 core 临时加一行 `#include <windows.h>`，CI 门禁步骤必须失败（本地验证后删除）；`ci.yml` 两个 Windows job + `core-unix` 日志均能看到该步骤。
- **风险 / 回滚**：脚本语义变化可能让历史注释里的 `RECT`、`DrawText` 触发 FAIL（现有脚本扫全文含注释）。先只做 ①，② 前跑一次全量基线，把误报清单附在 PR 里。回滚只需删除 CI 步骤。

### P0-2 后台线程捕获裸 `this`，且跨线程写共享数据

- **证据**：`src/core/App.cpp:264`（重连探测）、`:472`（数据刷新）两处 `std::thread(...).detach()`；`src/main.cpp:115` 的 `App` 是栈对象，`App::Run()` 结束即 `return rc`（`App.cpp:446`），`App::~App() = default`（`:17`）不等待任何线程。
  - `:476` 在后台线程调用 `m_clips->GetSearchText()`（读 UI 控件），与 `:473` 注释「不碰 UI 控件」矛盾。
  - `src/pages/ClipsPage.cpp:356-386` 的 `ReloadEntries()` 虽不碰窗口句柄，但通过 `m_app.SetEntries(...)`（`App.h:118`）写 `m_entries`；UI 线程在绘制时经 `Entries()`（`App.h:116`）读同一个 `std::vector<UiEntry>`，**无锁数据竞争**。`RefreshStats()`（`App.cpp:554`）同样写 `m_stats` / `m_svcState` / `m_svcText`。`:474` 的「UI 只在 WM_APP+6 后才读」只是时序约定，`ApplyRefresh()` 之外的绘制路径并不遵守。
- **影响**：退出时 UAF（概率性崩溃）、列表重建时读到半更新的 vector、筛选文本从工作线程读编辑框。
- **改法**（推荐「可 join 线程 + 结果经消息回投」，一次改完）：

  1. `App.h` 增加两个具名线程成员与停止标志；`App.cpp:17` 的析构改为先置位再 join：

     ```cpp
     // App.h
     std::thread      m_reconnectThread;
     std::thread      m_refreshThread;
     std::atomic<bool> m_shuttingDown{ false };

     // App.cpp
     App::~App()
     {
         m_shuttingDown.store(true);
         if (m_reconnectThread.joinable()) m_reconnectThread.join();
         if (m_refreshThread.joinable())  m_refreshThread.join();
     }
     ```

     两个任务各自已有 `m_reconnecting` / `m_refreshing` 防抖，同一时刻最多一个实例在途，因此可以在启动新线程前 `if (t.joinable()) t.join();`——防抖已保证前一个结束，join 立即返回。退出时最坏等待一次 `Connect()`（约 1s）或一次 `Request()`（1.5s）；若不可接受，再给 `PipeClient` 加取消信号，不要靠 detach 规避。

  2. **所有 UI 取值留在 UI 线程**：`RefreshData()` 里先取 `const std::wstring filter = m_clips ? m_clips->GetSearchText() : L"";`，再按值捕获进 lambda。

  3. **共享状态只在 UI 线程写**：把 `RefreshStats()` 与 `ReloadEntries()` 的产出收进一个结果结构，后台线程堆分配后经 `PostMessage` 回投，`WM_APP+6` 在 UI 线程取回并应用：

     ```cpp
     struct RefreshResult {
         ServiceStats            stats;
         int                     svcState = 0;
         std::wstring            svcText;
         std::vector<UiEntry>    entries;
         bool                    ok = false;
     };

     // 后台线程末尾
     auto* out = new RefreshResult{ /* 填充结果 */ };
     if (!PostMessage(hwnd, WM_APP + 6, 0, reinterpret_cast<LPARAM>(out)))
         delete out;                       // 窗口已销毁：必须回收

     // UI 线程 WM_APP+6
     std::unique_ptr<RefreshResult> r(reinterpret_cast<RefreshResult*>(lParam));
     if (r) {
         m_stats    = r->stats;
         m_svcState = r->svcState;
         m_svcText  = std::move(r->svcText);
         m_entries  = std::move(r->entries);
         ApplyRefresh();
     }
     ```

     这要求把 `RefreshStats()` 从「写成员」重构成「返回结构」，并把 `ClipsPage::ReloadEntries()` 拆出「纯 IPC 解析为 `vector<UiEntry>`」的函数（`SetEntries` 调用移回 UI 线程）。同时清理 `App.cpp:473-474` 那条自相矛盾的注释。

  **最小止血方案**（若 P0 时间紧）：不改数据结构，只把线程捕获从 `this` 换成 `std::shared_ptr<后台状态>`，并保证退出前 join；随后立刻补数据竞争修复。不要只加 `join()` 而不处理 `m_entries` 竞争。

- **验收**：`ctest -C Release` 全绿；用 Application Verifier / ASan（MSVC `/fsanitize=address`）跑「启动 → 反复切页 → 立刻关闭」20 次无崩溃；`AIRUI_FRAME_PROFILE=1` 下切页不再卡顿。
- **风险 / 回滚**：`RefreshStats` 重构牵涉总览 / 配置页读取路径，是本批最大改动面；建议单独提交，回滚时保留 join 改动、只回退结果结构。

### P0-3 插件 ABI 覆盖不到真正的跨界点

- **证据**：`src/airui/platform/skia/airui/SkiaPluginAbi.h:18` 只有 `AIRUI_SKIA_PLUGIN_ABI 1`，`:40-52` 描述 5 个 C 函数形状；但 `src/airui/platform/win32/PaintProviders.cpp:112-119` 通过 `IPainter*` 虚表把 `const std::wstring&` 传给插件，`:230-232` 注释明确写着「MSVC vtable/CRT layout matches across toolsets」。`docs/toolbox-plugin-plan.md:17` 却声称「零 STL 跨界」。
  - `PaintProviders.cpp:227-254` 用「画一个汉字数 alpha>8 的像素，`ink > 4`」代替能力协商，还有 `AIRUI_SKIA_TEXT` 环境变量旁路。
  - `:69-79` 的 `LoadLibraryW` 在函数指针缺失 / ABI 不符时直接 `return false`，`a.module` 从不 `FreeLibrary`；`:199-205` 用「永不卸载」规避 vtable 悬空。
- **影响**：改 `IPainter` 虚函数集时 ABI 号不会变，旧 DLL 会被当成兼容加载 → 难以定位的崩溃；这是 v1.1「可插拔工具箱」的地基问题，越晚改破坏性越大。
- **改法**（列为一个独立 PR，与 `docs/toolbox-plugin-plan.md` 的 v0.9「一次定死」对齐）：

  1. 新 ABI 头放 **`src/pluginapi/`**（按项目约定，不能进 `src/airui/core/`，会踩平台标识符 grep 门禁）。结构体带 `size` + `caps` + **painter 接口修订号**：

     ```c
     /* src/pluginapi/airui_plugin_abi.h */
     #define AIRUI_PLUGIN_ABI 2

     enum {
         AIRUI_CAP_WIDE_TEXT    = 1u << 0,
         AIRUI_CAP_SUBPIXEL_AA  = 1u << 1,
         AIRUI_CAP_OFFSCREEN    = 1u << 2,
     };

     typedef struct AiruiPluginDesc {
         uint32_t size;              /* sizeof(AiruiPluginDesc) */
         uint32_t abi;               /* AIRUI_PLUGIN_ABI */
         uint32_t caps;              /* AIRUI_CAP_* */
         uint32_t painterIfaceRev;   /* IPainter 虚函数集 / 布局的修订号 */
     } AiruiPluginDesc;

     typedef const AiruiPluginDesc* (AIRUI_CALL *AiruiPluginDescribeFn)();
     ```

     `painterIfaceRev` 是这次的关键：它把「C 函数形状」与「跨模块 C++ 虚表布局」两个契约分开表达，宿主在 `LoadPlugin()` 里同时校验 `abi`、`size`、`painterIfaceRev`，任一不符即拒绝并回退 gdi。

  2. `IPainter::Text` / `TextBlock` / `MeasureText` 改为 `const wchar_t* + size_t len`（`src/airui/core/include/airui/hal.h:92-97`），同步改 4 处转发实现：`PaintProviders.cpp:89-127`、`SkiaSurface.cpp:380-417`、`:453-488` 宏转发、`GdiPainter.h:24-226`，以及全部调用点。**这一步破坏性最大，必须与 ABI v2 同批发布**。
  3. 用 `desc->caps & AIRUI_CAP_WIDE_TEXT` 取代 `SkiaPluginSupportsWideText()` 的像素探测；保留环境变量仅作为调试覆盖，并在日志里打印 `caps` 值。
  4. `LoadPlugin()` 只在**加载失败且尚未创建任何 painter** 的分支 `FreeLibrary(a.module)` 并清空函数指针；一旦有活跃 painter，仍保持模块常驻（保留现有注释解释原因）。

- **验收**：`test_skia_plugin` / `test_skia_backend` 通过；构造一个 `abi=1` 的旧 DLL，宿主 `client.log` 必须记录明确拒绝原因并回退 gdi；不存在 `std::wstring` 跨 DLL 的调用点（`git grep` 校验）。
- **风险 / 回滚**：宿主与 CI 插件产物需要同步发布，中间态会回退 gdi（功能降级但不崩）。发布顺序：先宿主支持「caps 缺失 → 走旧探测」的兼容层，再发新 DLL，最后删兼容层。

### P0-4 无障碍属性是纯泄漏，且功能无效

- **证据**：`src/airui/platform/win32/Accessibility.cpp:30/36/51/52/53` 用 `SetPropW(hwnd, ..., (HANDLE)new std::wstring(...))` 分配堆对象；全文件无 `RemovePropW`、无 `delete`，`LiveRegion::~LiveRegion()`（`:85-88`）只 `DestroyWindow`。**全仓库检索确认这些属性没有任何 `GetPropW` 读取方**；`:23` 注释自认「简化：使用 SetProp 存储，实际需要实现 IAccessible」。对照 `src/airui/platform/win32/Controls.cpp:185-189` 的 `PhData` 有 `RemovePropW` + `delete`。
- **影响**：每个设置过名称 / 描述 / 值的窗口泄漏一个 `std::wstring`；`SetAccessibleName` 重复调用还会覆盖旧指针使其永久丢失；`LiveRegion::Announce()`（`:90-93`）只是改一个没人读的属性，**等于无效**。
- **改法**：既然无读取方，直接删掉这些分配，改为明确的空实现 + 可检索的欠账标记，并把真实 `IAccessible` 实现列入 ROADMAP：

  ```cpp
  // TODO(a11y): SetProp 伪实现无任何读取方，真实 IAccessible 见 docs/ROADMAP.md
  void SetAccessibleName(HWND, const std::wstring&) {}
  void SetAccessibleDescription(HWND, const std::wstring&) {}
  void SetAccessibleValue(HWND, const std::wstring&,
                          const std::wstring&, const std::wstring&) {}
  ```

  `:24`（角色）与 `:44`（状态位）存的是整数值、不分配堆，可以保留，但同样无读取方，建议一并标注 TODO。若选择保留存储语义，则必须加 `WM_NCDESTROY` 钩子统一 `RemovePropW` + `delete`，且 `Set*` 前先释放旧值。

- **验收**：`grep -n "new std::wstring" src/airui/platform/win32/Accessibility.cpp` 无结果；窗口创建 / 销毁循环下任务管理器句柄数与内存无增长。
- **风险 / 回滚**：无外部行为依赖（属性无人读取），风险极低。

### P0-5 模态 / 重绘的锁状态非 RAII

- **证据**：`OverlayWindow.cpp:747` `EnableWindow(owner, FALSE)` 与 `:770` 有条件恢复之间隔着 20 行的局部消息循环（`:755-765`）；`RunPopupMenu` 也有同样的局部消息循环（`:945-952`，注意它自身没有 `EnableWindow` 调用）。`Shell.cpp:358/375` 的 `WM_SETREDRAW` 冻结、`Widgets.cpp:469` 的 `m_updateLock++` 同样是「手动加 / 手动减」。`Shell.cpp:1655` 无条件 `PostQuitMessage(0)` 与多顶层窗口 / 浮层并存冲突。
- **影响**：异常、`got < 0` 提前 break、或未来在循环中插入提前返回，都会让主窗口永久禁用或永久停绘。
- **改法**：抽 RAII 小工具（放 `src/airui/platform/win32/airui/`），循环体改为作用域对象：

  ```cpp
  class ScopedWindowDisable {
  public:
      explicit ScopedWindowDisable(HWND h)
          : m_h(h), m_was(h && IsWindowEnabled(h)) {
          if (m_was) EnableWindow(h, FALSE);
      }
      ~ScopedWindowDisable() {
          if (m_was && IsWindow(m_h)) EnableWindow(m_h, TRUE);
      }
      ScopedWindowDisable(const ScopedWindowDisable&) = delete;
      ScopedWindowDisable& operator=(const ScopedWindowDisable&) = delete;
  private:
      HWND m_h;
      bool  m_was;
  };
  ```

  `RunModalDialog` / `RunPopupMenu` 用 `ScopedWindowDisable disable(owner);` 包住 `layer.Show(true)` 到 `layer.Destroy()`；`ScopedRedrawFreeze` 同理，析构里调 `Shell::SetRedraw(true)`（现有实现已带 `RDW_ALLCHILDREN` 补偿）。`WM_DESTROY` 的 `PostQuitMessage` 改为仅在主窗口且非模态嵌套时投递。

- **验收**：确认框 / 右键菜单连续开关 50 次后主窗口始终可交互；`WM_SETREDRAW` 路径上注入一次异常（或模拟提前返回）不复现「窗口空白」。
- **风险 / 回滚**：`ownerWasEnabled` 的既有语义（`:767-770` 注释）已由 `m_was` 保留，注意 `IsWindow` 检查防止窗口先销毁。

---

## 3. P1 —— 流程与工程化

### P1-1 构建树命名与 `.gitignore`

- **证据**：仓库内并存 `build/`（约 973 MB）、`build-cmake/`、`build-vs2022/`、`build-x64/`、`build-linux-smoke/`；各入口各写各的：`build.bat:23` → `build-cmake`，`build.sh:20` → `build-linux`，`README.md:36` → `build`，`scripts/shot.py:92-93` → `build/vs2017-x64/Release/`（该路径**没有任何命令会生成**），CI → `build-core` / `build-x86` / `build-skia`。实测 `build-core`、`build-x86`、`build-skia`、`build-linux` 四个目录 **NOT IGNORED**。
- **改法**：约定单一本地入口 `build-cmake/`（`build.bat` 产物），CI 目录保持 `build-core` / `build-x86` / `build-skia`；`.gitignore` 追加：

  ```gitignore
  build-core/
  build-x86/
  build-skia/
  build-linux/
  build-local/
  ```

  `README.md:36` 手动示例改成 `-B build-cmake`（与 `build.bat` 一致）；`scripts/shot.py:92-93` 改成从 `build-cmake/Release/airecord.exe` 定位，或改为从命令行参数 / 环境变量取路径，不再硬编码不存在的目录。已跟踪的 7 个 `gallery_*.txt` 手工调试输出执行 `git rm --cached` 并加入忽略规则（**先确认无脚本依赖**）。

- **验收**：按 README 三套命令各跑一次后 `git status --short` 为空；`git check-ignore -v --no-index build-core/CMakeCache.txt` 等四项均 IGNORED。
- **风险 / 回滚**：纯配置；`git rm --cached` 会让其他克隆者本地文件变为未跟踪，需在 PR 说明里写明。

### P1-2 版本来源收敛到一处

- **证据**：`App.h:172` `L"v1.3"`；`App.cpp:612-613`、`:648` `L"AireRecord v1.3.1 ..."`；`Responsive.cpp:15/32` `L"AireRecord v1.0"`；`CMakeLists.txt:2` `project(AireRecord)` **无 VERSION**；`res/app.rc` 无版本块；git tag 却是 `v0.8`。`docs/v0.8-baseline.md:9-12` 早在 2026-11-09 就要求统一。
- **改法**：

  ```cmake
  # CMakeLists.txt
  project(AireRecord VERSION 0.9.0)
  configure_file(cmake/Version.h.in
                 ${CMAKE_BINARY_DIR}/generated/airui/Version.h @ONLY)
  ```

  `cmake/Version.h.in` 同时提供窄 / 宽字符串宏（`#define AIRUI_VERSION_W L"@PROJECT_VERSION@"`），所有目标通过 `target_include_directories(... ${CMAKE_BINARY_DIR}/generated)` 可见。新增 `App::Version()` / 一个 `kAppFooterTitle` 常量作为 UI 文案唯一拼接点，`App.cpp:612/613/648`、`Responsive.cpp:15/32` 全部引用它；`examples/DemoWindow.cpp:185` 的 `AirUI Demo v1.0` 单独保留或改读同一宏。补一个 `CHANGELOG.md`，条目从 `v0.8` 开始。

- **验收**：`git grep -n "v1\.3\|v1\.0" src/` 无硬编码；`airecord.exe` 页脚显示 `AireRecord v0.9.0 · ...`；`cmake --build` 后生成的 `Version.h` 与 tag 一致（发布时由流水线注入）。
- **风险 / 回滚**：纯展示层；注意 `App.cpp` 三处是三元 / 条件拼接，抽常量时保持「服务已连接 / 未连接」后缀不变。

### P1-3 CI 作业的真实地位与缓存

- **证据**：`ci.yml:240` 整个 `skia-windows` 作业 `continue-on-error: true`，但 `README.md:111` 写「CI 每次 push/PR 都做整体编译并发布全部产物」、`docs/ROADMAP.md:56` 写「真 SDK 实跑通过」、`docs/cross-platform.md:11` 把它列为验证承担者，三处都未提示该作业非阻塞。`ci.yml:119-129` 同一段注释写了两遍且关于 `GITHUB_TOKEN` 的结论互相矛盾。缓存 key `vcpkg-skia-windows-2022-v2`（`:273`）固定、`:280` 的 `vcpkg install skia:x64-windows` 不锁版本，而 `docs/skia-backend.md` 记录的是 skia m148。
- **改法**：
  1. 删除 `:119-123` 或 `:124-129` 其中一段重复注释。
  2. 把 `skia-windows` 拆成两个 job：`skia-sdk`（vcpkg 安装 + 缓存，允许非阻塞）与 `skia-verify`（编译 + `test_skia_backend` / `test_skia_plugin` / ctest，**阻塞**）。若暂时不拆，至少在 README / ROADMAP 明确标注「Skia 作业非阻塞，核心门禁是 `windows` + `core-unix`」，并把 branch protection 的 required checks 列清楚。
  3. 依赖锁定改用 `vcpkg.json` manifest（`builtin-baseline` + `overrides` 指定 skia 版本），缓存 key 加入 `hashFiles('vcpkg.json')`；**不要在经典模式下写 `skia:x64-windows@148`**（该语法不生效）。
  4. `windows` / `windows-x86` 两个 job 是复制粘贴，考虑用 matrix `arch: [x64, Win32]` 收敛（可选，P3）。

- **验收**：CI 日志中 required checks 与文档表格一致；故意让 `test_skia_plugin` 失败，合并必须被阻断。
- **风险 / 回滚**：拆 job 会改变 artifact 名称，需同步更新 `README.md:117-122` 表格与 `scripts/deploy-skia-plugin.cmake` 的下载名。

### P1-4 官方构建入口默认跑测试

- **证据**：`build.bat:83-85` 不带 `test` 参数时打印 `Skipped tests`；`build.sh:59-62` 默认 `DO_TEST=0`。而 `docs/v0.8-baseline.md` 把 `ctest 22/22` 当作交付质量证据。
- **改法**：两个脚本把测试改为默认开启，改成「`--no-test` / `notest` 显式跳过」；`build.bat` 的 `:73` 判断改为 `if not defined DO_SKIP_TEST`，`build.sh:24` 改 `DO_TEST=1`。README「其他用法」同步。
- **验收**：裸跑 `build.bat` / `./build.sh` 日志里出现测试结果；`ctest` 失败时脚本返回非 0。
- **风险 / 回滚**：本地增量构建时间增加（22 个用例，秒级）；保留跳过参数供快速迭代。

### P1-5 静态检查与格式化

- **证据**：仓库无 `.clang-format` / `.clang-tidy` / `.editorconfig`，CI 无 lint / 覆盖率 / sanitizer 步骤；全库 `try/catch` 20 处（多为注释），错误处理风格混用 bool / HRESULT / `GetLastError` / sqlite rc。
- **改法**：先加 `.clang-format`（基于现有 MSVC 风格）与 `.editorconfig`；CI 加一步 `git diff --check` + `clang-format --dry-run --Werror`（只检查本次 PR 改动文件，避免全库历史欠账一次性爆掉）；第二阶段再引入 `clang-tidy` 并针对 `bugprone-*` / `clang-analyzer-*` 子集。Python 取证脚本补 `scripts/requirements.txt`（`Pillow` 等），现在 `hover_frames.py:114/178/246` 是函数内延迟导入、缺依赖时才暴露。
- **验收**：PR 中格式错误会被 CI 拦下；新代码不引入新的风格分歧。
- **风险 / 回滚**：只对增量文件检查，历史代码不动。

### P1-6 提交流程与决策记录

- **证据**：154 次提交中 48 个 `fix`、仅 2 个 `test`；只有 `main` 单分支、2 个 tag、无 CHANGELOG；近期服务生命周期决策连续反转（`466b688`「服务停则 UI 退出」→ `c209f7e`「离线 UI 保持存活」），前一条的注释仍留在同一个头文件里（`App.h:143`「统一退出」与 `App.h:136`「UI 保持存活」并列，读者无法判断哪个是当前契约）。
- **改法**：新增 `docs/adr/` 轻量模板，至少补 4 条决策记录：**① 工具链矩阵**（本地 VS2017 / CI VS2022 / Skia 路径）、**② 版本号策略**、**③ 服务与 UI 生命周期契约**（离线时谁存活、pending.db 由谁同步、SCM 失败操作）、**④ 插件 ABI v2 破坏性变更**。每条含「背景 / 决定 / 后果 / 被替代的旧决定」。PR 模板加「本次是否改变了上述契约」勾选项。
- **验收**：`docs/adr/` 至少 4 篇；下一次生命周期变更有对应 ADR 且旧注释被清理。
- **风险 / 回滚**：纯文档。

---

## 4. P2 —— 架构

### P2-1 用 CMake 边界替代全局 include

- **证据**：`CMakeLists.txt:20-26` 用全局 `include_directories` 把 core / win32 / skia / `src` / sqlite 五个路径暴露给**包括 `airui-core` 在内的所有目标**；`:29-31` 的注释声称「core 绝不 include 平台头」，但构建系统完全不阻止。P0-1 的文本 grep 只能查字面量。
- **改法**：删掉全局 `include_directories`，按目标精确授权：

  ```cmake
  target_include_directories(airui-core PUBLIC
      ${CMAKE_SOURCE_DIR}/src/airui/core/include)
  target_include_directories(airui-win32 PUBLIC
      ${CMAKE_SOURCE_DIR}/src/airui/platform/win32
      ${CMAKE_SOURCE_DIR}/src/airui/core/include)
  target_include_directories(airecord PRIVATE
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_BINARY_DIR}/generated)
  ```

  `airui-skia` 单独拿 `platform/skia`；`third_party/sqlite` 只给 P2-2 抽出的 `sqlite3` 静态库。**注意**：`src/core/Plugin.h:7` 等头文件用的是 `"..."` 相对包含，改边界时要逐个确认 `#include "airui/..."` 的解析来源，别一次性全删导致 Windows 编译失败——分两次提交：先加各目标的 `target_include_directories`（保留全局），确认全平台编译通过后再删全局。

- **验收**：Linux / macOS 上 `cmake --build ... --target airui-core` 成功且编译命令里不含 `platform/win32`（用 `cmake --build --verbose` 检查）；临时在 core 里 `#include <windows.h>` 会**编译失败**（不只是 grep 失败）。
- **风险 / 回滚**：这是最有价值也最容易踩坑的一项，务必在 P0-1 生效后做。

### P2-2 业务层不再逐 exe 重复编译

- **证据**：`CMakeLists.txt:204-235` 用 `file(GLOB_RECURSE ...)` 把 `src/core/`、`ipc/`、`pages/`、`plugins/`、`agent/` 与 9.27 MB 的 `third_party/sqlite/sqlite3.c` 汇成 `COMMON_SOURCES`，直接塞进 `airecordd`（`:238`）、`airecord`（`:254`）及 20+ 个测试 target。`CMakeLists.txt:347/363/.../512` 共 15 处 `LINK_FLAGS "/SUBSYSTEM:CONSOLE"` **无 `if(MSVC)` 保护**（`:347`~`:512` 的 14 处 + `:711`；而 `:524-526` 等较新目标有），这才是 `ci.yml:45-51` 只能在 Linux 手列 5 个 target 的技术根因。
- **改法**：
  1. 抽 `add_library(app-common STATIC ${CORE_SOURCES} ${IPC_SOURCES} ${PAGES_SOURCES} ${PLUGINS_SOURCES} ${AGENT_SOURCES})` 与 `add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)`，exe 改为 `target_link_libraries(airecord PRIVATE app-common sqlite3 airui-win32 ...)`。`GLOB_RECURSE` 加 `CONFIGURE_DEPENDS` 或改回显式列表（推荐显式，避免新增文件不触发重配置）。
  2. 统一封装控制台目标，消除 15 处无保护链接选项：

     ```cmake
     function(airui_console_target name)
         if(MSVC)
             set_target_properties(${name} PROPERTIES
                 LINK_FLAGS "/SUBSYSTEM:CONSOLE")
         endif()
     endfunction()
     ```

     `cmake_minimum_required` 从 `3.10` 提到 `3.15`（README 已要求 3.15+）；若改用 `target_link_options`，同样需要 3.13+。
  3. 完成后把 `ci.yml:44-51` 从「手列 5 个 target」改成 `cmake --build build-core --config Release`（Linux 上 Win32 app 目标用 `EXCLUDE_FROM_ALL` 或 `if(WIN32)` 包起来），让两个平台跑**同一批平台无关测试**。

- **验收**：`cmake --build build-core`（Linux）成功且构建 20+ 个 core-only 测试；`ctest` 在 Linux 的用例数与 Windows 的差集只剩 Win32 条件目标；全量构建时间显著下降（sqlite 只编一次）。
- **风险 / 回滚**：目标链接关系变化较大，建议先在 Windows 上验证 `ctest 22/22` 再动 CI。

### P2-3 两个同名 `core` 与命名空间

- **证据**：`src/airui/core/` 是平台无关 UI 框架（`airui-core` 静态库）；`src/core/` 是业务应用层（`App.cpp` / `Database.cpp` / `Plugin.h`，`:7` 直接 include `windows.h`），不成库、无稳定接口。业务层符号全部裸全局（`App`、`Database`、`IFeaturePlugin`、`PluginManager`、`AgentHost`），而 UI 层用 `air::ui`，另有 `ipc` / `diag` / `machine` 等小命名空间。
- **改法**：**本批只做命名与文档澄清，不做大规模搬迁**：在 `docs/design-patterns.md` 明确写上「两个 core 的定义与边界」，并给业务层加 `namespace arecord {`（机械改动，但涉及所有 `src/pages/`、`src/plugins/` 的前向声明，建议单独 PR）。目录重命名 `src/core/` → `src/app/` 排到 v1.1 之后，因为会打断 `docs/` 里大量路径引用。
- **验收**：`docs/design-patterns.md` 有一节专门解释两者；新代码不再往全局命名空间加类。
- **风险 / 回滚**：命名空间改动面大，单独提交，编译失败即回滚。

### P2-4 消除无锁全局可变状态

- **证据**：`src/airui/core/src/Theme.cpp:138-139` 的 `g_palette` / `g_metrics` 是进程级可写全局，`SetPalette()` 与绘制线程无同步；`src/airui/platform/skia/airui/SkiaSurface.cpp:304-311` 的 `FrameSlot::busy` 是普通 `bool`，`AcquireSkiaFrame`（`:315-352`）/`ReleaseSkiaFrame`（`:354-373`）无任何锁或原子操作；`PaintFrame.cpp:17` 的 `g_current` 同理。
- **改法**：先明确并写进注释「换肤 / 开帧只允许 UI 线程调用」，再把 `busy` 改 `std::atomic<bool>` 并用 `compare_exchange_strong` 认领槽位；`g_palette` 若确实只在 UI 线程改，加 `assert(GetCurrentThreadId() == uiThreadId)` 式的调试约束（不要引入跨平台依赖，放 win32 层）。池帧析构路径（`:364` 只置 `busy=false`，`Impl` 的 DIB 常驻到进程退出）一并评估是否要显式 `ReleaseAllSkiaFrames()` 在 `WM_DESTROY` 调用。
- **验收**：多窗口 / 快速切页压力下 `test_paint_frame` 与 `test_paint_compare` 稳定通过；TSan 或人工审查确认唯一的写入线程。
- **风险 / 回滚**：单线程假设若被打破，改原子量只是止血，仍需重新设计帧所有权。

### P2-5 Win32 消息与返回值检查

- **证据**：`Shell.cpp:1219`、`Widgets.cpp:325-327`、`OverlayWindow.cpp:146-147` 三处 `WM_ERASEBKGND` 返回 1 却什么都不擦，正确性与「GDI 按 `rcPaint` 裁剪 / Skia 整窗缓冲」两套失效语义绑定；`GdiPainter.h:35/51/96/126/174` 与 `PaintFrame.cpp:222` 的 `CreateXxx` / `SaveDC` 不判 NULL / 0；`Ipc.cpp:27-31/38-40` 的 WideCharToMultiByte 首调用返回值 `n` 不检查，`n==0` 时 `&s[0]` 越界；`Database.cpp:67` DDL 失败未 `sqlite3_close`；`Gfx.h:64` 的 `UsePen` 复制构造函数把参数误写成 `UseBrush`（`= delete`），导致 `UsePen` 的复制被禁止而 `UseBrush` 的复制反而可写。
- **改法**：按「先修会崩的、再修风格」的顺序：
  1. `Gfx.h:64` 一行修正；`Ipc.cpp` 两个转换函数加 `if (n <= 0) return {};`；`Database.cpp:67` 失败分支补 `sqlite3_close(m_db); m_db = nullptr;`。
  2. 抽一个 `ScopedDC` 统一 `SaveDC`/`RestoreDC`（含返回值检查），替换三处手写；GDI 创建失败统一走现有 `gfx` RAII 路径（`Gfx.h:21-76` 已是正确的样板）。
  3. `WM_ERASEBKGND` 的「返回 1 不擦」在代码注释里写清**前置条件**（`WM_PAINT` 必须整窗覆盖），并补一个断言式自检：当 `partialPaint` 为真时不允许走抑制分支。这与 `docs/skia-backend.md` §4.2 的既有约定要对齐，**不要直接改成擦底**（会重新引入闪烁）。
- **验收**：`ctest` 全绿 + `scripts/probe_flash.py` 复测无闪烁回归；构造空字符串输入验证 `WideToUtf8` 不崩。
- **风险 / 回滚**：第 3 项涉及闪烁，必须用桌面抓图取证后再合入，切勿凭推理改。

---

## 5. P3 —— 测试、文档与清理

### P3-1 测试资产整理

- **状态（P3-1 已完成）**：11 个孤儿测试已按上述分类处理（8 个移到 `tools/diagnostics/`，3 个删除）；`test_font_lab` 已注明为手动排版诊断工具，未注册 ctest；`test_maximize.cpp` 的 FAIL 分支已返回非零；`CMakeLists.txt` 的 ctest 循环已改为对缺失目标报 `FATAL_ERROR`（Skia 条件目标白名单）。`ctest -N` 与 `ctest -C Release` 全绿。
- **证据**：`tests/` 下 37 个 `test_*.cpp`，其中 **11 个完全不被 CMake 引用**（`test_flash_screen` / `test_gallery_page_regression` / `test_gallery_shellcore_drawcalls` / `test_max_frames` / `test_maximize` / `test_restore_frames` / `test_scroll_diff` / `test_scroll_frames` / `test_shell_core` / `test_shell_gdi` / `test_win32_hal_integration`）；`test_font_lab` 在 `CMakeLists.txt:600` 被编译却不在 `:729` 的 ctest 列表里；`tests/test_maximize.cpp:84-85` 打印 `FAIL` 后恒 `return 0`；`:730` 的 `if(TARGET ${_t})` 会静默跳过不存在的目标，用例数悄悄减少。
- **改法**：11 个孤儿二选一——**移到 `tools/diagnostics/` 并加 README 说明启动前提**，或删除；`test_font_lab` 注册 ctest 或补注释说明为何是手动工具；`test_maximize.cpp:85` 改 `return failed ? 1 : 0;`；把 `:729` 的硬编码列表改成注册函数，或在循环末尾加 `if(NOT TARGET ${_t}) message(FATAL_ERROR "test target missing: ${_t}")`（Win32 / Skia 条件目标单独白名单）。
- **验收**：`ctest -N` 列出的用例数与预期一致；新增一个测试只需改一处；`git grep` 无未被引用的 `test_*.cpp`。

### P3-2 金样真正跨平台比对

- **状态（P3-2 已完成）**：`PaintRecorder` 新增整场景 `ToString()`（每 op 一行 ASCII，复用 per-op `PaintOp::ToString()`）；`test_paint_recorder` 现与提交基线 `tests/golden/paint_recorder.golden` 比对，仅当 `UPDATE_GOLDEN=1` 时才重写该文件（否则比对失败即非零退出）；`core-unix` / `windows` / `windows-x86` 三平台均运行 `test_paint_recorder`，各自与**同一份提交基线**比对，任一平台产出不同则其 required ctest 失败，无需单独的 artifact 下载 diff 作业。
- **证据**：`tests/test_paint_recorder.cpp:4-7` 与 `README.md:122` 声称「三平台产出同一份金样」，但 CI 各平台只是各自运行自比较（期望值硬编码在 `test_paint_recorder.cpp:79-92`），仓库无任何 `.golden` 文件，也没有跨产物比对步骤。
- **改法**：`PaintRecorder::ToString()` 结果落盘到 `tests/golden/paint_recorder.golden`，测试改为「与基线文件比对 + 显式 `UPDATE_GOLDEN=1` 才允许重写」；`core-unix` / `windows` / `windows-x86` 的 ctest 各自与同一份提交基线比对，平台差异即失败。
- **验收**：故意改一处 `PaintOp` 顺序，三平台 CI 同时失败并打印 diff。

### P3-3 文档修订清单（逐条）

- **状态（P3-3 已完成）**：本表逐条修订已在本批合入——`README.md` 定位与工具链措辞改为当前现实（本地推荐 VS2017 / CI 与 Skia 为 VS2022 / MSVC-only 禁 MinGW）、`-B build` → `-B build-cmake`、位数改为 x64、补注 `skia-windows` 非阻塞；硬性约定搬进 `docs/build-conventions.md`；`ROADMAP.md` 日期与 `ctest` 计数更新；`design-patterns.md` / `cross-platform.md` / `v0.8-baseline.md` / `toolbox-plugin-plan.md` 的事实修订落地；本表各「改为」列已与实际文档一致。

| 位置 | 现状 | 改为 |
|---|---|---|
| `README.md:3` | 「Windows 桌面性能录制与分析工具」（`docs/v0.8-baseline.md:23` 已判定过时） | 按当前定位改写：自绘 UI 框架 + 剪贴板工具箱宿主 |
| `README.md:7/11/39/131` | 「唯一 VS2017」「硬性约定」 | 「本地推荐 VS2017；CI 与 Skia 路径为 VS2022；完整应用需 MSVC，禁止 MinGW」；并把硬性约定从**未入库**的 `.workbuddy/skills/p-system-build/SKILL.md`（被 `.gitignore:41` 忽略）搬进 `docs/build-conventions.md` |
| `README.md:36` | `-B build`（第五套构建树） | `-B build-cmake`，与 `build.bat` 一致 |
| `README.md:85` | 「官方 `build/` 是 x86，请用 `build/vs2017-x64`」 | 「`build.bat` 固定 `-A x64`；CI Skia 插件为 x64；exe 与插件位数必须一致」；删除不存在的路径，同步修 `scripts/shot.py:92-93` |
| `README.md:111/124` | 「CI 每次 push/PR 都做整体编译」「build.bat 默认是 x86」 | 补注 `skia-windows` 非阻塞；位数改为 x64 |
| `docs/ROADMAP.md:4` | 最后更新 2026-11-07，正文已到 11-12 | 更新日期 |
| `docs/ROADMAP.md:22` | `ctest 17/17` | `22/22`（与同文件 `:419/452/495/507` 一致） |
| `docs/design-patterns.md:16-17` | 「在 CI 中强制校验（扫描 `#include` 与导入库）」 | P0-1 合入后再改为事实：「由 CI 的 Core cleanliness gate 步骤调用，扫描平台标识符字面量；编译层隔离见 P2-1」 |
| `docs/cross-platform.md:3` | 「草稿 v0.1（仅设计，未实施）」，但 `:8-13` 补记已完成 Phase 1 | 状态改为「设计稿 + Phase 1 已实施」，日期补记 |
| `docs/v0.8-baseline.md:10/32/33/34` | 精确行号引用已漂移（如 `App.cpp:443-444` 实为 `:612-613`） | 改为按**符号名**引用；或标注「行号以 tag `v0.8` 为准，不适用于 main」 |
| `docs/v0.8-baseline.md:201` | 「CI 5 个作业」 | 4 个 job（`core-unix` 的 matrix 产生 2 次运行） |
| `docs/toolbox-plugin-plan.md:412` | 要求 `check-tool-abi.sh PASSED`，该文件不存在 | 改为「P0-3 交付 `src/pluginapi/` 的 ABI 校验脚本后再启用」，或本批创建该脚本 |

### P3-4 仓库卫生

- **证据**：7 个 `gallery_*.txt` 手工对比输出被 git 跟踪；顶层大量被忽略的 `shot_*.png` / `test_output*.txt` / `*.log` 无归档目录；`third_party/sqlite/sqlite3.c`（9.27 MB）手工 vendored，无版本 / 来源 / 许可证说明。
- **改法**：`git rm --cached` 7 个 txt 并补 `.gitignore` 的 `gallery_*.txt`；顶层诊断产物统一落 `artifacts/`（加入忽略）；给 `third_party/sqlite/` 补 `README.md`（版本、下载来源、编译选项、许可证），P2-2 抽库后不再重复编译。
- **验收**：`git status --short` 干净；`git ls-files` 中无手工诊断输出。

### P3-5 检索式欠账标记

- **证据**：全库 `TODO` / `FIXME` / `HACK` **0 个**，但未完成工作藏在散文注释里：`Accessibility.cpp:23`「实际需要实现 IAccessible」、`Layout.cpp:182`「暂时用字体行高估算」、`gallery/main.cpp:139-140` 空回调占位、`docs/toolbox-plugin-plan.md:3`「待评审」。
- **改法**：把这些统一改成 `TODO(<领域>): ...` 格式并汇总进 `docs/ROADMAP.md` 的「未实现清单」；CI 加一步统计 TODO 数量（仅报告，不失败）。
- **验收**：`git grep -n "TODO("` 能一次列出所有已知欠账。

---

## 6. 验收清单

每批合入前按顺序执行（Windows 用 Git Bash，注意代理大小写撞键，configure/build 需 `env -u https_proxy -u http_proxy -u all_proxy -u no_proxy`）：

```bash
# 通用：仓库卫生
git diff --check
git status --short

# 门禁（P0-1 起，全部批次必跑）
bash scripts/check-core-clean.sh
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/check-core-clean.ps1
bash scripts/check-public-headers-clean.sh
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/check-public-headers-clean.ps1
bash scripts/check-core-linux-smoke.sh
bash scripts/check-skia-syntax.sh

# Windows 全量（ALL 里含 airui_skia_mock_syntax：
# MSVC + AIRUI_SKIA_MOCK 语法检查 Skia 后端的 _WIN32 分支）
cmake --build build-cmake --config Release
ctest --test-dir build-cmake -C Release --output-on-failure      # 默认 28/28

# Linux / macOS（P2-2 后应能全量构建）
cmake -S . -B build-linux -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --config Release
ctest --test-dir build-linux --output-on-failure

# 运行时回归（改动过绘制 / 浮层时）
#   scripts/probe_flash.py / scripts/screen_shot.py 复测无闪烁
#   AIRUI_FRAME_PROFILE=1 观察 frame_cost.log 无回退
```

**批次专项判据**：P0-2 需 ASan 下 20 次「启动→切页→关闭」无崩溃；P0-3 需用 `abi=1` 旧 DLL 验证拒绝回退；P0-5 需连续 50 次模态开关；P1-3 需故意让 Skia 测试失败验证合并被阻断；P3-2 需故意改 `PaintOp` 顺序验证三平台 diff 失败。

---

## 7. 需要补的决策记录（ADR）

按 P1-6 建立 `docs/adr/`，至少包含：

1. **工具链矩阵**：本地 `build.bat` / CI / Skia 路径分别用哪个生成器与位数，为什么 `windows-2022` 被 pin，「唯一支持」这类措辞如何退出。
2. **版本号策略**：`project(VERSION)` 为单一来源，工具箱序列（`v0.8` → `v0.9`）与历史 UI 页脚 `v1.3.1` 的关系与退役计划。
3. **服务 / UI 生命周期契约**：离线时谁存活、`pending.db` 归属与同步时机、SCM 失败操作、`STOPPING` 广播语义；把 `466b688` → `c209f7e` 的反转记为「旧决定被替代」。
4. **插件 ABI v2**：`size` / `caps` / `painterIfaceRev` 的定义、跨 DLL 字符串表示、发布顺序与兼容层退役时间。

---

## 8. 明确不在本方案范围

- **渲染像素与字体度量**：GDI/Skia 圆角覆盖率、ClearType 亚像素抗锯齿、金样像素差等实测结论（`docs/skia-backend.md` §4.2 / §5）保持不动；本方案只改「约束与流程」，不改绘制结果。
- **`v0.8` tag 与 `docs/v0.8-baseline.md` 的基线内容**：作为历史快照保留，只在 P3-3 修正指向 `main` 的失效引用。
- **业务模块**：文件监控 / 打印监控按用户排期最后做，不因本次整改提前。
- **平台移植**：Linux / macOS 应用本体与 Skia 版 `IWindow` 仍属路线图。
- **大规模目录搬迁**：`src/core/` → `src/app/` 延后到 v1.1 之后（P2-3 只做命名澄清）。
- 不引入新第三方依赖（除 P1-5 的 lint 工具与 P1-3 的 `vcpkg.json` 版本声明）。

---

## 9. 风险登记表

| 条目 | 主要风险 | 缓解 |
|---|---|---|
| P0-1 ② | 扩展 pattern 后历史注释误报 FAIL | 先只做 ①；② 前跑全量基线并列清单 |
| P0-2 | `RefreshStats` 重构牵动总览 / 配置页 | 单独提交；回滚时保留 join 修复 |
| P0-3 | 宿主与 CI 插件版本不同步 → 集体回退 gdi | 先发宿主兼容层，再发 DLL，最后删兼容层 |
| P2-1 | 删全局 include 后 Windows 编译失败 | 分两次提交：先加目标级、验证、再删全局 |
| P2-2 | 链接关系变化影响 20+ 测试目标 | 先在 Windows 验证 `22/22`，再改 CI |
| P2-5 ③ | 改擦底会重新引入闪烁 | 只加注释 / 自检，不改绘制策略；用抓图取证 |
| P1-1 | `git rm --cached` 影响他人本地文件 | PR 说明中写明，仅影响索引不影响磁盘 |

---

## 10. 建议的第一步

从 **P0-1 ①** 开始：只在 `ci.yml` 加 3 处步骤、不改任何 C++ 代码，一次提交即可让「CI 强制校验 core」这句文档陈述第一次成为事实。这一步零风险、见效最快，也是后面所有架构改动的安全网。随后按 `P0-4`（一行级修复）→ `P0-2` → `P0-5` → `P0-3` 推进。

完成 P0 后建议重新跑一次全量审查，确认「文档声称 ≠ 实际执行」的四层断层已闭合，再进入 P1/P2。
