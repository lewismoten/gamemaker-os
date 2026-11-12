# SPDX-License-Identifier: MIT
"""Malformed picture regressions, with canaries and a deterministic loader clock.
No display, audio device, sanitizer, or fault-based memory probe is used.
PCX is unsupported in this tree; both its suffix and disguised bytes must fail.
"""
import ctypes as C
import random
import struct
import unittest
import zlib
from gmos_page import lib, Page, DETAILS, W, H

U8 = C.c_uint8
VOID = C.CFUNCTYPE(None)
NOW = C.CFUNCTYPE(C.c_uint64, C.c_void_p)
PRESENT = C.CFUNCTYPE(None, C.c_void_p)
READ = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_char_p, C.c_void_p, C.c_uint32)

class Fli(C.Structure):
    _fields_ = [('d', C.c_void_p)] + [(n, C.c_uint32) for n in
                ('n', 'w', 'h', 'frames', 'speed', 'next', 'found')]

class Progress(C.Structure):
    _fields_ = [('last', C.c_uint64), ('now', NOW), ('present', PRESENT), ('user', C.c_void_p)]


def chunk(kind, data):
    return struct.pack('<IH', 6 + len(data), kind) + data


def animation(chunks, w=3, h=3, frames=1, magic=0xaf11):
    head = bytearray(128)
    struct.pack_into('<5H', head, 4, magic, frames, w, h, 8)
    record = struct.pack('<IHH8x', 16 + len(chunks), 0xf1fa, 1)
    return bytes(head) + (record + chunks) * frames


def game(title=b'TITLE.FLI', menu=b''):
    head = bytearray(262)
    head[:4] = b'GM\x2c\x01'
    struct.pack_into('<H', head, 256, 3)
    scenes = bytearray(283 * 3)
    struct.pack_into('<H', scenes, 283 * 2, 10)  # one playable level
    names = b'\0' + title + b'\0' + menu + b'\0'
    struct.pack_into('<H', scenes, 43, 1)
    struct.pack_into('<H', scenes, 43 + 2 * 2, 2 + len(title))  # GP_MENU is scene 0 slot 2
    return bytes(head + scenes) + struct.pack('<H', len(names)) + names


def guarded(n):
    buf = (U8 * (n + 32))(*([0xa5] * (n + 32)))
    C.memset(C.addressof(buf) + 16, 0, n)
    return buf, C.c_void_p(C.addressof(buf) + 16)


class Bounds(unittest.TestCase):
    def setUp(self):
        self.c = lib()
        self.c.gp_fli_open.argtypes = [C.POINTER(Fli), C.c_void_p, C.c_uint32]
        self.c.gp_fli_frame.argtypes = [C.POINTER(Fli), C.c_void_p, C.c_void_p]
        self.c.gp_gif.argtypes = [C.c_void_p, C.c_uint32, C.c_void_p, C.c_void_p]
        self.c.gp_decode_hook.argtypes = [C.c_void_p]
        self.c.gm_load_start.argtypes = [C.POINTER(Progress), NOW, PRESENT, C.c_void_p]
        self.c.gm_load_tick.argtypes = [C.POINTER(Progress)]

    def check_canary(self, buf):
        self.assertEqual(bytes(buf[:16]) + bytes(buf[-16:]), b'\xa5' * 32)

    def decode(self, data):
        f = Fli()
        src = C.create_string_buffer(data)
        if self.c.gp_fli_open(C.byref(f), src, len(data)):
            return
        px, p = guarded(f.w * f.h)
        pal, q = guarded(768)
        # A few frames cover state transitions without trusting the advertised count.
        for _ in range(4):
            self.c.gp_fli_frame(C.byref(f), p, q)
            self.check_canary(px)
            self.check_canary(pal)
        self.assertLessEqual(f.next, max(len(data), struct.unpack_from('<I', data, 80)[0]))

    def test_truncated_and_mutated_fli_packets(self):
        rng = random.Random(1110)
        # Every decoder, including odd-width SS2's final byte and 256-color packets.
        seeds = [animation(chunk(k, d)) for k, d in (
            (4, b'\x01\0\0\0' + bytes(range(256)) * 3),
            (11, b'\xff\xff\xff\xff' + b'\xff' * 30),
            (12, b'\0\0\x03\0\x01\0\x03\x01\x02\x03'),
            (7, b'\x01\0\x09\x80\x01\0\0\x01\x01\x02'),
            (13, b''), (15, b'\x01\x03\x07' * 3), (16, bytes(range(9))))]
        for seed in seeds:
            for n in range(len(seed) + 1):
                self.decode(seed[:n])
            for _ in range(100):
                data = bytearray(seed)
                for _ in range(rng.randrange(1, 8)):
                    data[rng.randrange(len(data))] = rng.randrange(256)
                self.decode(bytes(data))
        for w, h in ((65535, 65535), (64000, 1), (1, 64000), (319, 199), (0, 0)):
            self.decode(animation(chunk(16, b'abc'), w, h))
        for offset in (0xfffffffc, 0xfffffff0, 127, 129):
            data = bytearray(animation(chunk(13, b''), magic=0xaf12))
            struct.pack_into('<I', data, 80, offset)
            self.decode(bytes(data))

    def test_palette_cannot_consume_the_next_chunk(self):
        # An empty color packet followed by pixel bytes must leave the palette alone.
        data = animation(chunk(11, b'\x01\0\0\x01') + chunk(16, b'\x3f' * 9))
        data = bytearray(data)
        struct.pack_into('<H', data, 128 + 6, 2)
        f = Fli(); src = C.create_string_buffer(bytes(data))
        self.assertEqual(self.c.gp_fli_open(C.byref(f), src, len(data)), 0)
        px = (U8 * 9)(); pal = (U8 * 768)()
        self.c.gp_fli_frame(C.byref(f), px, pal)
        self.assertEqual(bytes(pal), bytes(768))
        self.assertEqual(bytes(px), b'\x3f' * 9)

    def test_fuzzed_truncated_pcx_is_rejected(self):
        rng = random.Random(0x0a)
        pcx = bytearray(128)
        pcx[:4] = b'\x0a\x05\x01\x08'
        struct.pack_into('<4H', pcx, 4, 0, 0, 319, 199)
        pcx[65] = 1
        struct.pack_into('<H', pcx, 66, 320)
        pcx += b'\xff\xff' * 100 + b'\x0c' + bytes(range(256)) * 3
        px, p = guarded(64000); pal, q = guarded(768)
        for i in range(len(pcx) + 300):
            data = bytearray(pcx[:i]) if i < len(pcx) else bytearray(rng.randbytes(rng.randrange(2048)))
            if len(data) >= 4:
                data[:4] = b'\x0a\x05\x01\x08'
            src = C.create_string_buffer(bytes(data))
            self.assertEqual(self.c.gp_gif(src, len(data), q, p), -1)
            self.check_canary(px); self.check_canary(pal)
        # Unsupported suffix never reaches an image decoder; disguised PCX is rejected too.
        for suffix in (b'.PCX', b'.GIF', b'.FLI'):
            files = {b'GAME.GAM': game(b'TITLE' + suffix), b'TITLE' + suffix: bytes(pcx)}
            @READ
            def read(_, name, dest, cap):
                d = files.get(name)
                if d is None: return -1
                C.memmove(dest, d, min(cap, len(d)))
                return min(cap, len(d))
            self.assertEqual(self.c.gm_title_picture(read, None, b'GAME.GAM', p, q), -1)

    def test_text_extreme_coordinates_and_long_lines(self):
        self.c.gp_write.argtypes = [C.c_void_p, C.c_int32, C.c_int32, U8, C.c_char_p, C.c_uint32]
        px, p = guarded(64000)
        for x, y in ((2147483647, 2147483647), (-2147483648, -2147483648), (319, 199)):
            data = b'x' * 65536 + b'\n' * 65536
            self.c.gp_write(p, x, y, 7, data, len(data))
            self.check_canary(px)

    def test_details_presents_during_slow_animation_and_file_fallbacks(self):
        page = Page(); page.add('FW', 'Fireworks Night'); page.finish(); page.key('enter')
        self.assertEqual(page.view, DETAILS)
        surface = (C.c_uint16 * (W * H))()
        self.c.gm_draw(surface, W, page.m, None, None)
        files = {b'GAME.GAM': game(b'TITLE.LST'),
                 b'TITLE.LST': b''.join(b'MISS%02d.GIF 1\n' % i for i in range(31)) + b'TITLE.FLI 0\n',
                 b'TITLE.FLI': animation(chunk(16, bytes(range(256)) * 25), 32, 200, 120)}
        clock = [0]; presents = []; reads = []; progress = Progress()
        @NOW
        def now(_):
            clock[0] += 5_000_000  # simulated decode work between checkpoints
            return clock[0]
        @PRESENT
        def present(_): presents.append(clock[0])
        @VOID
        def checkpoint(): self.c.gm_load_tick(C.byref(progress))
        @READ
        def read(_, name, dest, cap):
            reads.append((name, clock[0]))
            d = files.get(name)
            # Model the board transport's bounded reads and checkpoints, including a miss.
            for at in range(0, max(1, min(cap, len(d) if d else 0)), 16384):
                clock[0] += 20_000_000
                checkpoint()
            if d is None: return -1
            C.memmove(dest, d, min(cap, len(d)))
            return min(cap, len(d))
        self.c.gm_load_start(C.byref(progress), now, present, None)
        self.c.gp_decode_hook(checkpoint)
        px, p = guarded(64000); pal, q = guarded(768)
        try:
            self.assertEqual(self.c.gm_title_picture(read, None, b'GAME.GAM', p, q), 0)
        finally:
            self.c.gp_decode_hook(None)
        presents.append(clock[0])  # final completed page
        self.assertLessEqual(presents[0], 100_000_000)
        self.assertLess(presents[0], reads[0][1])
        self.assertGreater(len(reads), 30)
        self.assertGreater(len(presents), 120)
        self.assertLessEqual(max(b - a for a, b in zip(presents, presents[1:])), 100_000_000)
        self.check_canary(px); self.check_canary(pal)

    def test_inflate_checkpoints_preserve_output(self):
        self.c.puff_yield.argtypes = [C.c_void_p, C.POINTER(C.c_ulong), C.c_void_p,
                                     C.POINTER(C.c_ulong), VOID]
        rng = random.Random(1110)
        # Stored, fixed Huffman, and dynamic Huffman blocks, as used by GMP/ZIP.
        for level, strategy in ((0, zlib.Z_DEFAULT_STRATEGY), (6, zlib.Z_FIXED),
                                (6, zlib.Z_DEFAULT_STRATEGY)):
            raw = rng.randbytes(32768) * 8 if level == 0 else b'Fireworks Night! ' * 32768
            compressor = zlib.compressobj(level, zlib.DEFLATED, -15, strategy=strategy)
            packed = compressor.compress(raw) + compressor.flush()
            src = C.create_string_buffer(packed)
            out, dest = guarded(len(raw)); dn = C.c_ulong(len(raw)); sn = C.c_ulong(len(packed))
            calls = []
            @VOID
            def checkpoint(): calls.append(1)
            self.assertEqual(self.c.puff_yield(dest, C.byref(dn), src, C.byref(sn), checkpoint), 0)
            self.assertEqual((dn.value, sn.value), (len(raw), len(packed)))
            self.assertEqual(C.string_at(dest, len(raw)), raw)
            self.assertGreater(len(calls), 4)
            self.check_canary(out)

    def test_gif_truncations_and_extreme_offsets(self):
        from test_play import DICE
        raw = (DICE / 'TITLE.GIF').read_bytes()
        self.c.gp_gif_draw.argtypes = [C.c_void_p, C.c_uint32, C.c_int32, C.c_int32,
                                      C.c_void_p, C.c_void_p]
        rng = random.Random(320200)
        px, p = guarded(64000); pal, q = guarded(768)
        for i in range(200):
            data = bytearray(raw[:rng.randrange(len(raw) + 1)])
            if data and i % 2:
                for _ in range(4): data[rng.randrange(len(data))] = rng.randrange(256)
            src = C.create_string_buffer(bytes(data))
            x, y = rng.choice(((0, 0), (2147483647, -2147483648), (-2147483648, 2147483647)))
            self.c.gp_gif_draw(src, len(data), x, y, q, p)
            self.check_canary(px); self.check_canary(pal)
