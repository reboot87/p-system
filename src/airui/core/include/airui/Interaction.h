#pragma once
// ============================================================
// AirUI — Interaction.h
// 自绘热区的悬停 / 按下状态机（平台无关）
//
// 页面里那些用 Canvas 直接画出来的按钮没有 HWND：Windows 不会替我们
// 记"指针正指着谁"，也没有 BN_CLICKED 告诉我们"按下又在同一块上松手
// 才算点击"。这个类只做三件事：记住指针当前命中哪个热区、左键按在哪
// 个热区、以及这次变化是否需要重绘。
//
//   int hot = HitTest(abs);                 // 0 = 没命中
//   if (hot.m_hover.Move(hot)) Invalidate();
//
//   OnMouseDown : m_hot.Down(hot); SetCapture(true);
//   OnMouseUp   : if (m_hot.Release(hot)) Activate(hot);
//
// 视觉约定：
//   Hovered(id)  —— 指针在上面（也可能是按住后拖回到这里）
//   Pressed(id)  —— 按在上面且指针没被拖走，等价于桌面按钮的"凹下去"
// 拖出去松手不触发点击，拖回来再松手仍然触发 —— 与原生按钮一致。
// ============================================================

namespace air {
namespace ui {

class HotState
{
public:
    // ---- 指针移动。hit = 命中的热区 id，0 表示未命中 ----
    // 返回 true 表示悬停目标变了，调用方需要重绘。
    bool Move(int hit)
    {
        if (hit == m_hover) return false;
        m_hover = hit;
        return true;
    }

    // ---- 指针离开窗口：悬停与按下一起清掉 ----
    // （捕获期间收不到 WM_MOUSELEAVE，走的是带负坐标的移动，
    //   所以这里同时也承担"离开按钮"的兜底。）
    bool Leave()
    {
        const bool dirty = (m_hover != 0) || (m_press != 0);
        m_hover = 0;
        m_press = 0;
        return dirty;
    }

    // ---- 左键按下。返回 true 表示需要重绘 ----
    bool Down(int hit)
    {
        const bool dirty = Move(hit);
        if (hit == 0) {
            const bool had = (m_press != 0);
            m_press = 0;
            return dirty || had;
        }
        if (m_press == hit) return dirty;
        m_press = hit;
        return true;
    }

    // ---- 左键抬起。返回 true = 构成一次点击（同一热区按下并抬起） ----
    bool Release(int hit)
    {
        const bool clicked = (m_press != 0 && m_press == hit);
        m_press = 0;
        Move(hit);
        return clicked;
    }

    bool Pressing() const          { return m_press != 0; }
    bool Hovered(int id) const     { return id != 0 && m_hover == id; }
    bool Pressed(int id) const     { return id != 0 && m_press == id &&
                                            m_hover == id; }
    int  HoverId() const           { return m_hover; }
    int  PressId() const           { return m_press; }

private:
    int m_hover = 0;
    int m_press = 0;
};

} // namespace ui
} // namespace air
