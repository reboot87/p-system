"""Fast screen sampler: reuses one memory DC so a sample costs ~1 ms.

Usage: probe_flash.py <x> <y> [reps]
Prints per-frame luminance / near-white share / dark-ink share for a few
watched bands, plus the sample interval, so a transient wipe can be caught.
"""
import ctypes
import sys
import time

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
SRCCOPY = 0x00CC0020


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", ctypes.c_uint32), ("biWidth", ctypes.c_int32),
        ("biHeight", ctypes.c_int32), ("biPlanes", ctypes.c_uint16),
        ("biBitCount", ctypes.c_uint16), ("biCompression", ctypes.c_uint32),
        ("biSizeImage", ctypes.c_uint32), ("biXPelsPerMeter", ctypes.c_int32),
        ("biYPelsPerMeter", ctypes.c_int32), ("biClrUsed", ctypes.c_uint32),
        ("biClrImportant", ctypes.c_uint32),
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", ctypes.c_uint32 * 3)]


_sdc = user32.GetDC(None)


class Band:
    def __init__(self, label, x, y, w, h, step=5):
        self.label, self.x, self.y, self.w, self.h = label, x, y, w, h
        self.mem = gdi32.CreateCompatibleDC(_sdc)
        self.bmp = gdi32.CreateCompatibleBitmap(_sdc, w, h)
        gdi32.SelectObject(self.mem, self.bmp)
        self.bi = BITMAPINFO()
        self.bi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        self.bi.bmiHeader.biWidth = w
        self.bi.bmiHeader.biHeight = -h
        self.bi.bmiHeader.biPlanes = 1
        self.bi.bmiHeader.biBitCount = 32
        self.buf = ctypes.create_string_buffer(w * h * 4)
        self.step = step

    def sample(self):
        gdi32.BitBlt(self.mem, 0, 0, self.w, self.h, _sdc, self.x, self.y, SRCCOPY)
        gdi32.GetDIBits(self.mem, self.bmp, 0, self.h, self.buf,
                        ctypes.byref(self.bi), 0)
        raw = self.buf.raw
        stride = 4 * self.step
        lum = white = dark = cnt = 0
        for i in range(0, self.w * self.h * 4, stride):
            b, g, r = raw[i], raw[i + 1], raw[i + 2]
            l = (r * 299 + g * 587 + b * 114) // 1000
            lum += l
            if l >= 248:
                white += 1
            if l < 140:
                dark += 1
            cnt += 1
        return lum / cnt, white / cnt, dark / cnt


def main():
    hwnd = user32.FindWindowW("AireRecordWnd", None)
    if not hwnd:
        print("no window")
        return 1
    cx = int(sys.argv[1]) if len(sys.argv) > 1 else 582
    cy = int(sys.argv[2]) if len(sys.argv) > 2 else 80
    reps = int(sys.argv[3]) if len(sys.argv) > 3 else 1

    bands = [
        Band("list", 240, 110, 640, 90),
        Band("edit", 230, 62, 300, 30),
        Band("panel", 1600, 110, 300, 90),
    ]
    for b in bands:
        b.sample()
        b.sample()

    for rep in range(reps):
        lp = (cy << 16) | cx
        t0 = time.perf_counter()
        user32.PostMessageW(hwnd, 0x0201, 1, lp)
        user32.PostMessageW(hwnd, 0x0202, 0, lp)
        print("---- rep %d ----" % rep)
        for _ in range(60):
            t = 1000 * (time.perf_counter() - t0)
            line = ["%7.1f" % t]
            for b in bands:
                lum, w, d = b.sample()
                line.append("%s %5.1f/%.2f/%.3f" % (b.label, lum, w, d))
            print(" ".join(line))
        time.sleep(0.5)
    return 0


if __name__ == "__main__":
    sys.exit(main())
