// ============================================================
// AirUI - test_paint_recorder.cpp (core-only, no platform headers)
//
// Produces a deterministic, platform-independent DrawCall golden.
// The same sequence must be produced on Windows / Linux / macOS;
// once a second backend (Skia) exists, PaintRecorder::Diff() is the
// G5 cross-backend consistency gate.
// ============================================================
#include "airui/PaintRecorder.h"
#include "airui/ControlsCore.h"
#include "airui/ShellCore.h"
#include "airui/Theme.h"

#include <cstdio>
#include <string>

using namespace air::ui;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++g_failures; }
    else     { std::printf("PASS: %s\n", what); }
}

PaintRecorder RecordButton(const std::wstring& label)
{
    PaintRecorder rec;
    Rect rc{ 0, 0, 100, 32 };
    ControlState st{};
    PaintButton(rec, rc, label, ButtonStyle::Primary, st, Pal(), Mx());
    return rec;
}

PaintRecorder RecordShell()
{
    ShellCore core;
    CaptionState cs; cs.title = L"App"; cs.subtitle = L"sub"; cs.badgeOk = true;
    StatusState ss;  ss.text = L"ready"; ss.ok = true;
    core.SetCaption(cs);
    core.SetStatus(ss);

    std::vector<ShellNavGroup> groups(1);
    groups[0].title = L"Group";
    ShellNavItem item; item.id = 7; item.label = L"Item";
    groups[0].items.push_back(item);
    core.SetNavGroups(groups);
    core.SetActiveNav(7);

    core.SetShellGeometry(40, 20, 200);
    core.Layout(Rect{ 0, 0, 800, 600 });

    PaintRecorder rec;
    core.Paint(rec, Pal(), Mx());
    return rec;
}

bool IsAscii(const std::string& s)
{
    for (unsigned char ch : s)
        if (ch > 127) return false;
    return true;
}

} // namespace

int main()
{
    // ---- core controls can be recorded through the shared IPainter ----
    PaintRecorder btn = RecordButton(L"Save");
    Check(btn.Ops().size() >= 2, "button records multiple paint ops");
    Check(btn.Count(PaintOpKind::FillRound) >= 1, "button records a rounded fill");
    Check(btn.Count(PaintOpKind::Text) >= 1, "button records text");

    // ---- wildcard matching (same idea as the old Win32 DrawCall) ----
    PaintOp expected;
    expected.kind = PaintOpKind::Text;
    expected.text = L"Save";
    bool found = false;
    for (const auto& op : btn.Ops())
        if (op.Matches(expected)) found = true;
    Check(found, "wildcard Matches() finds the labelled text op");

    PaintOp wrong = expected;
    wrong.text = L"Cancel";
    bool falsePositive = false;
    for (const auto& op : btn.Ops())
        if (op.Matches(wrong)) falsePositive = true;
    Check(!falsePositive, "Matches() rejects a different label");

    // ---- determinism: the golden must be reproducible ----
    PaintRecorder btn2 = RecordButton(L"Save");
    Check(btn.SequenceEquals(btn2), "identical scene produces identical sequence");
    Check(btn.Diff(btn2).empty(), "Diff() is empty for identical sequences");

    PaintRecorder btn3 = RecordButton(L"Cancel");
    Check(!btn.SequenceEquals(btn3), "changed scene produces a different sequence");
    Check(!btn.Diff(btn3).empty(), "Diff() explains the first difference");
    Check(btn.FirstDiff(btn3) < btn.Ops().size(), "FirstDiff() returns a valid index");

    // ---- serialization is deterministic and CI-log safe (ASCII) ----
    const std::string s1 = btn.Ops().front().ToString();
    const std::string s2 = btn2.Ops().front().ToString();
    Check(!s1.empty() && s1 == s2, "ToString() is deterministic");
    Check(IsAscii(s1), "ToString() is ASCII-only (wide text is length-encoded)");

    // ---- every IPainter method is captured ----
    PaintRecorder all;
    all.PushClip(Rect{ 0, 0, 50, 50 });
    all.FillRectAlpha(Rect{ 0, 0, 10, 10 }, MakeColor(1, 2, 3), 128);
    all.StrokeRound(Rect{ 1, 1, 9, 9 }, 3, MakeColor(4, 5, 6), 2);
    all.StrokeDashed(Rect{ 1, 1, 9, 9 }, MakeColor(7, 8, 9), 1, 4);
    all.TextBlock(Rect{ 0, 0, 50, 50 }, L"block", FontRole::Body, 0, 3);
    all.MeasureText(L"width", FontRole::Meta);
    all.Translate(Point{ 7, -3 });
    all.PopClip();
    Check(all.Count(PaintOpKind::PushClip) == 1 &&
          all.Count(PaintOpKind::FillRectAlpha) == 1 &&
          all.Count(PaintOpKind::StrokeRound) == 1 &&
          all.Count(PaintOpKind::StrokeDashed) == 1 &&
          all.Count(PaintOpKind::TextBlock) == 1 &&
          all.Count(PaintOpKind::MeasureText) == 1 &&
          all.Count(PaintOpKind::Translate) == 1 &&
          all.Count(PaintOpKind::PopClip) == 1,
          "all IPainter operations are recorded");
    all.Clear();
    Check(all.Ops().empty(), "Clear() resets the recording");

    // ---- the platform-free shell produces a stable golden too ----
    PaintRecorder shell = RecordShell();
    PaintRecorder shell2 = RecordShell();
    Check(shell.Ops().size() >= 3, "ShellCore paints caption / nav / status");
    Check(shell.SequenceEquals(shell2), "ShellCore golden is reproducible");
    Check(shell.Count(PaintOpKind::FillRect) >= 2, "ShellCore records surface fills");

    if (g_failures) std::printf("FAILED: %d\n", g_failures);
    else            std::printf("ALL PASS (%d failures)\n", g_failures);
    return g_failures ? 1 : 0;
}
