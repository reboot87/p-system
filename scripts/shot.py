"""Capture the airecord client area to a BMP for visual comparison.

Usage: python scripts/shot.py <gdi|skia> <out.bmp> [wait_s]
Pure ctypes + gdi32 (this environment's Pillow / Add-Type are unusable).
"""
import ctypes
import os
import struct
import subprocess
import sys
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

SRCCOPY = 0x00CC0020
DIB_RGB_COLORS = 0


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


def save_bmp(path, w, h, buf):
    row = w * 4
    hdr = struct.pack("<2sIHHI", b"BM", 54 + row * h, 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, w, -h, 1, 32, 0, row * h, 2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(hdr + info + buf)


def grab(hwnd, path):
    rc = RECT()
    if not user32.GetClientRect(hwnd, ctypes.byref(rc)):
        raise OSError("GetClientRect failed")
    w, h = rc.right - rc.left, rc.bottom - rc.top
    if w <= 0 or h <= 0:
        raise OSError("empty client rect")
    pt = POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))

    src = user32.GetDC(hwnd)
    mem = gdi32.CreateCompatibleDC(src)
    bmp = gdi32.CreateCompatibleBitmap(src, w, h)
    gdi32.SelectObject(mem, bmp)
    gdi32.BitBlt(mem, 0, 0, w, h, src, 0, 0, SRCCOPY)

    bi = BITMAPINFO()
    bi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bi.bmiHeader.biWidth = w
    bi.bmiHeader.biHeight = -h
    bi.bmiHeader.biPlanes = 1
    bi.bmiHeader.biBitCount = 32
    bi.bmiHeader.biCompression = 0
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(bi), DIB_RGB_COLORS)

    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(hwnd, src)
    save_bmp(path, w, h, buf.raw)
    return w, h, pt.x, pt.y


def main():
    backend = sys.argv[1] if len(sys.argv) > 1 else "skia"
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.environ["TEMP"], "shot.bmp")
    wait = float(sys.argv[3]) if len(sys.argv) > 3 else 4.0

    exe = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                       "build", "vs2017-x64", "Release", "airecord.exe")
    exe = os.path.abspath(exe)
    env = dict(os.environ)
    if os.environ.get("AIRUI_TRACE"):
        env["AIRUI_PAINT_TRACE"] = "1"
    proc = subprocess.Popen([exe, f"--paint-backend={backend}"], env=env)
    try:
        hwnd = 0
        deadline = time.time() + wait + 12
        while time.time() < deadline:
            time.sleep(0.3)
            h = user32.FindWindowW(None, "AireRecord")
            if h:
                hwnd = h
                break
        if not hwnd:
            raise OSError("window not found")
        time.sleep(wait)
        w, h, x, y = grab(hwnd, out)
        print(f"captured {backend}: {w}x{h} at ({x},{y}) -> {out}")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except Exception:
            proc.kill()


if __name__ == "__main__":
    main()
