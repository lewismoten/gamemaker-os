# SPDX-License-Identifier: MIT
"""Host tests for playing a game: the shared Game-Maker play core fed from a
game's original DOS files (src/game.cpp), the GIF cards, the session (cards,
18.2 Hz ticks, keys, Esc) and the 640x480 layout, rendered to PNG files in
build/host/. The fixture is the Starter "Roll the Dice" (tests/games/DICE);
the Pocket Player's own host test, run on its packed copy of the same game,
wrote tests/data/dice-pocket.txt and dice-cards.txt (tools/pocket_reference.py)."""
import ctypes as C
import pathlib, struct, subprocess, unittest, zlib
from test_gmos import png, W, H
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/host'
DICE = ROOT / 'tests/games/DICE'
DATA = ROOT / 'tests/data'
TICK_NS10 = 549254157
GS_DRAW, GS_EXIT = 1, 2
GS_CARDS, GS_PLAY, GS_DONE = range(3)
EV_KEY, RIGHT, ESC, SPACE, ENTER = 1, 106, 1, 57, 28
READ = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_char_p, C.POINTER(C.c_uint8), C.c_uint32)

def tick_ns(n): return (n * TICK_NS10 + 9) // 10  # when tick n is due (rounded up to a whole ns)

_lib = None
def lib():
    global _lib
    if _lib is None:
        subprocess.run(['make', '-s', '-C', str(ROOT), 'host'], check=True)
        c = C.CDLL(str(OUT / 'libgmos.so'))
        c.gp_gif.argtypes = [C.c_char_p, C.c_uint32, C.c_void_p, C.c_void_p]
        c.gp_frame_hash.restype = C.c_uint32
        c.gs_start.argtypes = [C.c_void_p, C.c_void_p, C.c_char_p, C.c_char_p, C.c_uint64]
        c.gs_event.argtypes = [C.c_void_p, C.c_uint16, C.c_uint16, C.c_int32]
        c.gs_step.argtypes = [C.c_void_p, C.c_uint64]
        c.gs_wake.argtypes = [C.c_void_p, C.c_uint64]; c.gs_wake.restype = C.c_uint64
        c.gs_draw.argtypes = [C.c_void_p, C.c_void_p, C.c_uint32]
        c.gmos_session_new.restype = C.c_void_p
        for f in ('gmos_session_phase', 'gmos_session_card', 'gmos_session_ticks', 'gmos_session_showing_card'): getattr(c, f).argtypes = [C.c_void_p]
        c.gp_graphic.restype = C.c_char_p
        _lib = c
    return _lib

class Session:
    """A gs_session over tests/games/<dir>, logging into self.log."""
    def __init__(self, folder=DICE, gam=b'DICE.GAM', now=0):
        self.c = lib(); self.s = self.c.gmos_session_new(); self.log = []; self.folder = str(folder).encode()
        self.say = C.CFUNCTYPE(None, C.c_char_p)(lambda line: self.log.append(line.decode()))
        self.c.gmos_session_say(C.c_void_p(self.s), self.say)
        self.read = C.cast(self.c.gmos_host_read, C.c_void_p)
        self.result = self.c.gs_start(self.s, self.read, self.folder, gam, now)
    def step(self, t): return self.c.gs_step(self.s, t)
    def key(self, code, value): return self.c.gs_event(self.s, EV_KEY, code, value)
    def phase(self): return self.c.gmos_session_phase(self.s)
    def ticks(self): return self.c.gmos_session_ticks(self.s)
    def card(self): return self.c.gmos_session_showing_card(self.s)
    def frame(self):
        buf = (C.c_uint32 * (W * H))(); self.c.gs_draw(self.s, buf, W); return list(buf)
    def to_play(self):
        """Through the title (12 s) and the level's intro (3 s): play starts at 15 s."""
        self.step(12_000_000_000); self.step(15_000_000_000); return 15_000_000_000

def gmplay(keys, folder=DICE, gam='DICE.GAM'):
    out = subprocess.run([str(OUT / 'gmplay'), str(folder), gam, str(keys)], check=True, capture_output=True, text=True).stdout
    return out.splitlines()

class SameCoreAsThePocketPlayer(unittest.TestCase):
    def test_states_tick_for_tick(self):
        # 1200 ticks (66 s) of walking and jumping: position, sequence, scroll,
        # score, hit points, lives, counters and the view's hash after each tick.
        lib()
        want = (DATA / 'dice-pocket.txt').read_text().splitlines()
        got = gmplay(DATA / 'dice.keys')
        self.assertEqual(len(got), 1200)
        for a, b in zip(want, got): self.assertEqual(a, b)
    def test_gif_cards_match_the_packed_ones(self):
        lines = (DATA / 'dice-cards.txt').read_text().split('\n')
        names = {'i0': ['ONE.GIF'], 'w': ['WONPIC.GIF', 'WON.GIF'], 'l': ['LOSTPIC.GIF', 'LOST.GIF']}
        order = [n for k in ('i0', 'w', 'l') for n in names[k]]
        crcs = [int(l.split()[1]) for l in lines if l]
        self.assertEqual(len(crcs), len(order))
        for name, crc in zip(order, crcs):
            d = (DICE / name).read_bytes(); pal = (C.c_uint8 * 768)(); px = (C.c_uint8 * 64000)()
            self.assertEqual(lib().gp_gif(d, len(d), pal, px), 0, name)
            self.assertEqual(zlib.crc32(bytes(pal) + bytes(px)), crc, name)
    def test_not_a_320x200_gif(self):
        pal = (C.c_uint8 * 768)(); px = (C.c_uint8 * 64000)()
        self.assertEqual(lib().gp_gif(b'GIF89a' + struct.pack('<HH', 10, 10) + bytes(8), 18, pal, px), -1)
        self.assertEqual(lib().gp_gif(b'not a gif at all', 16, pal, px), -1)

class Cards(unittest.TestCase):
    @classmethod
    def setUpClass(cls): OUT.mkdir(parents=True, exist_ok=True)
    def test_title_then_intro_then_play(self):
        s = Session(); self.assertEqual(s.result, 0)
        self.assertEqual(s.phase(), GS_CARDS); self.assertTrue(s.card())
        self.assertIn('GMOS card skipped (only GIF so far): starter.fli', s.log)   # TITLE.LST: the animation, then title.gif for 12 s
        self.assertIn('GMOS card title.gif, 12 s', s.log)
        png(OUT / 'play-title.png', s.frame())
        self.assertEqual(s.step(11_999_999_999), 0)                                 # still the title
        self.assertEqual(s.step(12_000_000_000), GS_DRAW); self.assertIn('GMOS card one.gif, 3 s', s.log)
        png(OUT / 'play-intro.png', s.frame())
        self.assertEqual(s.step(15_000_000_000), GS_DRAW); self.assertEqual(s.phase(), GS_PLAY)
        self.assertIn('GMOS level 1', s.log); self.assertFalse(s.card())
    def test_a_key_moves_on(self):
        s = Session(); s.key(ENTER, 1); self.assertEqual(s.step(1_000_000), GS_DRAW)
        self.assertIn('GMOS card one.gif, 3 s', s.log)
        s.key(ENTER, 0); self.assertEqual(s.step(2_000_000), 0)                     # a release is not a press
        s.key(SPACE, 1); s.step(3_000_000); self.assertEqual(s.phase(), GS_PLAY)
    def test_card_layout(self):
        s = Session(); px = s.frame()
        self.assertTrue(all(p == 0 for p in px[:40 * W]))                         # black above and below the 640 x 400 card
        self.assertTrue(all(p == 0 for p in px[440 * W:]))
        self.assertGreater(len(set(px[40 * W:440 * W])), 4)
        for y in range(40, 440, 37):                                                # every card pixel is 2 x 2
            for x in range(0, 640, 2): self.assertEqual(px[y * W + x], px[y * W + x + 1])
            self.assertEqual(px[y * W:(y + 1) * W], px[(y + (1 if y % 2 == 0 else -1)) * W:(y + (2 if y % 2 == 0 else 0)) * W])

class Play(unittest.TestCase):
    @classmethod
    def setUpClass(cls): OUT.mkdir(parents=True, exist_ok=True)
    def test_ticks_at_18_2_hz(self):
        s = Session(); t0 = s.to_play()
        self.assertEqual(s.step(t0 + tick_ns(1) - 1), 0); self.assertEqual(s.ticks(), 0)
        self.assertEqual(s.step(t0 + tick_ns(1)), GS_DRAW); self.assertEqual(s.ticks(), 1)
        self.assertEqual(lib().gs_wake(s.s, t0 + tick_ns(1)), t0 + tick_ns(2))
        s.step(t0 + 1_000_000_000); self.assertEqual(s.ticks(), 5)                  # 17 ticks behind: at most 4 at once ...
        s.step(t0 + 1_000_000_001); self.assertEqual(s.ticks(), 5)                  # ... and the lost time is dropped
        s.step(t0 + 1_000_000_000 + tick_ns(1)); self.assertEqual(s.ticks(), 6)
        s2 = Session(); t0 = s2.to_play()
        for k in range(1, 183): s2.step(t0 + tick_ns(k))
        self.assertEqual(s2.ticks(), 182)                                           # 182 ticks in 9.996 s
    def test_same_frames_as_gmplay(self):
        # The session's level, entered after its cards, plays tick for tick like
        # the host player (and so like the Pocket Player) for the same keys.
        keys = OUT / 'play-keys.txt'
        keys.write_text('K 3 77\nK 30 57\nK 33 185\nK 60 205\nT 90\n')
        want = [int(l.split()[-1]) for l in gmplay(keys)]
        s = Session(); t0 = s.to_play(); got = []
        for k in range(1, 91):
            if k == 3: s.key(RIGHT, 1)
            if k == 30: s.key(SPACE, 1)
            if k == 33: s.key(SPACE, 0)
            if k == 60: s.key(RIGHT, 0)
            s.step(t0 + tick_ns(k)); got.append(lib().gp_frame_hash())
            if k == 1: png(OUT / 'play-level1.png', s.frame())
        png(OUT / 'play-level1-walked.png', s.frame())
        self.assertEqual(got, want)
    def test_play_layout(self):
        s = Session(); t0 = s.to_play(); s.step(t0 + tick_ns(1)); px = s.frame()
        self.assertTrue(all(p == 0 for p in px[:44 * W]))                         # the 624 x 392 view, centered
        self.assertTrue(all(p == 0 for p in px[436 * W:]))
        self.assertTrue(all(px[y * W + x] == 0 for y in range(44, 436, 7) for x in list(range(8)) + list(range(632, 640))))
        self.assertGreater(len(set(px[44 * W:436 * W])), 4)
    def test_esc_leaves(self):
        s = Session(); t0 = s.to_play(); s.key(RIGHT, 1); s.step(t0 + tick_ns(1))
        self.assertEqual(s.key(ESC, 1), GS_EXIT); self.assertEqual(s.phase(), GS_DONE)
        self.assertEqual(s.step(t0 + tick_ns(2)), GS_EXIT); self.assertIn('GMOS back to the list (Esc)', s.log)
        s = Session(); self.assertEqual(s.key(ESC, 1), GS_EXIT)                     # on a card too
    def test_scan_codes(self):
        c = lib()
        for code, scan in ((103, 72), (108, 80), (105, 75), (106, 77), (57, 57), (28, 28), (96, 28), (30, 30), (102, 71), (109, 81), (1, 0), (200, 0)):
            self.assertEqual(c.gs_scan(code), scan, code)
    def test_graphic_slots(self):
        Session(); c = lib()
        self.assertEqual(c.gp_graphic(0, 0), b'title.lst'); self.assertEqual(c.gp_graphic(3, 0), b'one.lst')
        self.assertEqual((c.gp_graphic(1, 0), c.gp_graphic(2, 0)), (b'won.lst', b'lost.lst'))
    def test_missing_game(self):
        s = Session(gam=b'NONE.GAM'); self.assertEqual(s.result, -1); self.assertIn('GMOS cannot read NONE.GAM', s.log)

if __name__ == '__main__': unittest.main()
