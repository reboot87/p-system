// test_ui.c — airui 重构回归测试（消息驱动，只读验证）
// 1. 枚举主窗口子控件（Widget 是否创建）
// 2. 切到剪贴板页 → 列表条数（IPC + ListBox 装填）
// 3. 读搜索框文本（TextBox WM_GETTEXT）
// 4. 客户区截图存 BMP（视觉验证）
#include <windows.h>
#include <stdio.h>
#include <string>

static FILE* g_log;

static void Log(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vfprintf(g_log ? g_log : stdout, fmt, ap);
    fprintf(g_log ? g_log : stdout, "\n");
    va_end(ap);
    fflush(stdout);
}

static BOOL CALLBACK ChildCb(HWND h, LPARAM lp)
{
    wchar_t cls[64] = L"", txt[128] = L"";
    GetClassNameW(h, cls, 64);
    GetWindowTextW(h, txt, 128);
    RECT rc; GetWindowRect(h, &rc);
    MapWindowPoints(HWND_DESKTOP, (HWND)lp, (LPPOINT)&rc, 2);
    Log("  child: cls=%ls id=%d vis=%d rect=(%ld,%ld,%ld,%ld) text=[%ls]",
        cls, GetDlgCtrlID(h), IsWindowVisible(h) ? 1 : 0,
        rc.left, rc.top, rc.right, rc.bottom, txt);
    return TRUE;
}

static void Dump(HWND w, const char* tag)
{
    Log("[%s] children:", tag);
    EnumChildWindows(w, ChildCb, (LPARAM)w);
}

static void Click(HWND w, int x, int y)
{
    PostMessageW(w, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    PostMessageW(w, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
}

static int ListCount(HWND w)
{
    HWND list = NULL;
    EnumChildWindows(w, [](HWND h, LPARAM lp) -> BOOL {
        wchar_t cls[64]; GetClassNameW(h, cls, 64);
        if (_wcsicmp(cls, L"ListBox") == 0) { *(HWND*)lp = h; return FALSE; }
        return TRUE;
    }, (LPARAM)&list);
    return list ? (int)SendMessageW(list, LB_GETCOUNT, 0, 0) : -1;
}

// 便捷：按 类名+id 找子控件
struct FindCtx { const wchar_t* cls; int id; HWND found; };

static BOOL CALLBACK FindCb(HWND h, LPARAM lp)
{
    auto* ctx = (FindCtx*)lp;
    wchar_t cls[64]; GetClassNameW(h, cls, 64);
    if (_wcsicmp(cls, ctx->cls) == 0 && GetDlgCtrlID(h) == ctx->id) {
        ctx->found = h;
        return FALSE;
    }
    return TRUE;
}

static HWND Ctrl(HWND w, const wchar_t* cls, int id)
{
    FindCtx ctx = { cls, id, nullptr };
    EnumChildWindows(w, FindCb, (LPARAM)&ctx);
    return ctx.found;
}

// 逐字符键入WM_CHAR 模拟真实输入
// （跨进程 SetWindowTextW 打到多重子类化的 EDIT 上不生效，只能用键入路径）
static void TypeText(HWND w, int id, const wchar_t* s)
{
    HWND h = Ctrl(w, L"Edit", id);
    if (!h) return;
    for (; *s; ++s) SendMessageW(h, WM_CHAR, (WPARAM)*s, 1);
}

// ---- IPC 直连（UTF-8，一行请求 → 读到连接关闭） ----
static std::string IpcReq(const std::string& req)
{
    HANDLE h = CreateFileW(L"\\\\.\\pipe\\AireRecordIpc",
                           GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return "";
    // 服务端按 '\n' 切分命令：请求必须带行尾，否则双方互等（ReadFile 阻塞）
    std::string line = req + "\n";
    DWORD written = 0;
    WriteFile(h, line.data(), (DWORD)line.size(), &written, nullptr);
    std::string resp;
    char buf[4096]; DWORD n = 0;
    while (ReadFile(h, buf, sizeof(buf), &n, nullptr) && n > 0)
        resp.append(buf, n);
    CloseHandle(h);
    return resp;
}

// LIST 响应第一行的 id 字段
static std::string FirstIdOf(const std::string& resp)
{
    size_t p1 = resp.find('\n');
    if (p1 == std::string::npos) return "";
    size_t p2 = resp.find('\t', p1);
    if (p2 == std::string::npos) return "";
    return resp.substr(p1 + 1, p2 - p1 - 1);
}

static void Shot(HWND w, const wchar_t* path)
{
    RECT rc; GetClientRect(w, &rc);
    int W = rc.right, H = rc.bottom;
    HDC wdc = GetDC(w);
    HDC mem = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    PrintWindow(w, mem, 3 /* PW_CLIENTONLY | PW_RENDERFULLCONTENT */);
    SelectObject(mem, old);   // GetDIBits 要求位图未选入 DC

    // 32bpp 兼容位图：信息头带 3 个 BITFIELDS 掩码槽，避免 GetDIBits 越界
    struct BI {
        BITMAPINFOHEADER h;
        DWORD masks[3];
    } bi = {};
    bi.h.biSize = sizeof(BITMAPINFOHEADER);

    int bpp = 32;
    int rowBytes = W * 4;
    BYTE* bits = (BYTE*)malloc(rowBytes * H);
    bi.h.biWidth = W;
    bi.h.biHeight = -H;
    bi.h.biPlanes = 1;
    bi.h.biBitCount = bpp;
    bi.h.biCompression = BI_RGB;
    int got = GetDIBits(wdc, bmp, 0, H, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    BITMAPFILEHEADER fh = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + rowBytes * H;
    FILE* f = _wfopen(path, L"wb");
    if (f && got == H) {
        fwrite(&fh, 1, sizeof(fh), f);
        fwrite(&bi.h, 1, sizeof(BITMAPINFOHEADER), f);
        fwrite(bits, 1, rowBytes * H, f);
        fclose(f);
    } else if (f) {
        fclose(f);
    }
    free(bits);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(w, wdc);
    Log("shot -> %ls (%dx%d, rows=%d)", path, W, H, got);
}

int main(void)
{
    g_log = stdout;

    HWND w = FindWindowW(L"AireRecordWnd", NULL);
    if (!w) {
        Log("FAIL: no AireRecordWnd — enumerating top-level windows:");
        EnumWindows([](HWND h, LPARAM) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(h, &pid);
            wchar_t cls[64] = L"", txt[128] = L"";
            GetClassNameW(h, cls, 64);
            GetWindowTextW(h, txt, 128);
            if (txt[0] || wcsstr(cls, L"Aire"))
                Log("  hwnd=%p pid=%lu cls=%ls title=[%ls]",
                    (void*)h, (unsigned long)pid, cls, txt);
            return TRUE;
        }, 0);
        return 1;
    }
    Log("PASS: main window found (hwnd=%p)", (void*)w);

    Dump(w, "overview(default)");

    // ---- 切到剪贴板页（侧栏第二项） ----
    Click(w, 100, 126);
    Sleep(700);
    Dump(w, "clips");

    int cnt = ListCount(w);
    Log(cnt > 0 ? "PASS: list count = %d" : "WARN: list count = %d", cnt);

    // 搜索框文本（占位提示读不到是正常的，读真实文本）
    HWND search = NULL;
    EnumChildWindows(w, [](HWND h, LPARAM lp) -> BOOL {
        wchar_t cls[64]; GetClassNameW(h, cls, 64);
        int id = GetDlgCtrlID(h);
        if (wcscmp(cls, L"Edit") == 0 && id == 1001) { *(HWND*)lp = h; return FALSE; }
        return TRUE;
    }, (LPARAM)&search);
    if (search) {
        wchar_t buf[128] = L"";
        SendMessageW(search, WM_GETTEXT, 128, (LPARAM)buf);
        Log("search text = [%ls]", buf);
        Log("PASS: search TextBox reachable");
    } else {
        Log("FAIL: search edit not found");
    }

    Shot(w, L"C:\\Users\\Administrator\\AppData\\Local\\Temp\\airui_clips.bmp");
    Log("checkpoint: clips shot done");

    // ============================================================
    // 功能链路一：手动录入表单 → 保存按钮（插件 BN_CLICKED）→ IPC 落库
    // ============================================================
    int cntBefore = cnt;

    // 点击「＋ 手动录入」
    Click(w, 1044, 76);
    Sleep(500);
    if (IsWindowVisible(Ctrl(w, L"Edit", 1003)))
        Log("PASS: form card opened (manual edit visible)");
    else
        Log("FAIL: form card did not open");

    TypeText(w, 1003, L"airui2.0-save-check");
    TypeText(w, 1005, L"refactor-regression");

    // WM_COMMAND(BN_CLICKED, IDC_SAVE_BUTTON) → 插件保存链路
    SendMessageW(w, WM_COMMAND, 1004, (LPARAM)Ctrl(w, L"Button", 1004));
    Sleep(900);

    if (!IsWindowVisible(Ctrl(w, L"Edit", 1003)))
        Log("PASS: form closed after save (plugin returned success)");
    else
        Log("FAIL: form still open — save likely failed");

    // IPC 校验落库并清理
    {
        std::string resp = IpcReq("LIST\tairui2.0-save-check");
        std::string id = FirstIdOf(resp);
        if (resp.compare(0, 2, "OK") == 0 && !id.empty()) {
            Log("PASS: entry saved via IPC (id=%s)", id.c_str());
            std::string del = IpcReq("DEL\t" + id);
            Log(del.compare(0, 2, "OK") == 0 ? "PASS: cleanup DEL ok"
                                             : "FAIL: cleanup DEL failed");
        } else {
            Log("FAIL: saved entry not found via IPC");
        }
    }
    Sleep(500);
    int cntAfter = ListCount(w);
    Log(cntAfter == cntBefore ? "PASS: list restored (%d rows)"
                              : "WARN: list count %d -> %d",
        cntBefore, cntAfter);

    // ============================================================
    // 功能链路二：详情面板「备注」→ 就地编辑 → 保存 → 还原
    // ============================================================
    // 记住当前选中条目（列表默认选第一条）与其原备注
    std::string full = IpcReq("LIST\t");
    std::string selId, origNote;
    {
        size_t p1 = full.find('\n');
        size_t p2 = full.find('\t', p1);
        size_t p3 = full.find('\t', p2 + 1);
        size_t p4 = full.find('\t', p3 + 1);
        size_t p5 = full.find('\t', p4 + 1);
        if (p1 != std::string::npos && p5 != std::string::npos) {
            selId    = full.substr(p1 + 1, p2 - p1 - 1);
            origNote = full.substr(p4 + 1, p5 - p4 - 1);   // 已转义形式
        }
    }

    // 详情面板「备注」按钮（btn1: x≈908..992, y≈694..726）
    Click(w, 950, 710);
    Sleep(500);
    if (IsWindowVisible(Ctrl(w, L"Edit", 1007)))
        Log("PASS: note edit opened (multiline edit visible)");
    else
        Log("FAIL: note edit did not open");

    TypeText(w, 1007, L"note-e2e-check");
    SendMessageW(w, WM_COMMAND, 1004, (LPARAM)Ctrl(w, L"Button", 1004));
    Sleep(900);

    if (!IsWindowVisible(Ctrl(w, L"Edit", 1007)))
        Log("PASS: note saved (edit closed)");
    else
        Log("FAIL: note edit still open");

    // 还原原备注
    if (!selId.empty()) {
        std::string restore = "NOTE\t" + selId + "\t" + origNote;
        std::string r = IpcReq(restore);
        Log(r.compare(0, 2, "OK") == 0 ? "PASS: original note restored"
                                       : "FAIL: note restore failed");
    }
    Sleep(400);

    // ---- 切到机器配置页（侧栏「系统」分组首项） ----
    Click(w, 100, 334);
    Sleep(900);
    Shot(w, L"C:\\Users\\Administrator\\AppData\\Local\\Temp\\airui_config.bmp");

    // ---- 切回总览页（侧栏首项） ----
    Click(w, 100, 94);
    Sleep(700);
    Shot(w, L"C:\\Users\\Administrator\\AppData\\Local\\Temp\\airui_overview.bmp");

    Log("DONE");
    return 0;
}
