// ============================================================
// AirUI - test_paint_recorder.cpp (core-only, no platform headers)
//
// Produces a deterministic, platform-independent DrawCall golden.
// The same sequence must be produced on Windows / Linux / macOS;
// once a second backend (Skia) exists, PaintRecorder::Diff() is the
// G5 cross-backend consistency gate.
//
// CROSS-PLATFORM GOLDEN (P3-2):
//   The whole-scene PaintRecorder::ToString() is compared against a
//   committed baseline at tests/golden/paint_recorder.golden. That file is
//   the SINGLE source of truth: every platform's `test_paint_recorder`
//   (which is core-only and runs in the core-unix / windows / windows-x86
//   ctest) compares its own output against the same committed file. So a
//   platform that produces a different sequence fails that platform's
//   required ctest -- no separate artifact-download/diff job is needed.
//   Set UPDATE_GOLDEN=1 to (re)write the baseline; otherwise the test only
//   reads it and fails on any mismatch. Keep the file ASCII-only.
// ============================================================
#include "airui/PaintRecorder.h"
#include "airui/ControlsCore.h"
#include "airui/ShellCore.h"
#include "airui/Theme.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
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

// The committed cross-platform golden file lives next to this source file:
// tests/golden/paint_recorder.golden. Builds run from the build dir, so
// derive the repo path from __FILE__. __FILE__ may be absolute or relative
// and may use '/' or '\' depending on the toolchain, so normalize both.
std::string GoldenPath()
{
    std::string src = __FILE__;

    // Find the "tests" directory in the path. __FILE__ may be absolute or
    // relative and may use '/' or '\', so cover both. For a relative path
    // like "tests\test_paint_recorder.cpp" there is no leading separator
    // before "tests", so also detect the bare "tests/" or "tests\\" prefix.
    std::string::size_type pos = std::string::npos;
    std::string::size_type endLen = 0;
    const std::string markers[] = { "/tests/", "\\tests\\", "\\tests/", "/tests\\",
                                    "tests/",  "tests\\" };
    for (const auto& m : markers)
    {
        std::string::size_type p = src.rfind(m);
        if (p != std::string::npos && p >= pos)
        {
            pos = p;
            endLen = m.size();
        }
    }
    if (pos == std::string::npos)
    {
        // Fallback: assume the source is in a "tests" subdir; strip everything
        // after the test file name.
        const std::string fname = "test_paint_recorder.cpp";
        std::string::size_type q = src.rfind(fname);
        if (q != std::string::npos) src.resize(q);
        return src + "golden/paint_recorder.golden";
    }

    src.resize(pos + endLen);               // keep everything up to and incl. separator
    return src + "golden/paint_recorder.golden";
}

bool ReadFile(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
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
    all.TextBlock(Rect{ 0, 0, 50, 50 },L"block", 5,FontRole::Body,0,3);
    all.MeasureText(L"width", 5,FontRole::Meta);
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

    // ---- whole-scene canonical golden (P3-2) ----
    // Button scene then shell scene, concatenated with a blank line between.
    // This is the cross-platform baseline that every platform's
    // test_paint_recorder compares against (committed golden file).
    const std::string golden =
        btn.ToString() + "\n\n" + shell.ToString() + "\n";
    Check(IsAscii(golden), "whole-scene ToString() is ASCII-only");

    const std::string path = GoldenPath();

    // Explicit rewrite gate: UPDATE_GOLDEN=1 writes the baseline and returns 0.
    const char* updateEnv = std::getenv("UPDATE_GOLDEN");
    if (updateEnv && updateEnv[0] == '1')
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            std::printf("FAIL: cannot write golden file: %s\n", path.c_str());
            return 1;
        }
        out << golden;
        std::printf("GOLDEN UPDATED: %s (%zu bytes)\n", path.c_str(), golden.size());
        return 0;
    }

    // Normal mode: read the committed baseline and diff against it.
    std::string baseline;
    if (!ReadFile(path, baseline))
    {
        std::printf("FAIL: cannot read golden baseline: %s\n"
                    "  (run once with UPDATE_GOLDEN=1 to generate it)\n", path.c_str());
        ++g_failures;
    }
    else if (baseline == golden)
    {
        std::printf("PASS: output matches golden baseline (%zu bytes)\n", golden.size());
    }
    else
    {
        std::printf("FAIL: output differs from golden baseline: %s\n", path.c_str());
        ++g_failures;
        // Minimal diff summary: first differing line.
        size_t i = 0, line = 1;
        while (i < baseline.size() && i < golden.size() && baseline[i] == golden[i])
        {
            if (baseline[i] == '\n') ++line;
            ++i;
        }
        if (i == baseline.size() && i == golden.size())
            std::printf("  (lengths equal; see file)\n");
        else
            std::printf("  first diff at line %zu, offset %zu\n", line, i);
    }

    if (g_failures) std::printf("FAILED: %d\n", g_failures);
    else            std::printf("ALL PASS (%d failures)\n", g_failures);
    return g_failures ? 1 : 0;
}
