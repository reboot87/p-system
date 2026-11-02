#include "ConfigPage.h"
#include "core/App.h"
#include "core/Format.h"
#include "../ipc/Ipc.h"

using namespace air::ui;

void ConfigPage::OnAttach(Shell& shell)
{
    (void)shell;
}

void ConfigPage::OnShow(bool shown)
{
    if (!shown) {
        KillTimer(m_shell->Hwnd(), TIMER_SYSINFO);
        return;
    }
    m_sys = sysinfo::Collect();
    m_sysLoaded = true;
    SetTimer(m_shell->Hwnd(), TIMER_SYSINFO, 2000, nullptr);
}

void ConfigPage::OnLayout(const RECT& content)
{
    (void)content;   // 配置页是纯绘制页，双栏流式排布，无需缓存几何
}

bool ConfigPage::OnTimer(UINT_PTR id)
{
    if (id != TIMER_SYSINFO) return false;
    m_sys = sysinfo::Collect();
    m_shell->InvalidateContent();
    return true;
}

void ConfigPage::DrawInfoCard(HDC dc, RECT rc, const CardData& c)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();

    gfx::FillRound(dc, rc, p.surface, m.radiusLg);
    gfx::Stroke(dc, rc, p.split, m.radiusLg);

    // 头部：图标块 + 标题 + 右上角状态
    RECT ic = { rc.left + 14, rc.top + 10, rc.left + 40, rc.top + 36 };
    gfx::IconTile(dc, ic, c.glyph, F().Get(FontRole::Icon),
                  p.primarySoft, p.primary, 6);

    RECT rcT = { ic.right + 8, rc.top + 10, rc.right - 90, rc.top + 36 };
    gfx::Text(dc, F().Get(FontRole::Title), p.text, rcT, c.title,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    if (!c.badge.empty()) {
        gfx::PillRight(dc, rc.right - 14, rc.top + 13, c.badge.c_str(),
                       F().Get(FontRole::Meta),
                       c.badgeOk ? p.okSoft : p.primarySoft,
                       c.badgeOk ? p.okText : p.primaryText);
    }

    // 行
    int y = rc.top + 44;
    for (auto& row : c.rows) {
        auto f = ipc::SplitTabs(row);
        const std::wstring k = f.size() > 0 ? f[0] : L"";
        const std::wstring v = f.size() > 1 ? f[1] : L"";
        int bar = f.size() > 2 ? _wtoi(f[2].c_str()) : -1;

        RECT rcK = { rc.left + 16, y, rc.left + 112, y + 22 };
        gfx::Text(dc, F().Get(FontRole::Meta), p.textFaint, rcK, k,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

        bool hasBar = (bar >= 0 && bar <= 100);
        int vRight = hasBar ? rc.right - 16 - 158 : rc.right - 16;
        RECT rcV = { rc.left + 118, y, vRight, y + 22 };
        gfx::Text(dc, F().Get(FontRole::Body), p.text, rcV, v,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER |
                  DT_END_ELLIPSIS | DT_NOPREFIX);

        if (hasBar) {
            RECT rcBar = { rc.right - 16 - 160, y + 7, rc.right - 16 - 46,
                           y + 14 };
            gfx::ProgressBar(dc, rcBar, bar, RGB(239, 241, 244),
                             p.primary, RGB(250, 173, 20), RGB(229, 72, 77));

            wchar_t pct[16];
            swprintf_s(pct, 16, L"%d%%", bar);
            RECT rcPct = { rc.right - 16 - 42, y, rc.right - 16, y + 22 };
            gfx::Text(dc, F().Get(FontRole::Meta), p.textFaint, rcPct, pct,
                      DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);
        }
        y += 26;
    }
}

void ConfigPage::OnPaint(HDC dc, const RECT& content)
{
    const Palette& p = Pal();

    if (!m_sysLoaded) {
        m_sys = sysinfo::Collect();
        m_sysLoaded = true;
    }

    int x0 = content.left;
    int x1 = content.right;
    int W  = x1 - x0;
    int colW = (W - 14) / 2;
    int top  = content.top + 2;

    std::wstring dbPath = L"%APPDATA%\\AireRecord\\airecord.db";

    std::wstring svcText;
    int svcState = sysinfo::QueryServiceState(L"AireRecord", svcText);

    std::vector<CardData> cards;

    // 1) 服务与进程
    {
        CardData c = { L"\xE9F9", L"服务与进程",
                       svcState == 2 ? L"运行中"
                     : (svcState == 1 ? L"已停止" : L"未安装"),
                       svcState == 2, {} };
        wchar_t buf[128];
        c.rows.push_back(L"运行模式\t系统服务 + 会话代理\t-1");
        swprintf_s(buf, 128, L"服务状态\t● %s\t-1", svcText.c_str());
        c.rows.push_back(buf);
        c.rows.push_back(L"启动类型\t自动（随系统启动）\t-1");
        c.rows.push_back(std::wstring(L"IPC 管道\t") + ipc::PipeName() + L"\t-1");
        c.rows.push_back(L"数据库\t" + dbPath + L"\t-1");
        cards.push_back(std::move(c));
    }

    // 2) 操作系统
    {
        CardData c = { L"\xE770", L"操作系统", L"已采集", true, {} };
        c.rows.push_back(L"系统版本\t" + m_sys.osName + L"（Build " +
                         m_sys.osBuild + L"）\t-1");
        c.rows.push_back(L"系统架构\t" + m_sys.arch + L" · 中文简体\t-1");
        c.rows.push_back(L"计算机名\t" + m_sys.computerName + L"\t-1");
        c.rows.push_back(L"当前用户\t" + m_sys.userName + L"\t-1");
        c.rows.push_back(L"开机时长\t" + m_sys.uptime + L"\t-1");
        cards.push_back(std::move(c));
    }

    // 3) 处理器与内存
    {
        CardData c = { L"\xE950", L"处理器与内存", L"已采集", true, {} };
        c.rows.push_back(L"处理器\t" + m_sys.cpuName + L"\t-1");
        c.rows.push_back(L"逻辑处理器\t" +
                         std::to_wstring(m_sys.logicalCpu) + L" 个\t-1");
        int cpu = (int)(m_sys.cpuUsage < 0 ? 0 : m_sys.cpuUsage);
        c.rows.push_back(L"CPU 占用\t" + std::to_wstring(cpu) + L"%" +
                         L"\t" + std::to_wstring(cpu));
        double usedMB = (double)(m_sys.memTotalMB - m_sys.memAvailMB);
        int memPct = m_sys.memTotalMB
            ? (int)(usedMB * 100 / m_sys.memTotalMB) : 0;
        c.rows.push_back(L"物理内存\t" + fmt::SizeMB(m_sys.memTotalMB) + L"\t-1");
        c.rows.push_back(L"内存占用\t已用 " + fmt::SizeMB((long long)usedMB) +
                         L"\t" + std::to_wstring(memPct));
        cards.push_back(std::move(c));
    }

    // 4) 显示与图形
    {
        CardData c = { L"\xE7F4", L"显示与图形", L"已采集", true, {} };
        c.rows.push_back(L"显卡\t" + m_sys.gpuName + L"\t-1");
        c.rows.push_back(L"主显示器\t" + m_sys.primaryRes + L"\t-1");
        c.rows.push_back(L"显示器数量\t" +
                         std::to_wstring(m_sys.monitorCount) + L" 台\t-1");
        cards.push_back(std::move(c));
    }

    // 5) 磁盘
    {
        CardData c = { L"\xEDA2", L"磁盘", L"已采集", true, {} };
        for (auto& d : m_sys.disks) {
            int pct = d.totalMB
                ? (int)(((d.totalMB - d.freeMB) * 100) / d.totalMB) : 0;
            c.rows.push_back(d.letter + L" 剩余\t" + fmt::SizeMB(d.freeMB) +
                             L" / " + fmt::SizeMB(d.totalMB) + L"\t" +
                             std::to_wstring(pct));
        }
        if (m_sys.disks.empty())
            c.rows.push_back(L"磁盘\t未采集到固定磁盘\t-1");
        cards.push_back(std::move(c));
    }

    // 6) 网络
    {
        CardData c = { L"\xE968", L"网络",
                       m_sys.netIp.empty() ? L"未连接" : L"已连接",
                       !m_sys.netIp.empty(), {} };
        c.rows.push_back(L"主机名\t" + m_sys.computerName + L"\t-1");
        c.rows.push_back(L"IP 地址\t" +
                         (m_sys.netIp.empty() ? L"—" : m_sys.netIp) + L"\t-1");
        c.rows.push_back(L"网卡\t" +
                         (m_sys.netAdapter.empty() ? L"—" : m_sys.netAdapter) +
                         L"\t-1");
        cards.push_back(std::move(c));
    }

    // 7) 快捷键
    {
        CardData c = { L"\xE765", L"快捷键", L"3 个已启用", false, {} };
        c.rows.push_back(L"呼出主窗口\tCtrl + Shift + V\t-1");
        c.rows.push_back(L"搜索\tF3\t-1");
        c.rows.push_back(L"手动录入\tF4\t-1");
        cards.push_back(std::move(c));
    }

    // 8) 功能模块
    {
        CardData c = { L"\xE71A", L"功能模块（插件注册）",
                       L"1 / 3 已启用", false, {} };
        c.rows.push_back(L"剪贴板\t运行中 · ClipboardPlugin\t-1");
        c.rows.push_back(L"文件监控\t规划中 · FileWatchPlugin\t-1");
        c.rows.push_back(L"打印监控\t规划中 · PrintWatchPlugin\t-1");
        cards.push_back(std::move(c));
    }

    // 双栏：交替放入左右两列
    int yL = top, yR = top;
    int col = 0;
    for (auto& c : cards) {
        int h = CardHeight((int)c.rows.size());
        RECT rc;
        if (col == 0) { rc = { x0, yL, x0 + colW, yL + h }; yL += h + 14; }
        else          { rc = { x0 + colW + 14, yR, x1, yR + h }; yR += h + 14; }
        DrawInfoCard(dc, rc, c);
        col ^= 1;
    }

    (void)p;
}
