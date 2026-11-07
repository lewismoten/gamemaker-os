#!/usr/bin/env python3
# SPDX-License-Identifier: MIT; a jsrun --shot file (any mode: 32 bpp XRGB8888,
# 16 bpp RGB565, 8 bpp with its palette) to a PNG, optionally scaled up by a
# whole number (nearest neighbor, as the board's GPU would with scaling_kernel=8).
import struct, sys, zlib

def main(src, out, scale=1):
    d = open(src, 'rb').read()
    magic, w, h, depth = struct.unpack_from('<4I', d)
    assert magic == 0x534a4d47, 'not a jsrun shot'
    pal, px = d[16:16 + 768], d[16 + 768:]
    rows = []
    for y in range(h):
        row = bytearray()
        for x in range(w):
            if depth == 32:
                b, g, r = px[(y * w + x) * 4:(y * w + x) * 4 + 3]
            elif depth == 16:
                v = struct.unpack_from('<H', px, (y * w + x) * 2)[0]
                r, g, b = (v >> 11) << 3 | v >> 13, (v >> 5 & 63) << 2 | (v >> 9 & 3), (v & 31) << 3 | (v >> 2 & 7)
            else:
                i = px[y * w + x]
                r, g, b = ((c & 63) << 2 | (c & 63) >> 4 for c in pal[i * 3:i * 3 + 3])
            row += bytes((r, g, b)) * scale
        rows += [bytes(row)] * scale
    raw = b''.join(b'\0' + r for r in rows)
    chunk = lambda t, b: struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b))
    open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w * scale, h * scale, 8, 2, 0, 0, 0))
                          + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 1)
