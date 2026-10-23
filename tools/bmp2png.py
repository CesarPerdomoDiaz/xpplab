"""Convert an uncompressed 24/32-bit BMP (as written by SDL_SaveBMP) to PNG.
Needs only NumPy and the standard library.

    python tools/bmp2png.py in.bmp out.png [scale]
"""
import struct
import sys
import zlib

import numpy as np


def read_bmp(path):
    data = open(path, "rb").read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if bpp not in (24, 32):
        raise SystemExit(f"unsupported BMP: {bpp} bits per pixel")
    stride = ((width * bpp // 8) + 3) & ~3
    rows = np.frombuffer(data, dtype=np.uint8, count=stride * abs(height),
                         offset=offset).reshape(abs(height), stride)
    px = rows[:, : width * bpp // 8].reshape(abs(height), width, bpp // 8)
    if compression == 3 and bpp == 32:  # BI_BITFIELDS: read the masks
        masks = struct.unpack_from("<III", data, 54)
        order = [int(np.log2(m) // 8) for m in masks]  # byte of R, G, B
        rgb = px[:, :, order]
    else:  # BGR(A)
        rgb = px[:, :, [2, 1, 0]]
    if height > 0:  # bottom-up
        rgb = rgb[::-1]
    return np.ascontiguousarray(rgb)


def write_png(path, rgb):
    h, w, _ = rgb.shape
    raw = b"".join(b"\x00" + rgb[y].tobytes() for y in range(h))

    def chunk(tag, payload):
        c = struct.pack(">I", len(payload)) + tag + payload
        return c + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


if __name__ == "__main__":
    img = read_bmp(sys.argv[1])
    if len(sys.argv) > 3:
        s = int(sys.argv[3])
        img = img[::s, ::s]
    write_png(sys.argv[2], img)
