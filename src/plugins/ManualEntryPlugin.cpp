#include "ManualEntryPlugin.h"
#include "../core/App.h"
#include "../ipc/Ipc.h"
#include <commctrl.h>

// ---------- EDIT 子类化：捕获 Ctrl+Enter ----------
static LRESULT CALLBACK ManualEditProc(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam,
                                       UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* app = (App*)dwRefData;

    if (msg == WM_KEYDOWN && wParam == VK_RETURN &&
        (GetKeyState(VK_CONTROL) & 0x8000))
    {
        if (ManualEntryPlugin::SaveFromManual(*app))
            return 0; // 已处理，不插入换行
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ---------- 插件挂载：子类化录入框以捕获 Ctrl+Enter ----------
bool ManualEntryPlugin::OnAttach(App& app)
{
    SetWindowSubclass(app.ManualEdit(), &ManualEditProc, 1, (DWORD_PTR)&app);
    return true;
}

bool ManualEntryPlugin::SaveFromManual(App& app)
{
    // 编辑态：按钮/Ctrl+Enter 此时保存的是备注修改，而不是新增记录
    if (app.IsEditingNote()) {
        app.SaveNoteEdit();
        return true;
    }

    std::wstring text = App::GetWindowTextStr(app.ManualEdit());
    std::wstring note = App::GetWindowTextStr(app.NoteEdit()); // 备注（可选）
    // 校验非空（纯空白也视为空）
    bool hasContent = false;
    for (wchar_t c : text)
        if (!iswspace(c)) { hasContent = true; break; }
    if (!hasContent) {
        SetFocus(app.ManualEdit());
        app.Toast(L"内容不能为空", L"请输入要保存的内容后再试", false);
        return false;
    }

    // 通过 IPC 请求服务端保存（source=Manual，附带备注）
    std::wstring resp = app.Ipc().Request(
        L"SAVE\tManual\t" + ipc::EscapeField(note) +
        L"\t" + ipc::EscapeField(text));
    if (resp.rfind(L"OK", 0) == 0) {
        SetWindowTextW(app.ManualEdit(), L""); // 清空录入框和备注框
        SetWindowTextW(app.NoteEdit(), L"");
        app.ToggleForm(false);                 // 保存成功 → 收起表单卡
        // 新增记录会改变总数 / 侧栏角标 / 总览页数字，不能只刷列表
        app.RefreshData();
        app.Toast(L"保存成功", L"记录已写入本地数据库");
        return true;
    }
    app.Toast(L"保存失败", L"服务未响应，请稍后重试", false);
    return false;
}

void ManualEntryPlugin::OnCommand(App& app, int ctrlId, int notifyCode)
{
    if (ctrlId == IDC_SAVE_BUTTON && notifyCode == BN_CLICKED)
        SaveFromManual(app);
}

void ManualEntryPlugin::OnDetach(App& app)
{
    RemoveWindowSubclass(app.ManualEdit(), &ManualEditProc, 1);
}
