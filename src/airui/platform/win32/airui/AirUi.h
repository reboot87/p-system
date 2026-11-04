#pragma once
// ============================================================
// AirUI — 总入口头文件
//
// 一个可直接复用的 Win32 自绘 UI 库（静态库，零运行时依赖）。
//
//   #include "airui/AirUi.h"
//
//   class HelloPage : public air::ui::Page {
//       const wchar_t* Title() const override { return L"首页"; }
//       void OnPaint(air::ui::Canvas& g, const RECT& rc) override {
//           g.FillRound(rc, Pal().surface, Mx().radiusLg);
//           g.TextRowCenter(rc, L"Hello AirUI", FontRole::Title, Pal().text);
//       }
//   };
//
//   air::ui::Shell shell;
//   air::ui::ShellOptions opt;
//   opt.appName = L"MyApp";
//   shell.Create(hInstance, opt);
//   shell.AddPage(1, std::make_unique<HelloPage>());
//   return shell.RunMessageLoop();
//
// 页面作者不需要接触 HDC / HFONT / HWND / DRAWITEMSTRUCT：
//   · 绘制    → Canvas（语义化字体 + 调色板）
//   · 控件    → CreateTextBox / CreateButton / CreateListBox / CreateCustom
//   · 定时器  → StartTimer / KillTimer
//   · 弹窗    → ShowMessageBox / ShowPopupMenu
// ============================================================
#include "airui/Theme.h"
#include "airui/Gfx.h"
#include "airui/Fonts.h"
#include "airui/Canvas.h"
#include "airui/Controls.h"
#include "airui/Widgets.h"
#include "airui/Page.h"
#include "airui/Shell.h"
