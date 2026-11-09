# SPDX-License-Identifier: MIT
"""Host tests for the demo preview (src/session.c gs_demo_start; docs/demo.md): the details view
starts a game's demo .REC after 3 s with the game selected (the idle rule is main.c's, so
the ctypes test checks the pieces it is made of: the session's demo, its end and its stop),
any key stops it, and each frame matches the play core fed the same keys tick for tick. A game with no .REC gets
nothing extra. The fixture is Roll the Dice with a demo.rec made from tests/data/dice.keys and
that DICE.GAM already names in scene 0's file slot 11 (demo.rec), as PLAYGAME's Game.File(0,11); frames go to build/host/ as PNG files."""
import ctypes as C
import shutil, struct, unittest
from gmos_page import png
from test_play import lib, OUT, DICE, DATA, EV_KEY, ENTER, RIGHT, GS_DRAW, GS_EXIT, GS_PLAY, GS_DONE, tick_ns

END_TICK = 300

def demo_folder(rec=True):
    """A copy of DICE, with demo.rec (the name in slot 11) from dice.keys up to END_TICK and an Esc."""
    d = OUT / ('DICEDEMO' if rec else 'DICENODEMO')
    shutil.rmtree(d, ignore_errors=True); shutil.copytree(DICE, d)
    keys = []
    for l in (DATA / 'dice.keys').read_text().splitlines():
        if l.startswith('K '):
            _, t, k = l.split(); t = int(t)
            if t < END_TICK: keys.append((t, int(k)))
    if not rec: return d, keys
    keys.append((END_TICK, 1))
    (d / 'demo.rec').write_bytes(b''.join(struct.pack('<HB', t - p, k) for (t, k), p in zip(keys, [0] + [x[0] for x in keys])))
    return d, keys

class Demo:
    def __init__(self, folder, now=0):
        self.c = lib(); self.s = self.c.gmos_session_new(); self.folder = str(folder).encode(); self.log = []
        self.say = C.CFUNCTYPE(None, C.c_char_p)(lambda line: self.log.append(line.decode()))
        self.c.gmos_session_say(C.c_void_p(self.s), self.say)
        self.c.gs_demo_start.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_char_p, C.c_uint64]
        self.c.gs_demo_stop.argtypes = [C.c_void_p]
        self.result = self.c.gs_demo_start(self.s, C.cast(self.c.gmos_host_read, C.c_void_p), self.folder, b'DICE.GAM', now)
    def step(self, t): return self.c.gs_step(self.s, t)
    def ticks(self): return self.c.gmos_session_ticks(self.s)
    def stop(self): self.c.gs_demo_stop(self.s)
    def phase(self): return self.c.gmos_session_phase(self.s)
    def frame(self):
        px = C.string_at(self.c.gmos_session_px(self.s), 64000); pal = C.string_at(self.c.gmos_session_pal(self.s), 768)
        rgb = [(lambda v: v << 2 | v >> 4) for _ in range(1)][0]
        out = []
        for j in range(400):
            for i in range(640):
                c = px[(j >> 1) * 320 + (i >> 1)] * 3; out.append(rgb(pal[c] & 63) << 16 | rgb(pal[c + 1] & 63) << 8 | rgb(pal[c + 2] & 63))
        return out

def reference(folder, keys, n):
    """The play core fed the keys by hand: the view's hash after each of n ticks (a key at tick t is
    pressed before tick t, as the Pocket Player host test's K lines are)."""
    c = lib(); c.gp_open(C.cast(c.gmos_host_read, C.c_void_p), str(folder).encode(), b'DICE.GAM'); assert c.gp_begin() == 0
    hashes = []
    for t in range(1, n + 1):
        for kt, k in keys:
            if kt == t: c.gp_key(k & 127, 0 if k & 128 else 1)
        c.gp_tick(); hashes.append(c.gp_frame_hash())
    return hashes

class DemoPreview(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        OUT.mkdir(parents=True, exist_ok=True); cls.folder, cls.keys = demo_folder(); cls.bare, _ = demo_folder(False)
    def test_starts_after_3_s_idle_and_a_key_press_stops(self):
        c = lib(); c.gs_demo_idle_due.argtypes = [C.c_uint64, C.c_uint64]; c.gs_demo_stops.argtypes = [C.c_uint16, C.c_int32]
        s = 1_000_000_000
        self.assertEqual(c.gs_demo_idle_due(5 * s, 5 * s + 3 * s - 1), 0)
        self.assertEqual(c.gs_demo_idle_due(5 * s, 5 * s + 3 * s), 1)
        self.assertEqual(c.gs_demo_idle_due(5 * s, 4 * s), 0)       # the clock never goes back, but if it did: no preview
        for key in (ENTER, RIGHT, 1, 57): self.assertEqual(c.gs_demo_stops(EV_KEY, 1), 1, key)   # any key down
        self.assertEqual(c.gs_demo_stops(EV_KEY, 0), 0); self.assertEqual(c.gs_demo_stops(EV_KEY, 2), 0)  # not a release or a repeat
        self.assertEqual(c.gs_demo_stops(0, 1), 0)                  # not a key event (a sync)
    def test_the_gam_names_the_demo(self):
        c = lib(); c.gp_demo.restype = C.c_char_p
        c.gp_open(C.cast(c.gmos_host_read, C.c_void_p), str(self.folder).encode(), b'DICE.GAM'); self.assertEqual(c.gp_demo(), b'demo.rec')
        c.gp_open(C.cast(c.gmos_host_read, C.c_void_p), str(DICE).encode(), b'DICE.GAM'); self.assertEqual(c.gp_demo(), b'demo.rec')  # named, but DICE has no file: gs_demo_start fails below
    def test_no_rec_shows_nothing_extra(self):
        d = Demo(self.bare); self.assertEqual(d.result, -1); self.assertEqual(d.log, [])
        d = Demo(DICE); self.assertEqual(d.result, -1)
    def test_frames_match_the_core_tick_for_tick(self):
        want = reference(self.folder, self.keys, END_TICK - 1)
        d = Demo(self.folder); self.assertEqual(d.result, 0); self.assertEqual(d.phase(), GS_PLAY)
        got = []
        for t in range(1, END_TICK):
            self.assertEqual(d.step(tick_ns(t)) & GS_EXIT, 0, t); self.assertEqual(d.ticks(), t)
            got.append(lib().gp_frame_hash())
        self.assertEqual(got, want)
        self.assertGreater(len(set(got)), 10)   # the hero really moves
    def test_the_recordings_esc_ends_it(self):
        d = Demo(self.folder)
        for t in range(1, END_TICK): d.step(tick_ns(t))
        self.assertEqual(d.step(tick_ns(END_TICK)) & GS_EXIT, GS_EXIT); self.assertEqual(d.phase(), GS_DONE)
    def test_stop_ends_it_at_once(self):
        d = Demo(self.folder); d.step(tick_ns(10)); d.stop(); self.assertEqual(d.phase(), GS_DONE)
        self.assertEqual(d.step(tick_ns(11)), GS_EXIT)
    def test_ticks_run_at_the_play_rate_not_faster(self):
        d = Demo(self.folder, now=1000); d.step(1000 + tick_ns(5) - 1); self.assertEqual(d.ticks(), 4)
        d.step(1000 + tick_ns(5)); self.assertEqual(d.ticks(), 5)
    def test_the_details_page_shows_the_frames(self):
        # gm_draw_picture (the preview's per-tick redraw) paints what gm_draw paints for the same picture.
        from gmos_page import Page, W, H
        c = lib(); c.gm_draw_picture.argtypes = [C.c_void_p, C.c_uint32, C.c_void_p, C.c_void_p]
        p = Page(); p.add_folder(self.folder)
        p.finish(); p.key('enter')
        d = Demo(self.folder)
        for t in range(1, 61): d.step(tick_ns(t))
        px, pal = c.gmos_session_px(d.s), c.gmos_session_pal(d.s)
        full = (C.c_uint16 * (W * H))(); c.gm_draw(full, W, p.m, C.c_void_p(px), C.c_void_p(pal))
        page = (C.c_uint16 * (W * H))(); c.gm_draw(page, W, p.m, 0, 0)
        c.gm_draw_picture(page, W, C.c_void_p(px), C.c_void_p(pal))
        x, y, w, h = (C.c_uint32() for _ in range(4)); c.gm_picture_rect(C.byref(x), C.byref(y), C.byref(w), C.byref(h))
        self.assertEqual((x.value, y.value, w.value, h.value), (36, 68, 640, 400))
        self.assertEqual(list(full), list(page))
        png(OUT / 'demo-preview-details.png', list(full))
    def test_preview_frames_png(self):
        d = Demo(self.folder)
        for t in range(1, 121):
            d.step(tick_ns(t))
            if t in (1, 40, 120): png(OUT / f'demo-preview-t{t}.png', d.frame(), 640, 400)

if __name__ == '__main__': unittest.main()
