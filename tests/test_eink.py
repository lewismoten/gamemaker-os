# SPDX-License-Identifier: MIT
"""Production e-ink client, fake device: no hardware or SDK modifications."""
import ctypes as C
import os
from pathlib import Path
import unittest

GAP = 2_100_000_000

class Eink(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.c = C.CDLL(str(Path(os.environ.get('GMOS_HOST', 'build/host')) / 'libeink.so'))
        c = cls.c
        c.fake_time.argtypes = [C.c_uint64]
        c.fake_at.restype = C.c_uint64
        c.fake_line.restype = C.c_char_p
        c.fake_page.restype = C.POINTER(C.c_uint8)
        c.fake_mark.restype = C.POINTER(C.c_uint8)
        c.ge_play.argtypes = [C.c_char_p]

    def setUp(self):
        self.c.fake_reset()

    def poll(self, ns):
        self.c.fake_time(ns)
        self.c.ge_poll()

    def counts(self):
        return [self.c.fake_count(i) for i in range(5)]

    def test_boot_library_play_save_and_return(self):
        c = self.c
        c.ge_init(1, 9)
        self.assertEqual(self.counts(), [1, 0, 1, 1, 0])
        page = bytes(c.fake_page()[:3904])
        self.assertTrue(any(page))
        # Error indicator and row padding remain white.
        self.assertTrue(all(not (page[y*32+x//8] & (128 >> (x%8)))
                            for y in range(8) for x in range(242, 250)))
        self.assertTrue(all(page[y*32+31] & 63 == 0 for y in range(122)))
        c.ge_library(12)
        self.poll(GAP-1)
        self.assertEqual(c.fake_count(2), 1)
        self.poll(GAP)
        self.assertEqual(c.fake_count(5), 1)  # clear the boot page first
        self.assertEqual(c.fake_count(6), 0)
        self.poll(2*GAP)
        self.assertEqual(c.fake_line(), b'12 GAMES')
        mark = bytes(c.fake_mark()[:200])
        self.assertTrue(any(mark))
        # The title mark and full-page boot castle are the same artwork.
        for y in range(40):
            for x in range(40):
                self.assertEqual(bool(mark[y*5+x//8] & (128 >> (x%8))),
                                 bool(page[(12+y)*32+(105+x)//8] & (128 >> ((105+x)%8))))
        c.ge_play(b'Castle Adventure Beyond')
        self.poll(3*GAP)
        self.assertEqual(c.fake_line(), b'CASTLE ADVENTURE')
        c.ge_saving(1)
        self.poll(4*GAP)
        self.assertEqual(c.fake_line(), b'SAVING')
        c.ge_saving(0)
        self.poll(5*GAP)
        self.assertEqual(c.fake_line(), b'CASTLE ADVENTURE')
        c.ge_library(12)
        self.poll(6*GAP)
        self.assertEqual(c.fake_line(), b'12 GAMES')
        count = c.fake_count(2)
        self.poll(100*GAP)
        self.assertEqual(c.fake_count(2), count)

    def test_unsupported_and_unavailable(self):
        c = self.c
        for major, minor in [(1, 8), (1, 0), (2, 9)]:
            c.ge_init(major, minor)
            c.ge_library(0)
            self.poll(100*GAP)
        self.assertEqual(self.counts(), [0]*5)
        for error in [-19, -38, -16]:  # ENODEV, ENOSYS, EBUSY
            c.fake_reset()
            c.fake_open_error(error)
            c.ge_init(1, 10)
            self.poll(100*GAP)
            self.assertEqual(self.counts(), [1, 0, 0, 0, 0])

    def test_device_disappears_on_page_or_title(self):
        c = self.c
        for stage in (0, 1, 2):
            c.fake_reset()
            if stage == 0:
                c.fake_error(-19)
            c.ge_init(1, 9)
            if stage:
                c.ge_library(0)
                if stage == 2:
                    self.poll(GAP)
                c.fake_error(-19)
                self.poll(stage*GAP)
            before = self.counts()
            self.assertEqual(before[1], 1)
            self.poll(100*GAP)
            self.assertEqual(self.counts(), before)

    def test_cadence_under_churn_and_eagain(self):
        c = self.c
        c.fake_error(-11)
        c.ge_init(1, 9)
        self.poll(GAP-1)
        self.assertEqual(c.fake_count(2), 1)
        self.poll(GAP)
        self.assertEqual(c.fake_count(3), 2)
        c.fake_error(0)
        self.poll(2*GAP)  # retry boot
        c.ge_library(0)
        self.poll(3*GAP)  # clear boot
        self.poll(4*GAP)  # first title
        c.fake_error(-11)
        for ms in range(8401, 180_001, 50):
            c.ge_library(ms)
            self.poll(ms*1_000_000)
        times = [c.fake_at(i) for i in range(c.fake_count(2))]
        self.assertTrue(all(b-a >= GAP for a, b in zip(times, times[1:])))
        self.assertTrue(all(sum(t <= u < t+60_000_000_000 for u in times) <= 30 for t in times))
        c.ge_library(180000)
        c.fake_error(0)
        self.poll(182*1_000_000_000)
        self.assertEqual(c.fake_line(), b'180000 GAMES')

    def test_latest_state_coalesces_and_clock_failure_defers(self):
        c = self.c
        c.ge_init(1, 9)
        c.ge_library(5)
        self.poll(GAP)  # clear the page
        c.ge_play(b'Old game')
        c.ge_play(b'new\xffgame')
        c.fake_clock_error(-5)
        self.poll(2*GAP)
        self.assertEqual(c.fake_count(2), 2)
        c.fake_clock_error(0)
        self.poll(2*GAP)
        self.assertEqual(c.fake_line(), b'NEW?GAME')

    def test_clear_eagain_keeps_page_then_retries_before_title(self):
        c = self.c
        c.ge_init(1, 9)
        c.ge_library(3)
        c.fake_error(-11)
        self.poll(GAP)
        self.assertEqual(c.fake_count(6), 1)
        self.assertEqual(c.fake_count(4), 0)
        c.fake_error(0)
        self.poll(2*GAP-1)
        self.assertEqual(c.fake_count(5), 1)
        self.poll(2*GAP)
        self.assertEqual(c.fake_count(6), 0)
        self.poll(3*GAP)
        self.assertEqual(c.fake_line(), b'3 GAMES')
