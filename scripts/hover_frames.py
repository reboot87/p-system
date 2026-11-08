"""Hover flicker forensics for the clip list.

Walks the pointer from the sidebar list into the clip list one step at a
time and grabs the *screen* every step, printing the list-region brightness
and frame delta. A flash shows up as a step with a big delta (the row's card
disappears into the page background) instead of the usual 1px border change.

Usage:
    python scripts/hover_frames.py run  <outdir> [step]
    python scripts/hover_frames.py shot <out.png>
"""
import ctypes
import os
import sys
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

SRCCOPY = 0x00CC0020
DIB_RGB_COLORS = 0
WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)


class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


class POINT(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", ctypes.c_uint32), ("biWidth", ctypes.c_int32),
                ("biHeight", ctypes.c_int32), ("biPlanes", ctypes.c_uint16),
                ("biBitCount", ctypes.c_uint16),
                ("biCompression", ctypes.c_uint32),
                ("biSizeImage", ctypes.c_uint32),
                ("biXPelsPerMeter", ctypes.c_int32),
                ("biYPelsPerMeter", ctypes.c_int32),
                ("biClrUsed", ctypes.c_uint32),
                ("biClrImportant", ctypes.c_uint32)]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", ctypes.c_uint32 * 3)]


def move_cursor(x, y):
    """SetCursorPos is exact and still generates WM_MOUSEMOVE."""
    user32.SetCursorPos(int(x), int(y))


def window_rect(hwnd):
    rc = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rc))
    return rc


def find_window():
    for cls, title in (("AireRecordWnd", None), (None, "AireRecord")):
        h = user32.FindWindowW(cls, title)
        if h:
            return int(h)
    return 0


def find_listbox(hwnd):
    hits = []

    def cb(child, _lp):
        buf = ctypes.create_unicode_buffer(64)
        user32.GetClassNameW(child, buf, 64)
        if buf.value.upper() == "LISTBOX":
            rc = window_rect(child)
            if rc.right - rc.left > 40 and user32.IsWindowVisible(child):
                hits.append(int(child))
                return False
        return True

    user32.EnumChildWindows(wintypes.HWND(hwnd), WNDENUMPROC(cb), 0)
    return hits[0] if hits else 0


def grab_screen(x, y, w, h):
    sdc = user32.GetDC(None)
    mem = gdi32.CreateCompatibleDC(sdc)
    bmp = gdi32.CreateCompatibleBitmap(sdc, w, h)
    gdi32.SelectObject(mem, bmp)
    gdi32.BitBlt(mem, 0, 0, w, h, sdc, x, y, SRCCOPY)
    bi = BITMAPINFO()
    bi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bi.bmiHeader.biWidth = w
    bi.bmiHeader.biHeight = -h
    bi.bmiHeader.biPlanes = 1
    bi.bmiHeader.biBitCount = 32
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(bi), DIB_RGB_COLORS)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(None, sdc)
    return buf.raw


def grab_window(hwnd):
    rc = window_rect(hwnd)
    return rc.right - rc.left, rc.bottom - rc.top, rc.left, rc.top, \
        grab_screen(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top)


def save_rgba(path, w, h, raw):
    from PIL import Image
    Image.frombuffer("RGBA", (w, h), raw, "raw", "BGRA", 0, 1).convert("RGB").save(path)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "run"
    hwnd = find_window()
    if not hwnd:
        print("no airecord window")
        return 1

    if mode == "shot":
        out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.environ["TEMP"], "win.png")
        w, h, _, _, raw = grab_window(hwnd)
        save_rgba(out, w, h, raw)
        print("saved", out, w, h)
        return 0

    outdir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.environ["TEMP"], "hover")
    step = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    os.makedirs(outdir, exist_ok=True)
    lb = find_listbox(hwnd)
    if not lb:
        print("no visible listbox (switch to the clips page first)")
        return 1
    lr = window_rect(lb)
    wr = window_rect(hwnd)
    print(f"window {wr.left},{wr.top} {wr.right - wr.left}x{wr.bottom - wr.top}")
    print(f"listbox {lr.left},{lr.top} {lr.right - lr.left}x{lr.bottom - lr.top}")

    import numpy as np

    W, H = wr.right - wr.left, wr.bottom - wr.top
    # list area inside the window frame, keeping clear of the scrollbar
    ax0, ay0 = lr.left - wr.left + 4, lr.top - wr.top + 4
    ax1, ay1 = lr.right - wr.left - 20, lr.bottom - wr.top - 10

    def snap():
        a = np.frombuffer(grab_screen(wr.left, wr.top, W, H), dtype=np.uint8)
        return a.reshape(H, W, 4)[:, :, :3].copy()

    def area(a):
        return a[ay0:ay1, ax0:ax1]

    y = lr.top + 100
    x0, x1 = wr.left + 60, lr.left + 200

    move_cursor(x0, y)
    time.sleep(0.4)
    prev = snap()

    print(f"{'i':>3} {'x':>5} {'mean':>8} {'diff':>7} {'max':>4} {'t':>6}")
    t0 = time.perf_counter()
    i, x = 0, x0
    while x <= x1:
        move_cursor(x, y)
        time.sleep(0.05)
        cur = snap()
        a, b = area(cur).astype(np.int16), area(prev).astype(np.int16)
        d = np.abs(a - b)
        mx = int(d.max())
        print(f"{i:3d} {x:5d} {a.mean():8.3f} {d.mean():7.3f} {mx:4d} "
              f"{time.perf_counter() - t0:6.3f}")
        if i and mx > 24:
            from PIL import Image
            Image.fromarray(cur).save(os.path.join(outdir, f"spike_{i:02d}.png"))
            print("        ^ spike frame saved")
        prev = cur
        i += 1
        x += step
    return 0


def cross():
    """Walk 1px steps over the sidebar/clip-list boundary and count flashy frames.

    A flash = a step whose list-region delta is an order of magnitude above the
    usual hover repaint (~0.05 mean), i.e. a row erased and redrawn again.
    """
    hwnd = find_window()
    lb = find_listbox(hwnd)
    if not lb:
        print("no visible listbox (switch to the clips page first)")
        return 1
    lr, wr = window_rect(lb), window_rect(hwnd)
    W, H = wr.right - wr.left, wr.bottom - wr.top
    ax0, ay0 = lr.left - wr.left + 4, lr.top - wr.top + 4
    ax1, ay1 = lr.right - wr.left - 20, lr.bottom - wr.top - 10
    y = lr.top + 100

    import numpy as np

    def snap():
        a = np.frombuffer(grab_screen(wr.left, wr.top, W, H), dtype=np.uint8)
        return a.reshape(H, W, 4)[:, :, :3].copy()

    def area(a):
        return a[ay0:ay1, ax0:ax1].astype(np.int16)

    # Approach from inside the list, leave it, come back: exactly the gesture
    # the report describes ("pointer moves from the left list to the right
    # list"), then repeat to see whether it is a one-off or a per-move flash.
    xs = list(range(lr.left + 60, lr.left - 41, -1)) + \
         list(range(lr.left - 40, lr.left + 61, 1))
    prev = None
    flash = 0
    worst = (0.0, 0)
    for x in xs:
        move_cursor(x, y)
        time.sleep(0.04)
        cur = snap()
        if prev is not None:
            d = np.abs(area(cur) - area(prev))
            m = float(d.mean())
            if m > 0.2:
                flash += 1
                worst = (m, x) if m > worst[0] else worst
                print(f"  flash x={x:5d} mean={m:6.3f} max={int(d.max()):3d}")
        prev = cur
    print(f"steps={len(xs)} flash_frames={flash} worst={worst[0]:.3f} at x={worst[1]}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "cross":
        sys.exit(cross())
    sys.exit(main())
