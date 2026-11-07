# SPDX-License-Identifier: MIT
"""Host tests for the pages between scenes and older games: FLI animations,
BKD backdrops, text pages, PLAYGAME's palette fades and pre-3.00 .GAM files.
The reference hashes in tests/data/pictures-ref.txt come from Game-Maker:
Sandbox Edition's own engine code run on the same files (formats/fli.ts,
showGraphic's displayFile on its Screen, formats/game.ts parseGame and
serializeGame). Files that are not in tests/games are read from a Sandbox
Edition checkout with its games built, named by GMOS_SANDBOX; without it those
tests are skipped."""
import ctypes as C
import os, pathlib, shutil, unittest, zlib
from test_play import lib, Session, gmplay, png, OUT, DICE, DATA, GS_CARDS, GS_PLAY, ENTER, SPACE, retrace
SANDBOX = pathlib.Path(os.environ['GMOS_SANDBOX']) if os.environ.get('GMOS_SANDBOX') else None
REF = [l.split() for l in (DATA / 'pictures-ref.txt').read_text().splitlines()]
def ref(kind, label): return next(r for r in REF if r[0] == kind and r[1] == label)
def need_sandbox(test): return unittest.skipUnless(SANDBOX, 'set GMOS_SANDBOX to a Sandbox Edition checkout with its games built')(test)

def dice_with(name, files):
    """A copy of Roll the Dice in build/host/<name> with some files replaced: {NAME: bytes or path}."""
    d = OUT / name
    if d.exists(): shutil.rmtree(d)
    shutil.copytree(DICE, d)
    for k, v in files.items(): (d / k).write_bytes(v if isinstance(v, bytes) else pathlib.Path(v).read_bytes())
    return d

class Animations(unittest.TestCase):
    def frames(self, path, label):
        head = ref('fli', label); want = [int(r[3]) for r in REF if r[0] == 'f' and r[1] == label]
        d = pathlib.Path(path).read_bytes(); info = lib().gmos_fli_open(d, len(d))
        self.assertEqual((info >> 16, info & 0xffff), (int(head[3]), int(head[5])))
        px = (C.c_uint8 * 64000)(); pal = (C.c_uint8 * 768)(); got = []
        for _ in want: lib().gmos_fli_next(px, pal); got.append(zlib.crc32(bytes(pal) + bytes(px)))
        self.assertEqual(got, want)
        return d
    def test_starter_sting_frames(self):
        # The Starters' sting: every frame's palette and pixels as the engine decodes them.
        self.frames(DICE / 'STARTER.FLI', 'starter')
    @need_sandbox
    def test_sandbox_sting_frames(self):
        self.frames(SANDBOX / 'games/brief/disk/sandlogo.fli', 'sandlogo')
        self.frames(SANDBOX / 'games/tuneup/disk/sandlogo.fli', 'sandlogo-tuneup')
        self.frames(SANDBOX / 'games/pictures-and-cartoons/disk/bloom.fli', 'bloom')   # a cartoon after a level
    def test_played_on_the_session_clock(self):
        # FLI::View: frame k at k * 5 retraces, over the black screen; the title follows the last frame's wait.
        s = Session(); t = 0; draws = []
        while s.shown() == 0:
            t += 1_000_000
            if s.step(t): draws.append(t)
        ms = lambda ns: -(-ns // 1_000_000) * 1_000_000                          # the first 1 ms step at or after
        self.assertEqual(t, ms(retrace(190)))   # the title begins after frame 37's 5 retraces
        self.assertEqual(draws[:37], [ms(retrace(5 * k)) for k in range(1, 38)])  # frame 0 was at the start
        png(OUT / 'pictures-starter-sting.png', s.frame())
    def test_not_an_animation(self):
        self.assertEqual(lib().gmos_fli_open(b'\0' * 200, 200), -1)
        big = bytearray(200); big[4:6] = b'\x11\xaf'; big[8:12] = (640).to_bytes(2, 'little') + (480).to_bytes(2, 'little')
        self.assertEqual(lib().gmos_fli_open(bytes(big), 200), -1)   # larger than 320 x 200 is not played

class TextPages(unittest.TestCase):
    def pages(self, folder_name, lst, label, png_name):
        r = ref('txt', label); want = [int(x) for x in r[r.index('pages') + 1:r.index('after')]]; after = int(r[-1])
        s = Session(dice_with(folder_name, {'TITLE.LST': lst}))
        got = []; t = 0; seen = 0
        while len(got) < len(want) and t < 60_000_000_000:
            t += 10_000_000; s.step(t)
            on_text = any('.txt' in l for l in s.log) and s.stage() == 3   # a text page written, its pause running
            if on_text and s.shown() != seen:
                seen = s.shown(); got.append(zlib.crc32(s.px()))
                if len(got) == 1: png(OUT / png_name, s.frame())
                s.key(ENTER, 1); s.key(ENTER, 0)
        self.assertEqual(got, want)
        return s, after
    def test_won_pages(self):
        # WON.LST shown as a title: its picture, WON.GIF with no pause, then WON.TXT over it, two pages.
        s, after = self.pages('DICEWON', b'wonpic.gif 6\r\nwon.gif 0\r\nwon.txt\r\n', 'won-menu', 'pictures-won-text.png')
        self.assertIn('GMOS page won.txt, until a key', s.log)
    def test_more_pages(self):
        s, after = self.pages('DICECRED', b'credits.gif 0\r\ncredits.txt\r\n', 'credits-menu', 'pictures-credits-text.png')
    def test_missing_text(self):
        self.pages('DICEMISS', b'won.gif 0\r\nnone.txt\r\n', 'missing-menu', 'pictures-missing-text.png')
    @need_sandbox
    def test_a_level_intro_text(self):
        # Cutscenes: level 1's intro list ends with a text page over its picture.
        r = ref('txt', 'cutscenes-onew'); folder = SANDBOX / 'games/cutscenes/disk'
        s = Session(folder, b'cuts.gam'); t = 0
        while not (any('onew.txt' in l for l in s.log) and s.stage() == 3) and t < 60_000_000_000:
            t += 10_000_000; s.step(t)
            if s.stage() == 3 and s.c.gmos_session_keys(s.s) == 0 and not any('onew.txt' in l for l in s.log): s.key(ENTER, 1); s.key(ENTER, 0)
        self.assertEqual(zlib.crc32(s.px()), int(r[r.index('pages') + 1]))
        png(OUT / 'pictures-cutscenes-text.png', s.frame())
    def test_lines(self):
        c = lib(); c.gp_text_line.argtypes = [C.c_char_p, C.c_uint32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
        d = b'short\n' + b'x' * 40 + b'\nend'; at = C.c_uint32(0); start = C.c_uint32(0); out = []
        while (n := c.gp_text_line(d, len(d), C.byref(at), C.byref(start))) >= 0: out.append(d[start.value:start.value + n])
        self.assertEqual(out, [b'short', b'x' * 31, b'x' * 9, b'end'])   # fgets(buf, 32): 31 characters at a time

class Fades(unittest.TestCase):
    def test_fade_steps(self):
        # FadeTo: from + trunc((to - from) * step / 65), each channel.
        a = bytes((i * 7) % 64 for i in range(768))
        b = bytes((i * 13 + 5) % 64 for i in range(768)); out = (C.c_uint8 * 768)()
        for step in (0, 1, 32, 64, 65):
            lib().gs_fade_palette(a, b, step, 65, out)
            self.assertEqual(bytes(out), bytes(x + int((y - x) * step / 65) for x, y in zip(a, b)), step)
    def test_title_fades_in(self):
        s = Session(); s.step(retrace(190)); pal = (C.c_uint8 * 768)(); px = (C.c_uint8 * 64000)()
        d = (DICE / 'TITLE.GIF').read_bytes(); lib().gp_gif(d, len(d), pal, px); title = bytes(pal)
        black = bytes(768); out = (C.c_uint8 * 768)()
        self.assertEqual(s.pal(), black)                                           # SetAllPalTo(&Black), then the picture
        for k in (1, 20, 64):
            s.step(retrace(190) + retrace(k)); lib().gs_fade_palette(black, title, k, 65, out); self.assertEqual(s.pal(), bytes(out), k)
            if k == 20: png(OUT / 'pictures-title-fading.png', s.frame())
        s.step(retrace(190) + retrace(65)); self.assertEqual(s.pal(), title)
    def test_level_fades_out_and_in(self):
        # Esc aside, a level ends by fading to black; here the level's start: it fades in from ONE.GIF's palette.
        s = Session(); s.step(retrace(190)); t = retrace(190) + retrace(65) + 12_000_000_000; s.step(t)
        s.step(t + retrace(65) + 3_000_000_000); level_at = t + retrace(65) + 3_000_000_000
        self.assertEqual(s.phase(), GS_CARDS); self.assertFalse(s.card())
        s.step(level_at + retrace(30)); png(OUT / 'pictures-level-fading.png', s.frame())
        pal = (C.c_uint8 * 768)(); px = (C.c_uint8 * 64000)(); d = (DICE / 'ONE.GIF').read_bytes(); lib().gp_gif(d, len(d), pal, px)
        out = (C.c_uint8 * 768)(); lib().gs_fade_palette(bytes(pal), (DICE / 'DICE.PAL').read_bytes(), 30, 65, out)
        self.assertEqual(s.pal(), bytes(out))
        s.step(level_at + retrace(65)); self.assertEqual(s.phase(), GS_PLAY); self.assertEqual(s.pal(), (DICE / 'DICE.PAL').read_bytes())

class Backdrops(unittest.TestCase):
    @need_sandbox
    def test_backdrop_window(self):
        # A .BKD names a GIF and the offset of the 320 x 200 window into it (MAXIMUS's 320 x 3200 sheets).
        m = SANDBOX / 'games/maximus/disk'
        for name in ('w99.bkd', 'w15.bkd'):
            r = ref('bkd', name)
            s = Session(dice_with('DICEBKD', {'TITLE.LST': name.encode() + b' 1\r\n', name.upper(): m / name, r[4].upper(): m / r[4]}))
            s.step(retrace(65)); self.assertIn(f'GMOS page {name}, 1 s', s.log)
            self.assertEqual(zlib.crc32(s.pal() + s.px()), int(r[5]), name)
        png(OUT / 'pictures-backdrop.png', s.frame())

class OlderGames(unittest.TestCase):
    def converted(self, path):
        c = lib(); folder = OUT / 'oldgam'; folder.mkdir(parents=True, exist_ok=True)
        (folder / 'OLD.GAM').write_bytes(pathlib.Path(path).read_bytes())
        n = c.gp_open(C.cast(c.gmos_host_read, C.c_void_p), str(folder).encode(), b'OLD.GAM'); size = C.c_uint32()
        p = c.gp_gam_bytes(C.byref(size))
        return n, zlib.crc32(C.string_at(p, size.value))
    def test_dice_before_3_00(self):
        # Roll the Dice saved in the pre-3.00 layout converts to the bytes the Sandbox Edition makes of it ...
        n, crc = self.converted(DATA / 'dice-old.gam')
        self.assertEqual((n, crc), (1, int(ref('gam', 'dice-old')[2])))
        # ... and plays tick for tick as the 3.00 file does.
        folder = dice_with('DICEOLD', {'DICE.GAM': DATA / 'dice-old.gam'})
        want = (DATA / 'dice-pocket.txt').read_text().splitlines()
        self.assertEqual(gmplay(DATA / 'dice.keys', folder), want)
        s = Session(folder); self.assertEqual(s.result, 0); s.step(1); self.assertIn('GMOS level 1', s.log)   # no title or intro slots then
    @need_sandbox
    def test_cd_shareware(self):
        cd = SANDBOX / 'cd'
        for r in REF:
            if r[0] == 'gam' and r[1].startswith('sharware/'):
                n, crc = self.converted(cd / r[1])
                self.assertEqual(crc, int(r[2]), r[1]); self.assertGreater(n, 0, r[1])

if __name__ == '__main__': unittest.main()
