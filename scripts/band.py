"""Measure dark-pixel bounding boxes in a horizontal band of a BMP.

Usage: python scripts/band.py <a.bmp> <b.bmp> x0 x1 y0 y1
Prints per-image: ink width, height and left edge inside the band.
"""
import struct
import sys


def read_bmp(path):
    data = open(path, "rb").read()
    off = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    assert bpp == 32, bpp
    flip = h > 0
    h = abs(h)
    stride = w * 4
    return w, h, flip, data[off:off + stride * h]


def ink_box(path, x0, x1, y0, y1, thresh=120):
    w, h, flip, px = read_bmp(path)
    stride = w * 4
    minx, maxx, miny, maxy = 10 ** 9, -1, 10 ** 9, -1
    for y in range(y0, min(y1, h)):
        sy = (h - 1 - y) if flip else y
        row = px[sy * stride:(sy + 1) * stride]
        for x in range(x0, min(x1, w)):
            b, g, r = row[x * 4], row[x * 4 + 1], row[x * 4 + 2]
            lum = 0.299 * r + 0.587 * g + 0.114 * b
            if lum < thresh:
                if x < minx: minx = x
                if x > maxx: maxx = x
                if y < miny: miny = y
                if y > maxy: maxy = y
    if maxx < 0:
        return None
    return minx, maxx, miny, maxy


def main():
    a, b = sys.argv[1], sys.argv[2]
    x0, x1, y0, y1 = (int(v) for v in sys.argv[3:7])
    for path in (a, b):
        box = ink_box(path, x0, x1, y0, y1)
        if not box:
            print(f"{path.split(chr(92))[-1]}: no ink")
            continue
        minx, maxx, miny, maxy = box
        print(f"{path.split(chr(92))[-1]}: x={minx}..{maxx} (w={maxx - minx + 1}) "
              f"y={miny}..{maxy} (h={maxy - miny + 1})")


if __name__ == "__main__":
    main()
