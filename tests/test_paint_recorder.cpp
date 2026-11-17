// ============================================================
// AirUI - test_paint_recorder.cpp (core-only, no platform headers)
//
// Produces a deterministic, platform-independent DrawCall golden.
// The same sequence must be produced on Windows / Linux / macOS;
// once a second backend (Skia) exists, PaintRecorder::Diff() is the
// G5 cross-backend consistency gate.
//
// CROSS-PLATFORM GOLDEN (P3-2):
//   The whole-scene PaintRecorder::ToString() was originally compared
//   byte-for-byte against a committed file (tests/golden/paint_recorder.golden).
//   That byte-exact file comparison proved too strict for CI:
//     * On Windows CI a fresh `git checkout` with core.autocrlf=true
//       rewrites the committed LF golden to CRLF, so the binary-mode read
//       yields CRLF while the test generates '\n' -> spurious "mismatch".
//     * On core-unix (Unix Makefiles) the compiler passes a *relative*
//       __FILE__ ("../tests/test_paint_recorder.cpp") so locating the
//       golden next to the source depends on the process working dir and
//       fails under ctest.
//   So the gate is now a STRUCTURAL assertion, not a byte-exact file diff:
//   every op is reduced to a platform-independent key (op kind + role +
//   ASCII length-prefixed label + semantic flags) that ignores the pure
//   layout/geometry numerics (rect, width, radius, measured, offset) which
//   legitimately differ across DPI / fonts / toolchains. The committed
//   EXPECTED_KEY_SEQUENCE is embedded below (ASCII-only, no file I/O), so
//   the check is immune to CRLF, relative __FILE__ and working-dir changes
//   while STILL failing when an op is dropped, added, reordered or relabelled.
//   Set UPDATE_GOLDEN=1 to re-print the current key sequence (and rewrite
//   the reference file) when the draw-call contract intentionally changes.
// ============================================================
#include "airui/PaintRecorder.h"
#include "airui/ControlsCore.h"
#include "airui/ShellCore.h"
#include "airui/Theme.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
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

// ---- structural (platform-independent) serialization ----
//
// Byte-exact ToString() is NOT stable across toolchains / DPI: the pure
// layout/geometry numerics (rect, width, radius, measureResult, offset)
// can legitimately differ on Windows vs Linux, VS2017 vs VS2022, or at a
// different system DPI. So the golden gate reduces each op to the SEMANTIC
// key that must be identical everywhere: op kind, font role, the ASCII
// length-prefixed label, and the draw flags (alpha, maxLines, dash, format,
// color). A dropped/added/reordered op or a changed label/role changes the
// key sequence and fails the gate; a different measured width does not.

// Length-prefix a wide label so it stays ASCII-only and reproducible.
std::string TextKey(const std::wstring& w)
{
    std::ostringstream os;
    os << '[' << w.size() << ']';
    for (wchar_t ch : w)
    {
        if (ch >= 0 && ch < 128) os << (char)ch;   // ASCII label kept verbatim
        else                    os << '?';          // non-ASCII -> '?' (still counted)
    }
    return os.str();
}

std::string StructuralLine(const PaintOp& op)
{
    std::ostringstream os;
    os << PaintOpName(op.kind)
       << " role=" << (int)op.role
       << " text=" << TextKey(op.text)
       << " alpha=" << (int)op.alpha
       << " maxLines=" << op.maxLines
       << " dash=" << op.dash
       << " fmt=0x" << std::hex << op.format << std::dec
       << " color=" << std::hex << std::setw(6) << std::setfill('0') << op.color << std::dec;
    return os.str();
}

std::string StructuralSignature(const PaintRecorder& rec)
{
    std::string out;
    for (const auto& op : rec.Ops())
    {
        if (!out.empty()) out.push_back('\n');
        out += StructuralLine(op);
    }
    return out;
}

// Committed cross-platform semantic baseline (ASCII-only). Regenerate with
// UPDATE_GOLDEN=1 and paste the output here when the draw-call contract is
// intentionally changed (see block comment at the top of this file).
const std::string kExpectedKeySequence =
R"(FillRound role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=ff7716
Text role=4 text=[4]Save alpha=0 maxLines=0 dash=0 fmt=0x825 color=ffffff

FillRect role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=ffffff
StrokeRect role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=d9d9d9
Text role=2 text=[3]App alpha=0 maxLines=0 dash=0 fmt=0x0 color=1f1f1f
Text role=1 text=[3]sub alpha=0 maxLines=0 dash=0 fmt=0x0 color=595959
PushClip role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=000000
FillRect role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=fdfcfb
StrokeRect role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=d9d9d9
Text role=1 text=[5]Group alpha=0 maxLines=0 dash=0 fmt=0x0 color=595959
FillRound role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=fff4e6
FillRect role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=ff7716
Text role=0 text=[4]Item alpha=0 maxLines=0 dash=0 fmt=0x0 color=dc6816
PopClip role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=000000
FillRect role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=faf8f7
StrokeRect role=0 text=[0] alpha=0 maxLines=0 dash=0 fmt=0x0 color=d9d9d9
Text role=1 text=[5]ready alpha=0 maxLines=0 dash=0 fmt=0x0 color=1f1f1f
)";

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

    // ---- whole-scene structural golden (P3-2, platform-agnostic) ----
    // Button scene then shell scene, concatenated with a blank line between.
    // The committed EXPECTED_KEY_SEQUENCE (below) is the cross-platform
    // semantic baseline: op kinds + roles + labels + draw flags. Layout
    // numerics (rect/width/radius/measured/offset) are intentionally NOT
    // part of the key because they can differ by DPI / font / toolchain.
    const std::string sig =
        StructuralSignature(btn) + "\n\n" + StructuralSignature(shell) + "\n";
    Check(IsAscii(sig), "structural signature is ASCII-only");

    const std::string path = GoldenPath();

    // Explicit rewrite gate: UPDATE_GOLDEN=1 writes the structural signature
    // to the golden file (diagnostic only) and returns 0. The committed
    // EXPECTED_KEY_SEQUENCE must be updated by hand to match.
    const char* updateEnv = std::getenv("UPDATE_GOLDEN");
    if (updateEnv && updateEnv[0] == '1')
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            std::printf("FAIL: cannot write golden file: %s\n", path.c_str());
            return 1;
        }
        out << sig;
        std::printf("GOLDEN UPDATED (structural): %s (%zu bytes)\n", path.c_str(), sig.size());
        std::printf("Update EXPECTED_KEY_SEQUENCE in test_paint_recorder.cpp to:\n");
        std::printf("----\n%s----\n", sig.c_str());
        return 0;
    }

    // Normal mode: compare the structural signature against the embedded,
    // platform-independent expected sequence (no file I/O, so CRLF / relative
    // __FILE__ / working-dir differences cannot cause a spurious mismatch).
    if (sig == kExpectedKeySequence)
    {
        std::printf("PASS: structural signature matches committed baseline (%zu bytes)\n", sig.size());
    }
    else
    {
        std::printf("FAIL: structural signature differs from committed baseline\n");
        ++g_failures;
        size_t i = 0, line = 1;
        while (i < kExpectedKeySequence.size() && i < sig.size() &&
               kExpectedKeySequence[i] == sig[i])
        {
            if (sig[i] == '\n') ++line;
            ++i;
        }
        if (i == kExpectedKeySequence.size() && i == sig.size())
            std::printf("  (lengths equal; see code)\n");
        else
            std::printf("  first diff at line %zu, offset %zu\n", line, i);
    }

    if (g_failures) std::printf("FAILED: %d\n", g_failures);
    else            std::printf("ALL PASS (%d failures)\n", g_failures);
    return g_failures ? 1 : 0;
}
