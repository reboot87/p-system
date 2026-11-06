# AirUI 设计模式地图（Design Patterns）

本文档盘点 `airui-core` 与画廊（gallery）实际使用的设计模式，重点说明
**为什么这样设计**以及**如何在新增控件时遵循既有约定**。代码以事实为准，
本章内容来自对 `src/airui/core`、`src/airui/platform/win32` 与
`src/gallery/ExtraGallery.h` 的核对。

---

## 1. 分层与边界（核心约束）

`airui-core` 是**平台无关**的：只依赖 `IPainter` / `Rect` / `Palette` /
`Metrics` 等抽象类型，绝不 `#include` `windows.h`、绝不链接 `user32` /
`gdi32`。平台相关逻辑全部收口到 `src/airui/platform/win32` 薄后端。

这一边界由 `scripts/check-core-clean.{ps1,sh}` 在 CI 中强制校验（扫描
`#include` 与导入库）。任何新代码都要先问一句：**它属于 core 还是平台层？**

---

## 2. airui-core 中的模式

### 2.1 Bridge（桥接）—— 绘制与后端解耦
- `IPainter`（core 抽象）是绘制接口；`Canvas`（Win32 后端）是它的具体实现。
- 所有 `Paint*` 纯函数只接受 `IPainter&`，因此同一套绘制逻辑既可在桌面
  GDI 后端、也可在无头测试 / 截图后端运行。
- **如何遵循**：新增绘制函数一律以 `IPainter&` 为第一参数，禁止直接调用
  平台 API。

### 2.2 Composite（组合）—— 控件树与事件派发
- `ControlHost` 持有 `vector<unique_ptr<Control>> m_controls`，按 z 序递归
  完成 `PaintAll` / `DispatchPointer` / `HitControl`。
- 每个 `Control` 是组合节点，既可以是叶子（Button）也可以是容器
  （嵌套 `ControlHost`），命中检测与绘制均沿树下行。
- **如何遵循**：把“容器 vs 叶子”的差异交给 `Control` 的多态，而不是在调用方
  写 `if (isContainer)`。

### 2.3 Template Method（模板方法）—— Control 基类骨架
- `Control` 基类定义稳定骨架：`SetBounds` / `SetTabIndex` / `Focus` /
  `Paint` 等；子类只覆写 `Paint` / `OnPointer` / `OnKey` / `AccessibleText`。
- 焦点遍历、Tab 序、z-order 命中这些**不变流程**留在基类，变化点下沉到虚函数。

### 2.4 Strategy（策略）—— 可替换的能力
- `IClock`（动画时间源）、`IClipboard`（剪贴板）以接口注入，核心逻辑不依赖
  具体实现；测试时可替换为假实现。
- 与 Bridge 的区别：Bridge 解耦**实现维度**（渲染后端），Strategy 解耦
  **算法/策略维度**（用什么时钟、什么剪贴板）。

### 2.5 Command（命令）—— 事件结果回传
- 指针/键盘事件处理统一返回 `ControlResult`，其中携带 `action` 字段（如
  “提交”“取消”）。控件不直接执行副作用，而是把“意图”作为命令对象回传，
  由上层决定如何处理。
- 画廊的 `DemoEntry` 也是命令模式的一种：每个控件把 Paint/Click/Wheel
  封成一个可执行单元（见第 3 节）。

### 2.6 Registry / 表驱动（注册表）—— 控件清单与查表
- 类体系通过 `Defs()` 返回 `vector<ExtraDef>` 描述全部控件；
- 画廊（`ExtraGallery.h`）的 `Demos()` 返回 `vector<DemoEntry>`，按索引
  查表得到某个控件的渲染/点击/滚轮行为。
- 索引即句柄：导航、命中、滚动都用 `idx` 在同一张表上查。

### 2.7 Flyweight（享元）—— 调色板与度量共享
- `Palette`（语义色板）与 `Metrics`（间距/圆角/字号）是跨所有控件共享的
  **只读**对象，不在每个控件里拷贝一份，避免“改一处颜色要改 80 个地方”。

### 2.8 Adapter（适配器）—— 类体系转发层
- `ControlsClasses.cpp` 中的 `widgets::*` 类是对 212 个 core 自由函数内核的
  适配层：类的 `Paint` 转发回 `PaintButton` 等自由函数，**零重写绘制逻辑**，
  只是把“C 风格函数 + 独立状态结构体”包成对象。

### 2.9 Externalized State（外置状态）—— XxxState 与对象分离
- 早期每个控件是一个 `PaintXxx(g, rc, state, ...)` 自由函数，状态结构
  `XxxState` 由调用方持有。
- 类体系重构后，状态内聚进 `widgets::Xxx` 对象自身（`s.wButton.state` …），
  调用方不再持有散落的 `XxxState`。

### 2.10 Observer（观察者）—— 窗口回调
- `IWindow` 通过 `std::function` 回调把“尺寸变化 / 关闭 / 焦点变化”等事件
  通知外部，核心不直接耦合具体窗口管理逻辑。

---

## 3. 画廊重构：从「四份并行 switch」到「单一注册表」

### 3.1 问题（违反开闭原则 OCP）
重构前，`src/gallery/ExtraGallery.h` 里每新增一个控件要改 **4 处**：

1. `Defs()` 里的元数据列表（`ExtraDef`）；
2. `DrawExtraControl` 里的 `switch (idx)`（绘制）；
3. `ExtraClick` 里的 `switch (idx)`（点击）；
4. `ExtraWheel` 里的 `switch (idx)`（滚轮）。

四个 `switch` 用同一枚 `idx` 做分支，**每加一个控件就要在四个地方同步插入**，
且 `idx` 与各 `case` 的对应关系靠人工维护，极易错位。这正是典型的 OCP 违反：
对“新增控件”这一变化，已有代码需要被反复修改。

### 3.2 方案：Command + Registry（工厂/注册表）
把每个控件描述为一个 `DemoEntry`：

```cpp
struct DemoEntry {
    const wchar_t* group;   // 元信息
    const wchar_t* label;
    int  height;
    DemoPaintFn  paint;     // 命令：绘制
    DemoClickFn  click;     // 命令：点击（可空）
    DemoWheelFn  wheel;     // 命令：滚轮（可空）
};

inline const std::vector<DemoEntry>& Demos() {
    static const std::vector<DemoEntry> demos = {
        { L"P0 框架", L"Framework", 120, DemoPaint_Framework,  DemoClick_Framework,  nullptr },
        { L"P0 框架", L"ScrollVirtual", 190, DemoPaint_ScrollVirtual, DemoClick_VirtualList, DemoWheel_VirtualList },
        // ……一个控件 = 一行注册……
    };
    return demos;
}
```

- 每个控件的 **元信息 + 三种行为** 收拢到**同一个注册表条目**里；
- `Defs()` 与 `Count()` 不再各自维护一份列表，而是**从 `Demos()` 派生**，
  全库只有一份“真相来源”；
- 三个 dispatch 函数退化为**查表**，不再有 `switch`：

```cpp
inline void DrawExtraControl(IPainter& g, const Rect& rc, int idx,
                             ExtraDemoState& s, const Palette& pal, const Metrics& mx) {
    if (rc.Empty()) return;
    const auto& v = Demos();
    if (idx < 0 || idx >= static_cast<int>(v.size())) return;
    if (v[idx].paint) v[idx].paint(g, rc, s, pal, mx);
}
```

### 3.3 收益
| 维度 | 重构前 | 重构后 |
| --- | --- | --- |
| 新增控件改动点 | 4 处（元数据 + 3 个 switch） | **1 处**（注册表加一行） |
| 行为归属 | 分散在 3 个 `switch` 的 `case` 中 | 集中在单个 `DemoEntry` |
| 缺漏风险 | 漏改任一 `switch` 即错位 | 编译期即暴露（缺条目） |
| 元信息来源 | 独立维护一份 `Defs()` | 由 `Demos()` 派生，单一真相 |

### 3.4 现在如何新增一个控件
1. 在 `Demos()` 注册表里追加一行 `{ group, label, height, paintFn, clickFn, wheelFn }`；
2. 实现对应的 `DemoPaint_Xxx` / `DemoClick_Xxx` / `DemoWheel_Xxx` 自由函数
   （简单绘制可用 `DEMO_PAINT` 宏一键生成）；
3. 无需再动 `Defs()`、`Count()`、任何 `switch`。

---

## 4. 约定速查
- 画绘制函数 → 第一参 `IPainter&`，不碰平台 API（Bridge）。
- 控件对象 → 状态内聚、虚 `Paint`/`OnPointer`（Template Method + Externalized State）。
- 可替换能力 → 用 `IClock`/`IClipboard` 注入（Strategy）。
- 控件清单 → 走 `Demos()` 注册表，单一来源（Registry）。
- 事件副作用 → 用 `ControlResult.action` 回传意图（Command）。
- 跨控件共享 → 颜色走 `Palette`、间距走 `Metrics`（Flyweight）。

---

## 5. 仓库内的活示例（可直接照抄）
本仓库已内置两条扩展路径的真实样例，均按上述模式实现并在画廊 P6 分组演示：

### 5.1 新增控件（从零）：`widgets::RatingBar`
- 声明：`src/airui/core/include/airui/ControlsCoreClasses.h`（`class RatingBar : public Control`）
- 绘制：`src/airui/core/src/ControlsCore.cpp`（`RatingBar::Paint`，自己画星 + 点击映射分值）
- 外置状态：`RatingBarState` 结构 + 自由 shim `PaintRatingBar`
  （`ControlsCore.h` 声明、`ControlsClasses.cpp` 实现，保持源码兼容）
- 画廊注册：`ExtraGallery.h` 的 `Demos()` 加一行
  `{ L"P6 类体系", L"RatingBar (新增)", 60, paintλ, clickλ, nullptr }`
- 要点：自带 `OnPointer`/`OnKey`，把指针/键盘命中换算成 `value`，零复用既有内核。

### 5.2 继承现有控件：`widgets::IconButton`
- 声明：`ControlsCoreClasses.h`（`class IconButton : public Button`）
- 绘制：`ControlsCore.cpp`（`IconButton::Paint` → 先 `Button::Paint(...)` 复用父类外观，再叠加前导图标）
- 自由 shim：`PaintIconButton`（同上文件）
- 画廊注册：`Demos()` 加一行
  `{ L"P6 类体系", L"IconButton (继承)", 60, paintλ, nullptr, nullptr }`
- 要点：**只 override `Paint` 并先调 `Base::Paint`**，事件/焦点/命中全部继承父类，改动最小。

> 两条路都不是“改 4 处”的旧模式：控件逻辑收敛到类，画廊只要一行注册即可。

