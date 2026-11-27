# ADR-0004: 插件 ABI v2（`size` / `caps` / `painterIfaceRev`）

- **状态**：生效
- **日期**：2026-11-15
- **关联**：`src/pluginapi/airui_plugin_abi.h`、`src/airui/platform/skia/airui/SkiaPluginAbi.h`、`src/airui/platform/skia/airui/SkiaPlugin.cpp`、`src/airui/platform/win32/PaintProviders.cpp`、提交 `1a1c0c8`

## 背景

旧 ABI（`AIRUI_SKIA_PLUGIN_ABI 1`）只声明 5 个 C 函数形状，跨模块仍通过 `IPainter*` 虚表把 `const std::wstring&` 传给插件，且 `PaintProviders.cpp` 注释承认「MSVC vtable/CRT layout matches across toolsets」。改 `IPainter` 虚函数集时 ABI 号不变，旧 DLL 会被当作兼容加载 → 难以定位的崩溃（P0-3）。同时 ABI 头声明与 `docs/toolbox-plugin-plan.md`「零 STL 跨界」矛盾。

## 决定（ABI v2，来自 `1a1c0c8` 与 `src/pluginapi/`）

- **版本化描述结构**：新 ABI 头放 `src/pluginapi/airui_plugin_abi.h`（core 之外的独立目录），定义 `AiruiPluginDesc`：
  ```c
  #define AIRUI_PLUGIN_ABI 2
  #define AIRUI_PAINTER_IFACE_REV 2
  typedef struct AiruiPluginDesc {
      uint32_t size;             // sizeof(AiruiPluginDesc)，向前兼容
      uint32_t abi;              // AIRUI_PLUGIN_ABI
      uint32_t caps;             // AIRUI_CAP_* 位掩码
      uint32_t painterIfaceRev;  // AIRUI_PAINTER_IFACE_REV
  } AiruiPluginDesc;
  ```
  宿主经 `airui_skia_describe` 取到静态描述后，校验 `size`、`abi`、`painterIfaceRev` 三者全部匹配才构造 painter；任一不符即拒绝 DLL，而不是当兼容加载。`painterIfaceRev` 独立于 C ABI 号，让宿主能在 C ABI 号未变时也拒绝 vtable 布局不同的插件。
- **跨 DLL 字符串表示**：`IPainter::Text` / `TextBlock` / `MeasureText` 从 `const std::wstring&` 改为 `const wchar_t* + size_t len`，杜绝 STL/CRT 跨模块传递；同步改 4 处转发（`PaintProviders.cpp`、`SkiaSurface.cpp` 两处、`GdiPainter.h` 宏转发）与全部调用点。
- **发布顺序**：先发宿主兼容层（能识别新旧两种 ABI），再发新 DLL，最后删兼容层，避免宿主与 DLL 版本不同步导致集体回退 gdi（见整改批次顺序）。

## 后果

- 收益：ABI 不匹配在加载期被拒，而不是运行期崩溃；跨模块只有纯 C 函数指针形状 + 宽字符指针，真正「零 STL 跨界」。
- 代价：改 `IPainter` 虚函数集必须同步 bump `AIRUI_PAINTER_IFACE_REV`（与 C ABI 号一起）；文本接口从 `std::wstring` 改为指针 + 长度，是破坏性变更，需与 ABI v2 同批发布；要按「先兼容层、再 DLL、后删兼容层」的顺序走完过渡期。

## 被替代的旧决定

- 旧 ABI v1（`AIRUI_SKIA_PLUGIN_ABI 1`）：只声明 5 个函数形状、跨模块传 `const std::wstring&`、无 `size`/`caps`/`painterIfaceRev` 版本化描述，改接口不 bump ABI 号。被本 ADR 的 ABI v2 取代。

## 后续修订

- **2026-11-27 · `AIRUI_PAINTER_IFACE_REV` 2 → 3**：`IPainter` 在
  `Translate` 之后追加 `HLine` / `HLineDashed` / `VLine` / `Dot` /
  `ProgressBar` / `LineHeight`（均带默认实现，既有实现者与测试替身无需
  改动），用于让页面绘制入口 `Canvas` 不再直接调用平台原生绘制助手。
  C ABI 号（`AIRUI_PLUGIN_ABI 2`）未变；按本 ADR 的约定，vtable 布局变化
  只 bump `painterIfaceRev`，因此 rev 2 的 DLL 会被宿主拒绝。当前取值以
  `src/pluginapi/airui_plugin_abi.h` 为准。
