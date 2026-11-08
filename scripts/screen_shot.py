"""Full-desktop screenshot, for visual checks against a live window.

PrintWindow() talks to the window's redirection surface, which goes stale
for a window that paints straight onto its BeginPaint DC (the Skia path
does exactly that), so it can hand back the previous frame. A desktop
BitBlt is what the user actually sees.

Usage:
    python scripts/screen_shot.py out.png [x y w h]
"""
import ctypes
import struct
import sys
import zlib
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
SRCCOPY = 0x00CC0020


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


def write_png(path, w, h, rgba):
    raw = b"".join(b"\x00" + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))

    def chunk(tag, data):
        body = tag + data
        return (struct.pack(">I", len(data)) + body +
                struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 6)))
        f.write(chunk(b"IEND", b""))


def grab(x, y, w, h):
    screen = user32.GetDC(0)
    mem = gdi32.CreateCompatibleDC(screen)
    bmp = gdi32.CreateCompatibleBitmap(screen, w, h)
    gdi32.SelectObject(mem, bmp)
    gdi32.BitBlt(mem, 0, 0, w, h, screen, x, y, SRCCOPY)
    bi = BITMAPINFOHEADER()
    bi.biSize, bi.biWidth, bi.biHeight = ctypes.sizeof(BITMAPINFOHEADER), w, -h
    bi.biPlanes, bi.biBitCount, bi.biCompression = 1, 32, 0
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(bi), 0)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(0, screen)
    return bytearray(buf.raw)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    out = sys.argv[1]
    if len(sys.argv) >= 6:
        x, y, w, h = (int(v) for v in sys.argv[2:6])
    else:
        x, y = 0, 0
        w, h = user32.GetSystemMetrics(0), user32.GetSystemMetrics(1)
    px = grab(x, y, w, h)
    px[0::4], px[2::4] = px[2::4], px[0::4]        # BGRA -> RGBA
    write_png(out, w, h, bytes(px))
    print(f"saved {out} {w}x{h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
