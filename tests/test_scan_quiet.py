# SPDX-License-Identifier: MIT
"""While the Archive scan has files left, the page shows one calm note in its header and holds the
Archive and Search counts back; with none left the page is the same as if no scan had run."""
import ctypes as C
import unittest
from gmos_page import lib, crc
from test_gmos import many

def frame(p, left):
    c = lib(); c.gmos_scan_left.argtypes = [C.c_void_p, C.c_uint]; c.gmos_scan_left(p.m, left)
    return p.render()

class ScanQuiet(unittest.TestCase):
    def test_note_appears_only_while_files_are_left(self):
        p = many(12)
        done, scanning, again = frame(p, 0), frame(p, 37), frame(p, 0)
        self.assertEqual(crc(done), crc(again))           # nothing lingers after the scan
        self.assertNotEqual(crc(done), crc(scanning))
    def test_the_note_and_held_counts_touch_only_the_header_and_tabs(self):
        p = many(12); done, scanning = frame(p, 0), frame(p, 37)
        from gmos_page import W
        changed = {i // W for i in range(len(done)) if done[i] != scanning[i]}
        self.assertTrue(changed and max(changed) < 90, sorted(changed)[-1])   # the header and the tab strip (the Search count waits), never the cards
    def test_the_note_counts_down_in_one_place(self):
        p = many(12); a, b = frame(p, 37), frame(p, 36)
        from gmos_page import W
        self.assertTrue({i // W for i in range(len(a)) if a[i] != b[i]} and max(i // W for i in range(len(a)) if a[i] != b[i]) < 40)

class PausedDuringPlay(unittest.TestCase):
    def test_archive_reads_run_only_in_the_idle_list_loop(self):
        import pathlib, re
        src = (pathlib.Path(__file__).resolve().parents[1] / 'src/main.c').read_text()
        calls = [m.start() for m in re.finditer(r'arch_step\(\)', src)]
        loop = src.index('for (;;) {\n    awake_set(demo_on);')
        idle = [c for c in calls if c > loop]
        self.assertEqual(len(idle), 1)                                  # one caller in the loop, none in play(), demos or the key handler
        self.assertLess(src.index('if (demo_run(px))', loop), idle[0])  # after the demo branch has `continue`d: no reads while a demo runs
        self.assertLess(src.index('play((uint32_t *)surface, g)', loop), len(src))
        self.assertFalse(re.search(r'arch_step', src[src.index('static void play('):src.index('static void play(') + 4000]))  # play() never steps the scan
        self.assertIn('cache_flush();', src[idle[0]:idle[0] + 900])    # the cache is written once, when the last file is read

if __name__ == '__main__': unittest.main()
