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
    def test_page_is_frame_plus_values(self):
        c = api(); b = (C.c_uint8 * N)(); c.ei_page(b, b'LIBRARY', 42, b'Dice Roller of the Deep and More')
        frame = (C.c_uint8 * N).in_dll(c, 'ei_status_frame')
        self.assertEqual(list(b[:48 * 32]), list(frame[:48 * 32]))  # mark and title untouched
        self.assertGreater(sum(px(b, x, y) for x in range(76, 250) for y in range(48, 62)), 50)
        self.assertEqual(sum(px(b, x, y) for x in range(76, 250) for y in range(62, 70)), 0)
        self.assertEqual(sum(px(b, x, y) for x in range(250 - 6, 250) for y in range(0, 8)), 0)  # kernel's error corner stays white
        # clipped to 14 characters: nothing past x = 76 + 14 * 12
        self.assertEqual(sum(px(b, x, y) for x in range(76 + 14 * 12, 250) for y in range(92, 106)), 0)
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
