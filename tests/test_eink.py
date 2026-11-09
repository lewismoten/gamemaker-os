# SPDX-License-Identifier: MIT
"""The e-ink status page and its update policy (src/eink.c) on the host: the page is the approved frame plus the
values, the layout is the kernel's (3904 bytes, 32-byte rows, 1 = black), and a page is asked for only after the
selection has been still 2 s, no sooner than 5 s after the last one, and not again until the retry wait after a refusal."""
import ctypes as C, os, pathlib, unittest
from gmos_page import lib, ROOT, OUT
N = 3904; MS = 1000000
class S(C.Structure): _fields_ = [('page', C.c_uint8 * N), ('sent', C.c_uint8 * N), ('have', C.c_uint8), ('ch', C.c_uint64), ('st', C.c_uint64), ('rt', C.c_uint64)]
def api():
    c = lib()
    c.ei_page.argtypes = [C.c_void_p, C.c_char_p, C.c_uint, C.c_char_p]
    c.ei_init.argtypes = [C.c_void_p]
    c.ei_update.argtypes = [C.c_void_p, C.c_uint64, C.c_uint, C.c_char_p]; c.ei_update.restype = C.c_int
    c.ei_sent.argtypes = [C.c_void_p, C.c_uint64, C.c_int]
    return c
def px(page, x, y): return page[y * 32 + x // 8] >> (7 - x % 8) & 1
class Eink(unittest.TestCase):
    def page(self, title, games=137):
        c = api(); b = (C.c_uint8 * N)(); c.ei_page(b, b'LIBRARY', games, title); return b
    def ink(self, b, y0, y1, x0=0, x1=250): return sum(px(b, x, y) for x in range(x0, x1) for y in range(y0, y1))
    def shot(self, name, b):
        d = os.environ.get('GMOS_SHOTS')
        if not d: return
        from PIL import Image
        im = Image.new('1', (250, 122), 1)
        for y in range(122):
            for x in range(250):
                if px(b, x, y): im.putpixel((x, y), 0)
        pathlib.Path(d).mkdir(parents=True, exist_ok=True); im.resize((750, 366), 0).save(pathlib.Path(d) / name)
    def test_one_line(self):
        b = self.page(b'Depth Perception'); self.shot('eink-status-one-line.png', b)
        frame = (C.c_uint8 * N).in_dll(api(), 'ei_status_frame')
        self.assertEqual(list(b[:20 * 32]), list(frame[:20 * 32]))  # mark and name
        self.assertGreater(self.ink(b, 26, 33, 50), 20)               # mode, 1x
        self.assertGreater(self.ink(b, 46, 60), 50)                   # GAMES: 137
        self.assertGreater(self.ink(b, 66, 80), 50)                   # the title
        self.assertEqual(self.ink(b, 86, 122), 0)                     # no second line
        self.assertEqual(self.ink(b, 0, 8, 242), 0)                   # kernel's error corner stays white
    def test_wraps_at_word_boundary(self):
        b = self.page(b'Virus Blaster Deluxe Edition Now'); self.shot('eink-status-two-lines.png', b)
        self.assertGreater(self.ink(b, 66, 80), 50); self.assertGreater(self.ink(b, 86, 100), 50)
        one = self.page(b'VIRUS BLASTER DELUXE')
        self.assertEqual(list(one[66 * 32:80 * 32]), list(b[66 * 32:80 * 32]))  # the first line breaks after DELUXE
        self.assertEqual(self.ink(b, 100, 122), 0)
    def test_long_title_ends_in_ellipsis(self):
        t = b'The Very Long Adventures of a Fly Named Carl and Friends Forever'
        b = self.page(t); self.shot('eink-status-ellipsis.png', b)
        self.assertEqual(self.ink(b, 100, 122), 0); self.assertEqual(self.ink(b, 86, 100, 244), 0)
        exact = self.page(b'THE VERY LONG ADVENTURES')   # line 2 is ADVENTURES
        plain = self.page(b'THE VERY LONG ADVENTURES OF A FLY')
        self.assertNotEqual(list(b[86 * 32:100 * 32]), list(exact[86 * 32:100 * 32]))
        self.assertEqual(self.ink(plain, 100, 122), 0)
    def test_mark_is_the_40x40_pbm(self):
        m = bytes((C.c_uint8 * 200).in_dll(api(), 'ei_mark')); self.assertEqual(m, (ROOT / 'art/eink/eink-mark-40.pbm').read_bytes()[-200:])
    def test_policy(self):
        c = api(); s = S(); c.ei_init(C.byref(s)); t = 1000 * MS; g = b'DICE'
        self.assertEqual(c.ei_update(C.byref(s), t, 5, g), 0)             # just changed
        self.assertEqual(c.ei_update(C.byref(s), t + 1900 * MS, 5, g), 0)
        self.assertEqual(c.ei_update(C.byref(s), t + 2000 * MS, 5, g), 1)  # settled 2 s
        c.ei_sent(C.byref(s), t + 2000 * MS, 1)
        self.assertEqual(c.ei_update(C.byref(s), t + 2100 * MS, 5, g), 0)  # nothing new
        self.assertEqual(c.ei_update(C.byref(s), t + 3000 * MS, 5, b'NEXT'), 0)  # changed again, settling
        self.assertEqual(c.ei_update(C.byref(s), t + 5100 * MS, 5, b'NEXT'), 0)  # settled but under 5 s since the last page
        self.assertEqual(c.ei_update(C.byref(s), t + 7000 * MS, 5, b'NEXT'), 1)
        c.ei_sent(C.byref(s), t + 7000 * MS, 0)                            # the kernel said too soon
        self.assertEqual(c.ei_update(C.byref(s), t + 12000 * MS, 5, b'NEXT'), 0)
        self.assertEqual(c.ei_update(C.byref(s), t + 17000 * MS, 5, b'NEXT'), 1)
        # a flicking selection never sends
        s2 = S(); c.ei_init(C.byref(s2)); now = 100000 * MS; sent = 0
        for i in range(30): now += 500 * MS; sent += c.ei_update(C.byref(s2), now, 5, b'G%d' % i)
        self.assertEqual(sent, 0)
if __name__ == '__main__': unittest.main()
