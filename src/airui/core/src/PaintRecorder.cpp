// ============================================================
// AirUI - PaintRecorder.cpp
// Platform-free IPainter recorder (core only, no platform headers).
// ============================================================
#include "airui/PaintRecorder.h"

#include <iomanip>
#include <sstream>

namespace air {
namespace ui {

namespace {

bool SameOp(const PaintOp& a, const PaintOp& b)
{
    return a.kind == b.kind
        && a.rect.left == b.rect.left && a.rect.top == b.rect.top
        && a.rect.right == b.rect.right && a.rect.bottom == b.rect.bottom
        && a.color == b.color
        && a.alpha == b.alpha
        && a.radius == b.radius
        && a.width == b.width
        && a.dash == b.dash
        && a.maxLines == b.maxLines
        && a.measureResult == b.measureResult
        && a.role == b.role
        && a.format == b.format
        && a.text == b.text
        && a.offset.x == b.offset.x && a.offset.y == b.offset.y;
}

} // namespace

const char* PaintOpName(PaintOpKind kind)
{
    switch (kind)
    {
    case PaintOpKind::FillRect:      return "FillRect";
    case PaintOpKind::FillRectAlpha: return "FillRectAlpha";
    case PaintOpKind::FillRound:     return "FillRound";
    case PaintOpKind::StrokeRect:    return "StrokeRect";
    case PaintOpKind::StrokeRound:   return "StrokeRound";
    case PaintOpKind::StrokeDashed:  return "StrokeDashed";
    case PaintOpKind::Text:          return "Text";
    case PaintOpKind::TextBlock:     return "TextBlock";
    case PaintOpKind::MeasureText:   return "MeasureText";
    case PaintOpKind::PushClip:      return "PushClip";
    case PaintOpKind::PopClip:       return "PopClip";
    case PaintOpKind::Translate:     return "Translate";
    }
    return "Unknown";
}

bool PaintOp::Matches(const PaintOp& expected) const
{
    if (kind != expected.kind) return false;
    if (!expected.rect.Empty() &&
        (rect.left != expected.rect.left || rect.top != expected.rect.top ||
         rect.right != expected.rect.right || rect.bottom != expected.rect.bottom))
        return false;
    if (expected.color != 0 && color != expected.color) return false;
    if (expected.alpha != 0 && alpha != expected.alpha) return false;
    if (expected.radius != 0 && radius != expected.radius) return false;
    if (expected.width != 0 && width != expected.width) return false;
    if (expected.dash != 0 && dash != expected.dash) return false;
    if (expected.maxLines != 0 && maxLines != expected.maxLines) return false;
    if (expected.format != 0 && format != expected.format) return false;
    if (expected.role != FontRole::Body && role != expected.role) return false;
    if (!expected.text.empty() && text != expected.text) return false;
    return true;
}

std::string PaintOp::ToString() const
{
    std::ostringstream os;
    os << PaintOpName(kind)
       << " rect=(" << rect.left << ',' << rect.top << ','
       << rect.right << ',' << rect.bottom << ')'
       << " color=" << std::hex << std::setw(6) << std::setfill('0') << color << std::dec
       << " alpha=" << (int)alpha
       << " radius=" << radius << " width=" << width << " dash=" << dash
       << " maxLines=" << maxLines << " measured=" << measureResult
       << " role=" << (int)role
       << " fmt=0x" << std::hex << format << std::dec
       << " textLen=" << text.size()
       << " offset=(" << offset.x << ',' << offset.y << ')';
    return os.str();
}

size_t PaintRecorder::Count(PaintOpKind kind) const
{
    size_t n = 0;
    for (const auto& op : m_ops)
        if (op.kind == kind) ++n;
    return n;
}

size_t PaintRecorder::FirstDiff(const PaintRecorder& other) const
{
    const size_t n = m_ops.size() < other.m_ops.size() ? m_ops.size() : other.m_ops.size();
    for (size_t i = 0; i < n; ++i)
        if (!SameOp(m_ops[i], other.m_ops[i])) return i;
    if (m_ops.size() != other.m_ops.size()) return n;
    return (size_t)-1;
}

bool PaintRecorder::SequenceEquals(const PaintRecorder& other) const
{
    return FirstDiff(other) == (size_t)-1;
}

std::string PaintRecorder::Diff(const PaintRecorder& other) const
{
    const size_t at = FirstDiff(other);
    if (at == (size_t)-1) return std::string();

    std::ostringstream os;
    if (m_ops.size() != other.m_ops.size())
        os << "op count " << m_ops.size() << " vs " << other.m_ops.size() << "; ";
    os << "first diff at #" << at;
    if (at < m_ops.size() && at < other.m_ops.size())
        os << "\n  lhs: " << m_ops[at].ToString()
           << "\n  rhs: " << other.m_ops[at].ToString();
    else
        os << " (one sequence ended)";
    return os.str();
}

void PaintRecorder::FillRect(const Rect& r, Color c)
{
    PaintOp op; op.kind = PaintOpKind::FillRect; op.rect = r; op.color = c;
    m_ops.push_back(op);
}

void PaintRecorder::FillRectAlpha(const Rect& r, Color c, uint8_t alpha)
{
    PaintOp op; op.kind = PaintOpKind::FillRectAlpha; op.rect = r;
    op.color = c; op.alpha = alpha;
    m_ops.push_back(op);
}

void PaintRecorder::FillRound(const Rect& r, int radius, Color c)
{
    PaintOp op; op.kind = PaintOpKind::FillRound; op.rect = r;
    op.radius = radius; op.color = c;
    m_ops.push_back(op);
}

void PaintRecorder::StrokeRect(const Rect& r, Color c, int width)
{
    PaintOp op; op.kind = PaintOpKind::StrokeRect; op.rect = r;
    op.color = c; op.width = width;
    m_ops.push_back(op);
}

void PaintRecorder::StrokeRound(const Rect& r, int radius, Color c, int width)
{
    PaintOp op; op.kind = PaintOpKind::StrokeRound; op.rect = r;
    op.radius = radius; op.color = c; op.width = width;
    m_ops.push_back(op);
}

void PaintRecorder::StrokeDashed(const Rect& r, Color c, int width, int dash)
{
    PaintOp op; op.kind = PaintOpKind::StrokeDashed; op.rect = r;
    op.color = c; op.width = width; op.dash = dash;
    m_ops.push_back(op);
}

void PaintRecorder::Text(const Rect& r, const std::wstring& s,
                         FontRole role, Color c, TextFormat fmt)
{
    PaintOp op; op.kind = PaintOpKind::Text; op.rect = r; op.text = s;
    op.role = role; op.color = c; op.format = fmt;
    m_ops.push_back(op);
}

void PaintRecorder::TextBlock(const Rect& r, const std::wstring& s,
                              FontRole role, Color c, int maxLines)
{
    PaintOp op; op.kind = PaintOpKind::TextBlock; op.rect = r; op.text = s;
    op.role = role; op.color = c; op.maxLines = maxLines;
    m_ops.push_back(op);
}

int PaintRecorder::MeasureText(const std::wstring& s, FontRole role)
{
    PaintOp op; op.kind = PaintOpKind::MeasureText; op.text = s; op.role = role;
    m_ops.push_back(op);
    return op.measureResult;   // deterministic 0; core never depends on pixels
}

void PaintRecorder::PushClip(const Rect& r)
{
    PaintOp op; op.kind = PaintOpKind::PushClip; op.rect = r;
    m_ops.push_back(op);
}

void PaintRecorder::PopClip()
{
    PaintOp op; op.kind = PaintOpKind::PopClip;
    m_ops.push_back(op);
}

void PaintRecorder::Translate(Point offset)
{
    PaintOp op; op.kind = PaintOpKind::Translate; op.offset = offset;
    m_ops.push_back(op);
}

} // namespace ui
} // namespace air
