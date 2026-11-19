# 声明式布局（Android 模型）— 设计与进度

> 模块：`airui/Declarative.h`（公共头）+ `src/airui/core/src/DeclarativeLayout.cpp`（core，平台无关）。
> 测试：`tests/test_declarative_layout.cpp`（`ctest -R declarative`）。

## 1. 思路

照 Android 的 `View` / `ViewGroup` 模型做两件事：

1. **节点树代替手写 Rect**：界面是一棵 `LayoutNode` 树，容器决定子节点位置，不在页面代码里算坐标。
2. **两趟布局**：`Measure`（自底向上算尺寸，带 `MeasureSpec` 约束）→ `Layout`（自顶向下摆位置）。

和 Android 的对应关系：

| Android | AirUI |
|---|---|
| `LinearLayout(vertical/horizontal)` | `NodeType::Column` / `Row` |
| `FrameLayout` | `NodeType::Frame` |
| `GridLayout` | `NodeType::Grid`（`columns` 等宽） |
| `ScrollView` | `NodeType::Scroll` |
| `TextView` / `View` | `NodeType::Label` / `Control` |
| `layout_width=match_parent/wrap_content/48dp` | `Dim::Match() / Dim::Wrap() / Dim::Fixed(48)` |
| `layout_weight` | `LayoutNode::weight` |
| `MeasureSpec(Exactly/AtMost/Unspecified)` | `MeasureSpec` 同名同义 |
| `padding` / `layout_margin` | `padding` / `margin`（`LayoutMargins`） |
| `layout_gravity` | `align`（`LayoutAlign`：Start/Center/End/Stretch） |

## 2. 用法

```cpp
#include "airui/Declarative.h"
using namespace air::ui;

LayoutNode root(NodeType::Column);
root.SetPadding({16}).SetGap(12);

root.Add(NodeType::Label)
    .SetText(L"设置")
    .SetHeight(Dim::Fixed(24))
    .SetFont(FontRole::Title);

LayoutNode& row = root.Add(NodeType::Row).SetGap(8);
row.Add(NodeType::Control, "save")
   .SetStyle(L"primary")
   .SetIntrinsic({72, 32});
row.Add(NodeType::Spacer).SetWeight(1);
row.Add(NodeType::Control, "cancel")
   .SetIntrinsic({72, 32});

root.Add(NodeType::Scroll).SetWeight(1)   // 剩余高度给滚动区
    .Add(NodeType::Column).SetGap(6);

// 可选：注入真实文字测量（不注入则用 8px/字符、18px 行高的估算）
LayoutEnv env;
env.measureText = [](const std::wstring& s, FontRole) { return Size{ (int)s.size() * 9, 20 }; };

PrepareTree(root, Rect{0, 0, 640, 480}, env);   // measure + layout 一步到位

// 之后：
//   node.Frame()         -> 绘制矩形
//   node.ContentFrame()  -> 扣掉 padding 的内容区
//   node.Find("save")    -> 按 id 找节点（绑定/事件）
//   node.MeasuredWidth() -> 测量结果（未 Layout 时也有）
```

## 3. 语义约定（当前实现）

- **Dim**：`Fixed` 固定像素；`Wrap` 取内容 intrinsic，被 `AtMost` 截断；`Match` 填满父给的空间
  （分支轴上与 `weight=1` 等价）。
- **父给子约束**：容器给子节点的是 `AtMost`（不是 `Exactly`），所以 `Wrap` 子节点不会被拉满；
  `Exactly` 只用于根窗口。
- **weight**：主轴剩余空间按比例分配（`lround`，余数不补）。
- **Row / Column**：主轴依次排列 + `gap`；交叉轴按 `align` 对齐；
  `align=Stretch` 只拉伸 `Dim::Match` 的子节点（与 Android 一致）。
- **Frame**：`Match` 子节点铺满；其余按 `align` 对齐（水平与垂直用同一 `align`，当前简化）。
- **Grid**：`columns` 等宽；行高取该行最高子节点；父高度为 `Match/Fixed` 时行高均分撑满。
- **Scroll**：子节点按竖向堆叠、高度不设限；记录 `ScrollContentHeight()`，
  `SetScrollY()` 自动夹在 `[0, contentH - viewportH]`。
- **min/max**：`minW/minH/maxW/maxH` 在测量结果上收紧（`-1` 表示不限）。
- **Label/Control**：`intrinsic` 为空时用 `LayoutEnv` 测量（Label）或 `minW/minH`（Control）。

## 4. 当前进度

| 步骤 | 状态 |
|---|---|
| 1. 节点模型（`LayoutNode` + `LayoutParams` + `MeasureSpec`） | ✅ 已完成 |
| 2. measure/layout 两趟引擎（Row/Column/Frame/Grid/Scroll） | ✅ 已完成 |
| 3. JSON inflate（布局描述 → 节点树，错误带行列号） | ✅ 已完成 |
| 4. 绑定与交互（`ValueStore` 占位符、`HitTest`、`DispatchClick`） | ✅ 已完成 |
| 5. 渲染桥（`DeclarativeRender.h`，节点树 → `IPainter`） | ✅ 已完成 |
| 6. gallery 声明式示例页 + 热加载（`layouts/demo.json`） | ✅ 已完成 |

## 5. 已知限制

- 无文字换行（`Label` 单行；真实换行需要 `IPainter::TextBlock` 与后端测量注入）。
- `Frame` 只有一个 `align`（水平/垂直共用一个），需要独立 `gravity` 时再加字段。
- `Scroll` 仅竖向；横向滚动后续按同一模式扩展。
- 尚未和 `ControlHost` / `Control` 的命中与焦点联动（步骤 4）。

## 5.5 JSON 布局描述（步骤 3）

`InflateLayout(utf8Json, &err)` 把布局描述解析成节点树；失败返回 `nullptr`，
`err.Describe()` 给出 `"line:col: 原因"`（解析错误与语义错误都定位到出错 token）。

```jsonc
{
  "type": "column",                 // column/row/frame/grid/scroll/label/control/spacer
  "padding": [12, 16],              // 数字 | [vertical,horizontal] | [l,t,r,b] | {left,top,right,bottom}
  "gap": 10,
  "children": [
    { "type": "label", "id": "title",
      "text": "{title}",            // UTF-8；支持 \uXXXX 与代理对；{key} 是绑定占位符
      "font": "title",              // body/meta/title/section/button/icon/icon_large/display
      "h": 24 },                    // 数字=固定；"wrap"/"match"；别名 width/height 亦可
    { "type": "row", "gap": 8, "onClick": "rowCommand", "children": [
        { "type": "control", "id": "save", "style": "primary",
          "w": 72, "h": 32, "onClick": "doSave" },
        { "type": "spacer", "weight": 1 } ] },
    { "type": "grid", "columns": 2, "gap": 6, "children": [
        { "type": "label", "text": "a" },
        { "type": "label", "text": "b" } ] }
  ]
}
```

- 其余字段：`minW/minH/maxW/maxH`、`align`（start/center/end/stretch）、`preferred: [w,h]`。
- 未知字段忽略（向前兼容）；未知 type/align/font、类型不符的值 → 带行列号的错误。

## 5.6 绑定与交互（步骤 4）

```cpp
ValueStore store;
store.Set("title", L"主页").Set("count", 42);
store.Apply(*root);                       // 把每个节点 text 里的 {key} 填充掉

LayoutNode* hit = HitTest(*root, Point{40, 16});   // 最深节点；Frame 后加入者在上层
DispatchClick(*root, Point{40, 16}, [](LayoutNode& n, const std::string& cmd) {
    // cmd == n.onClick；沿父链上溯到第一个非空 onClick 才触发
});
```

- `HitTest`：Scroll 开 `clip` 时视口外的点不命中内容（点落在 Scroll 容器或其外层）。
- `DispatchClick`：整条父链都没有 `onClick` 就不回调（纯展示控件不产生命令）。

## 5.7 渲染桥（步骤 5）

```cpp
#include "airui/DeclarativeRender.h"
RenderTree(g, *root, Pal(), Mx());        // GDI / Skia / PaintRecorder 同一条路径
```

节点映射：Label → `IPainter::Text`（font + align）；Control → `PaintButton`
（`style`: primary/danger/ghost/secondary）；Scroll → `PushClip(视口)/PopClip`；
容器默认无视觉（`RenderOptions::debugBoxes` 可画调试边框）。
测试用 `PaintRecorder` 断言绘制调用序列（平台无关，跨后端一致）。

## 5.8 gallery 示例页与热加载（步骤 6）

`airui_gallery` 导航「声明式 → DeclarativeJson」：整块区域由 JSON 描述，演示
绑定（`{clicks}` 计数）、命令（`+1` / `清零` / `回到顶部`）、滚轮滚动与视口裁剪。

- **热加载**：`<exe目录>\layouts\demo.json` 存在时优先于内置布局，按文件
  最后写入时间自动重载——改 JSON 保存即见效果。目录由 `main.cpp` 经
  `extragallery::LayoutDir()` 注入，demo 本身只用 `<filesystem>`（保持 core 可编译）。
  示例布局已随仓库提交：`src/gallery/layouts/demo.json`，把它复制到 exe 旁的
  `layouts\` 即生效（构建后是 `build\Release\layouts\`）。
- **模板/实例分离**：`declTemplate` 是解析出的纯净树；每帧 `CloneTree` 一份、
  `ValueStore::Apply` 填绑定、`PrepareTree` + `RenderTree`。占位符只存在于模板，
  渲染结果不会把 `{key}` 画出去。
- **失败兜底**：坏 JSON 保留上一份好布局继续渲染，状态行红字显示
  `"line:col: 原因"`；坏文件记录时间戳避免每帧重解析。
- 集成测试 `test_declarative_gallery`（core-only）无头驱动整个 demo：
  首帧绘制、计数重绘、命令分发、滚轮夹紧、外部文件替换/热重载/错误兜底。

## 6. 为什么先做引擎

- **可单测**：纯整数几何，不依赖窗口/后端，`ctest` 直接断言矩形（`Wrap/Match/Fixed`、
  weight、padding/margin、grid、scroll、clamp 共 28 条断言）。
- **平台无关**：仍在 `airui-core`，`check-core-clean` 门禁通过；GDI / Skia 输出一致。
- **为步骤 3–5 提供稳定地基**：JSON inflate 只负责建树，渲染只负责遍历绘图。

## 7. 验证

```powershell
cmake -S . -B build
cmake --build build --config Release
cd build; ctest -C Release          # 26/26（含 declarative layout/json/interact/render 四组）
cmake --build build --config Release --target check-public-headers
```
