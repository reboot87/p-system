"""List contiguous row ranges containing dark ink inside an x window.

Usage: python scripts/rows.py <img.bmp> <x0> <x1> [thresh]
Then feed the interesting ranges to band.py for width comparison.
"""
import struct
import sys


def read_bmp(path):
    data = open(path, "rb").read()
    off = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    assert bpp == 32, bpp
    stride = w * 4
    flip = h > 0
    h = abs(h)
    px = data[off:off + stride * h]
    return w, h, flip, stride, px


def main():
    path = sys.argv[1]
    x0, x1 = int(sys.argv[2]), int(sys.argv[3])
    thresh = int(sys.argv[4]) if len(sys.argv) > 4 else 140

    w, h, flip, stride, px = read_bmp(path)
    x1 = min(x1, w)
    rows = []
    for y in range(h):
        sy = (h - 1 - y) if flip else y
        base = sy * stride
        hit = False
        for x in range(x0, x1):
            i = base + x * 4
            b, g, r = px[i], px[i + 1], px[i + 2]
            if (r * 299 + g * 587 + b * 114) // 1000 < thresh:
                hit = True
                break
        rows.append(hit)

    y = 0
    while y < h:
        if not rows[y]:
            y += 1
            continue
        start = y
        while y < h and rows[y]:
            y += 1
        # Ignore 1px hairlines (separators / borders).
        if y - start >= 3:
            print("y=%d..%d (h=%d)" % (start, y - 1, y - start))


if __name__ == "__main__":
    main()
