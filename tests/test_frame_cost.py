# SPDX-License-Identifier: MIT
"""Worst-case frame cost of the details view and its demo preview, on the host build.
The board has no hardware divide: a division per pixel (the details panel's gradient) took over a
second a frame and missed the kernel's 2 s heartbeat ("FAULT.TXT: heartbeat missed 2 s", PC in
__divsi3, First Steps' details page). view.c counts pixel stores and software divisions
(gm_view_work); a frame must stay inside the budgets below. The preview must draw after a stall,
never owe the missed ticks, and the title screen's decode must checkpoint (feed audio, present)
often. First Steps (the first Starter) runs too when its built disk is there (GMOS_FIRST_STEPS)."""
import ctypes as C, os, pathlib, time, unittest
import gmos_page as gp
from test_demo import Demo, demo_folder
from test_play import GS_DRAW, tick_ns

STEPS = pathlib.Path(os.environ.get('GMOS_FIRST_STEPS', pathlib.Path(__file__).resolve().parents[2] / 'GameMaker/games/first-steps/disk'))
MAX_DIVS = 5000          # per frame: the panel's column colors only (it was 2.4 million)
MAX_STORES = 1_700_000   # per frame: the whole page, the doubled picture included
MAX_GAP_MS = 8.0         # host ms between decode checkpoints (the board is about 200x slower)

def work(c, draw):
    c.gm_view_work.argtypes = [C.c_void_p]; c.gm_view_work.restype = C.c_uint32
    c.gm_view_work_reset(); draw(); d = C.c_uint32()
    return c.gm_view_work(C.byref(d)), d.value

class FrameCost(unittest.TestCase):
    def folders(self):
        from test_play import DICE
        out = [('DICE', DICE)]
        if STEPS.is_dir(): out.append(('STEPS', STEPS))
        return out
    def test_page_frames_do_no_unbounded_work(self):
        for name, folder in self.folders():
            with self.subTest(game=name):
                P = gp.Page(); P.add_folder(folder, name); P.finish(); c = P.c
                P.load_page()
                stores, divs = work(c, lambda: P.render(load=False))
                self.assertEqual(divs, 0, 'grid frame divides'); self.assertLessEqual(stores, MAX_STORES)
                P.keys('enter'); self.assertEqual(P.view, gp.DETAILS)
                pic = P.title_picture(0); big, pal = pic if pic else (None, None)
                buf = (C.c_uint16 * (gp.W * gp.H))()
                stores, divs = work(c, lambda: c.gm_draw(buf, gp.W, P.m, big, pal))
                print(f'{name} details frame: {stores} stores, {divs} divisions')
                self.assertLessEqual(divs, MAX_DIVS); self.assertLessEqual(stores, MAX_STORES)
                c.gm_draw_picture.argtypes = [C.c_void_p, C.c_uint32, C.c_void_p, C.c_void_p]
                stores, divs = work(c, lambda: c.gm_draw_picture(buf, gp.W, big, pal))
                self.assertEqual(divs, 0); self.assertLessEqual(stores, 640 * 400 + 5000)
    def test_title_decode_checkpoints(self):
        for name, folder in self.folders():
            with self.subTest(game=name):
                c = gp.lib(); gaps = []; last = [time.perf_counter()]
                def hook(): now = time.perf_counter(); gaps.append(now - last[0]); last[0] = now
                H = C.CFUNCTYPE(None)(hook); c.gp_decode_hook(H)
                try:
                    px, pal = (C.c_uint8 * 64000)(), (C.c_uint8 * 768)()
                    gam = gp.gam_of(folder).name.encode(); last[0] = time.perf_counter()
                    self.assertEqual(c.gm_title_picture(C.cast(c.gmos_host_read, C.c_void_p), C.c_char_p(str(folder).encode()), gam, px, pal), 0)
                finally: c.gp_decode_hook(None)
                self.assertTrue(gaps); self.assertLess(max(gaps) * 1000, MAX_GAP_MS)
    def test_demo_draws_after_a_stall_and_owes_nothing(self):
        folder, _ = demo_folder(); d = Demo(folder); self.assertEqual(d.result, 0)
        n = 0
        for _ in range(3): n += 1; d.step(tick_ns(n))
        before = d.ticks(); n += 30 * 1_000_000_000 // tick_ns(1)  # a 30 s stall
        r = d.step(tick_ns(n))
        self.assertTrue(r & GS_DRAW, 'the first step after a stall draws')
        self.assertLessEqual(d.ticks() - before, 4, 'at most four ticks per step')
        got = d.ticks(); steps = 0
        while d.ticks() < got + 8 and steps < 40:
            n += 1; d.step(tick_ns(n)); steps += 1
        self.assertLessEqual(steps, 9, 'it plays on at once, with no debt to pay down')

    def test_loading_placeholder_and_title_after_a_demo(self):
        P = gp.Page(); P.add_folder(__import__('test_play').DICE, 'DICE'); P.finish(); c = P.c
        P.keys('enter')
        c.gm_view_picture_loading.argtypes = [C.c_int]
        self.assertIsNotNone(P.title_picture(0), 'DICE has a title picture')
        # P.render() fetches the picture itself; the placeholder is what draws when it is not there.
        buf1, buf2 = (C.c_uint16 * (gp.W * gp.H))(), (C.c_uint16 * (gp.W * gp.H))()
        c.gm_view_picture_loading(1); c.gm_draw(buf1, gp.W, P.m, None, None)
        c.gm_view_picture_loading(0); c.gm_draw(buf2, gp.W, P.m, None, None)
        self.assertNotEqual(bytes(buf1), bytes(buf2), '"Loading..." is not "No title screen"')
        folder, _ = demo_folder(); d = Demo(folder); self.assertEqual(d.result, 0)
        d.step(tick_ns(1)); d.stop()
        px, pal = (C.c_uint8 * 64000)(), (C.c_uint8 * 768)()
        self.assertEqual(c.gm_title_picture(C.cast(c.gmos_host_read, C.c_void_p), str(folder).encode(), b'DICE.GAM', px, pal), 0, 'the title screen reads back after a demo')
