#pragma once
// ============================================================
// AirUI — 总入口头文件
//
// 一个可直接复用的 Win32 自绘 UI 库（静态库，零运行时依赖）。
//
//   #include "airui/AirUi.h"
//
//   air::ui::Shell shell;
//   air::ui::ShellOptions opt;
//   opt.appName = L"MyApp";
//   shell.Create(hInstance, opt);
//   shell.AddPage(1, std::make_unique<MyPage>());
//   return shell.RunMessageLoop();
// ============================================================
#include "Theme.h"
#include "Gfx.h"
#include "Fonts.h"
#include "Page.h"
#include "Controls.h"
#include "Shell.h"
