"""Compares two captures of the same view (standard library only).

Usage: python tests/ui/png_diff.py A.png B.png [DIFF.png] [--crop OUT.png]

Prints the changed pixel count and bounding box. DIFF.png shows unchanged
pixels darkened and changed pixels in red. --crop writes A and B side by
side, cropped to the changed area (padded) and enlarged 3x, for inspection.
Captures come from the user's disc and stay in build/.
"""
import struct
import sys
import zlib


def read_png(path):
    data = open(path, "rb").read()
    pos, idat, width, height, ctype = 8, b"", 0, 0, 0
    while pos < len(data):
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        kind, chunk = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            width, height, depth, ctype = struct.unpack(">IIBB", chunk[:10])
            if depth != 8 or ctype not in (2, 6):
                raise ValueError(f"{path}: only 8-bit RGB/RGBA PNG")
        elif kind == b"IDAT":
            idat += chunk
        pos += 12 + n
    raw, bpp = zlib.decompress(idat), 4 if ctype == 6 else 3
    stride, rows, prev, i = width * bpp, [], bytearray(width * bpp), 0
    for _ in range(height):
        filt, line = raw[i], bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if filt == 1:
                line[x] = (line[x] + a) & 255
            elif filt == 2:
                line[x] = (line[x] + b) & 255
            elif filt == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif filt == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([tuple(line[x * bpp:x * bpp + 3]) for x in range(width)])
        prev = line
    return width, height, rows


def write_png(path, rows):
    height, width = len(rows), len(rows[0])
    raw = b"".join(b"\0" + bytes(v for px in row for v in px) for row in rows)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def main():
    args = sys.argv[1:]
    crop = None
    if "--crop" in args:
        k = args.index("--crop")
        crop = args[k + 1]
        del args[k:k + 2]
    wa, ha, a = read_png(args[0])
    wb, hb, b = read_png(args[1])
    if (wa, ha) != (wb, hb):
        sys.exit("different sizes")
    changed, box = 0, [wa, ha, -1, -1]
    out = []
    for y in range(ha):
        row = []
        for x in range(wa):
            if a[y][x] != b[y][x]:
                changed += 1
                box = [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)]
                row.append((255, 0, 0))
            else:
                row.append(tuple(v // 3 for v in a[y][x]))
        out.append(row)
    print(f"{changed} pixels differ" + (f", box x {box[0]}-{box[2]} y {box[1]}-{box[3]}" if changed else ""))
    if len(args) > 2:
        write_png(args[2], out)
    if crop and changed:
        x0, y0 = max(0, box[0] - 16), max(0, box[1] - 16)
        x1, y1 = min(wa, box[2] + 17), min(ha, box[3] + 17)
        rows = []
        for y in range(y0, y1):
            line = [px for px in a[y][x0:x1] for _ in range(3)] + [(255, 255, 0)] * 3 + \
                   [px for px in b[y][x0:x1] for _ in range(3)]
            rows += [line] * 3
        write_png(crop, rows)


if __name__ == "__main__":
    main()
