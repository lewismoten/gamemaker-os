# SPDX-License-Identifier: MIT
"""Host tests for the pure parts of GMOS.ELF: the .GAM title reader, the library
index, the games page (tabs, grid, details) and its keys, the title screens on
the cards, and the 640x480 layout, rendered to PNG files in build/host/ for a
person to look at. GMOS_GAMES may name a folder of game folders (each with a
.GAM) to title real games too."""
import ctypes as C
import os, pathlib, struct, subprocess, sys, unittest, zlib
from gmos_page import (Page, lib, png, crc, gam_of, W, H, TW, TH, NONE, MOVED, CHOSEN, QUIT, RESYNC,
                       STARTER, SANDBOX, BIZARRE, CD, OTHER, GRID, DETAILS)
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/host'
DICE = ROOT / 'tests/games/DICE'
INDEX = (ROOT / 'tests/data/library.jsl').read_text()
def c(): return lib()

def gam3(desc, scenes=3):
    d = bytearray(b'GM' + struct.pack('<h', 300) + bytes([0, 8]) + b'RSD' * 84)[:256]
    d += struct.pack('<3H', scenes, 20, 20)
    scene = bytearray(283); scene[0:2] = struct.pack('<H', 1); scene[23:23 + len(desc)] = desc
    return bytes(d + scene * max(scenes, 1) + b'\x01\x00\x00')
def gam_old(desc):
    rec = bytearray(330); rec[6:6 + len(desc)] = desc; return bytes(rec * 20)
def title(data):
    out = C.create_string_buffer(21); n = c().gm_gam_title(data, len(data), out); return n, out.value.decode('ascii')

def page(*games, index=''):
    p = Page()
    for d, t in games: p.add(d, t)
    if index: p.index(index)
    return p.finish()
def many(n, cat='sandbox'):
    """n games in one category, titled so they sort as numbered."""
    p = Page(); lines = []
    for i in range(n):
        p.add(f'G{i:02d}', f'Game {i:02d}')
        lines.append(f'{{"schema":"gmp-library","title":"Game {i:02d}","category":["{cat}"],"locator":"G{i:02d}"}}')
    p.index('\n'.join(lines)); return p.finish()

class Titles(unittest.TestCase):
    def test_version_3(self):
        self.assertEqual(title(gam3(b'ROLL THE DICE')), (13, 'ROLL THE DICE'))
        self.assertEqual(title(gam3(b"Kit's Shop")), (10, "Kit's Shop"))
        self.assertEqual(title(gam3(b'  \xc4\xc4 BOXED \xc4\xc4  ')), (5, 'BOXED'))         # frame characters around it are trimmed
        self.assertEqual(title(gam3(b'Caf\x82 \x01'))[1], 'Cafe ?')
        self.assertEqual(title(gam3(b'A' * 20)), (20, 'A' * 20))                              # all 20 bytes, no NUL
        self.assertEqual(title(gam3(b'')), (0, ''))                                           # empty: the folder name is shown
    def test_inner_frame_is_a_space(self):
        self.assertEqual(title(gam3(b'Caf\x82 \xb3 Two'))[1], 'Cafe   Two')
    def test_old_format(self):
        self.assertEqual(title(gam_old(b'OLD GAME')), (8, 'OLD GAME'))
        self.assertEqual(title(gam_old(b'B' * 15 + b'tail')), (15, 'B' * 15))                  # 15 bytes in old files
    def test_not_a_game(self):
        for data in (b'', b'GM', gam3(b'X')[:304], gam3(b'X', scenes=0), b'x' * 329):
            with self.subTest(len(data)): self.assertEqual(title(data)[0], -1)
    def test_real_games(self):
        folder = os.environ.get('GMOS_GAMES')
        if not folder: self.skipTest('set GMOS_GAMES to a folder of game folders')
        for g in sorted(pathlib.Path(folder).iterdir()):
            for f in g.glob('*.[gG][aA][mM]'):
                n, t = title(f.read_bytes()[:512]); self.assertGreaterEqual(n, 0, f); print(f'{g.name}: {t}')

class Library(unittest.TestCase):
    def test_index_fills_titles_and_tabs(self):
        p = Page(); p.add_folder(DICE); p.add('ALPHA', 'ALPHA GAME'); p.add('BETA', 'BETA', gam='BETA.GAM'); p.add('LOOSE', 'LOOSE GAME')
        self.assertEqual(p.index(INDEX), 3)                    # by CRC-32 (DICE), folder (ALPHA), DOS name (BETA); bad lines are skipped
        p.finish()
        sizes = [c().gm_tab_size(p.m, t) for t in range(5)]
        self.assertEqual(sizes, [1, 1, 1, 0, 1])               # Starters, Sandbox, Bizarre, CD, Other
        self.assertEqual(p.tab, SANDBOX)                       # the web page opens on Sandbox
        px = p.render(); png(OUT / 'games-index.png', px)
    def test_record_parser(self):
        class Rec(C.Structure): _fields_ = [('title', C.c_char * 64), ('company', C.c_char * 32), ('blurb', C.c_char * 640),
                                            ('category', (C.c_char * 32) * 2), ('dos', C.c_char * 9), ('locator', C.c_char * 32),
                                            ('crc', C.c_uint32), ('has_crc', C.c_int), ('year', C.c_int32), ('course', C.c_int32)]
        c().gm_record_parse.argtypes = [C.c_char_p, C.c_uint32, C.POINTER(Rec)]
        def parse(line):
            r = Rec(); b = line.encode(); return c().gm_record_parse(b, len(b), C.byref(r)), r
        lines = INDEX.splitlines()
        ok, r = parse(lines[1])
        self.assertEqual((ok, r.title, r.dos, r.category[0].value, r.category[1].value, r.course, r.crc, r.has_crc, r.year),
                         (1, b'Roll the Dice', b'DICE', b'starter', b'500', 506, 0xA745C830, 1, 1994))
        self.assertTrue(r.blurb.startswith(b'One short level about chance.'))
        ok, r = parse(lines[2]); self.assertEqual((ok, r.title, r.locator), (1, b"Alpha 's Cafe - One", b'ALPHA'))  # \u escapes folded
        ok, r = parse(lines[3]); self.assertEqual((ok, r.title, r.has_crc), (1, b'Beta\'s "Song"...', 1))        # UTF-8 folded
        for bad in lines[0], lines[4], lines[5], lines[6], '', '{}', '{"schema":"gmp-library"}':
            with self.subTest(bad[:30]): self.assertEqual(parse(bad)[0], 0)
    def test_crc(self):
        data = (DICE / 'DICE.GAM').read_bytes()
        self.assertEqual(c().gm_crc32(0, data, len(data)), zlib.crc32(data))

class Keys(unittest.TestCase):
    def test_grid_moves_and_pages(self):
        p = many(14)
        self.assertEqual((p.tab, p.pos, p.view), (SANDBOX, 0, GRID))
        self.assertEqual(p.key('up'), NONE); self.assertEqual(p.key('left'), NONE)           # the first card stays put
        self.assertEqual(p.key('right'), MOVED); self.assertEqual(p.pos, 1)
        self.assertEqual(p.key('down'), MOVED); self.assertEqual(p.pos, 4)                    # three across
        self.assertEqual(p.key('down'), MOVED); self.assertEqual(p.pos, 7)                    # onto the next page
        self.assertEqual(p.key('pgdn'), MOVED); self.assertEqual(p.pos, 13)                   # six on, held at the last
        self.assertEqual(p.key('down'), NONE); self.assertEqual(p.key('right'), NONE)
        self.assertEqual(p.key('pgup'), MOVED); self.assertEqual(p.pos, 7)
        self.assertEqual(p.key('home'), MOVED); self.assertEqual(p.pos, 0)
        self.assertEqual(p.key('end'), MOVED); self.assertEqual(p.pos, 13)
        p.key('home'); p.key('down'); p.key('down'); p.key('down')                            # 0, 3, 6, 9
        self.assertEqual(p.key('down'), MOVED); self.assertEqual(p.pos, 12)                   # 12
        self.assertEqual(p.key('down'), NONE)                                                 # 12 is on the last row
        p.key('pgup'); p.key('pgup'); p.key('right'); p.key('right')                          # 2
        self.assertEqual(p.key('right', 2), MOVED); self.assertEqual(p.key('right', 0), NONE) # repeats move, releases do not
    def test_short_row_below(self):
        p = many(5); p.key('right'); p.key('right')                                           # 2: the row below has 3 and 4
        self.assertEqual(p.key('down'), MOVED); self.assertEqual(p.pos, 4)
    def test_tabs(self):
        p = Page(); p.add_folder(DICE); p.add('ALPHA', 'A'); p.add('BETA', 'B', gam='BETA.GAM'); p.add('LOOSE', 'L'); p.index(INDEX); p.finish()
        self.assertEqual(p.key('tab'), MOVED); self.assertEqual(p.tab, BIZARRE)
        self.assertEqual(p.key('tab'), MOVED); self.assertEqual(p.tab, OTHER)                 # CD is empty: left out
        self.assertEqual(p.key('tab'), MOVED); self.assertEqual(p.tab, STARTER)               # around
        p.key('lshift', 1); self.assertEqual(p.key('tab'), MOVED); self.assertEqual(p.tab, OTHER)  # Shift+Tab goes back
        p.key('lshift', 0); self.assertEqual(p.key('tab'), MOVED); self.assertEqual(p.tab, STARTER)
        self.assertEqual(p.key('rbracket'), MOVED); self.assertEqual(p.tab, SANDBOX)
        self.assertEqual(p.key('lbracket'), MOVED); self.assertEqual(p.tab, STARTER)
        self.assertEqual(p.key('tab', 0), NONE)
    def test_each_tab_keeps_its_card(self):
        p = Page()
        for i in range(4): p.add(f'S{i}', f'S{i}'); p.add(f'B{i}', f'B{i}')
        p.index('\n'.join(f'{{"schema":"gmp-library","title":"{k}{i}","category":["{cat}"],"locator":"{k}{i}"}}'
                          for i in range(4) for k, cat in (('S', 'sandbox'), ('B', 'bizarre')))); p.finish()
        p.keys('right', 'right'); p.key('tab'); self.assertEqual(p.pos, 0); p.key('right')
        p.key('tab'); self.assertEqual((p.tab, p.pos), (SANDBOX, 2))
    def test_details_and_play(self):
        p = many(3)
        p.key('right')
        self.assertEqual(p.key('enter'), MOVED); self.assertEqual((p.view, p.button, p.pos), (DETAILS, 0, 1))
        self.assertEqual(p.key('enter', 2), NONE)                                             # a held Enter does not play
        self.assertEqual(p.key('left'), NONE)
        self.assertEqual(p.key('right'), MOVED); self.assertEqual(p.button, 1)                # Back
        self.assertEqual(p.key('right'), NONE)
        self.assertEqual(p.key('tab'), MOVED); self.assertEqual(p.button, 0)                  # Tab goes around the buttons
        self.assertEqual(p.key('down'), NONE); self.assertEqual(p.pos, 1)                     # the grid's keys do nothing here
        self.assertEqual(p.key('enter'), CHOSEN)                                              # Play
        p.key('right'); self.assertEqual(p.key('enter'), MOVED); self.assertEqual(p.view, GRID)  # Back
        p.key('enter'); self.assertEqual(p.key('esc'), MOVED); self.assertEqual(p.view, GRID)    # Esc goes back too
        p.key('enter'); self.assertEqual(p.key('space'), CHOSEN)
    def test_quit_resync_and_empty(self):
        p = many(2)
        self.assertEqual(p.key('esc'), QUIT); self.assertEqual(p.key('q'), QUIT); self.assertEqual(p.key('space'), NONE)
        self.assertEqual(c().gm_event(p.m, 0, 3, 0), RESYNC); self.assertEqual(c().gm_event(p.m, 0, 0, 0), NONE)
        self.assertEqual(c().gm_event(p.m, 3, 0, 100), NONE)                                  # axes do nothing yet
        e = page()
        for k in ('enter', 'down', 'right', 'tab', 'pgdn'): self.assertEqual(e.key(k), NONE)
        self.assertEqual(e.view, GRID)
    def test_sorting(self):
        p = Page()
        for d, t in (('ZED', 'zebra'), ('APPLE', 'Apple'), ('EMPTY', ''), ('B2', 'banana'), ('B1', 'banana')): p.add(d, t)
        p.finish()
        order = [p.c.gmos_tab_game(p.m, OTHER, i) for i in range(5)]
        self.assertEqual(order, [1, 4, 3, 2, 0])                                              # title ignoring case, then folder; empty title: folder
    def test_full_list(self):
        p = Page()
        self.assertTrue(all(p.add('D', 'T') for _ in range(256))); self.assertFalse(p.add('D', 'T'))

def card_box(i):
    v = [C.c_uint32() for _ in range(4)]; c().gm_card_rect(i, *v); return [x.value for x in v]

class Layout(unittest.TestCase):
    @classmethod
    def setUpClass(cls): OUT.mkdir(parents=True, exist_ok=True)
    def test_title_screen_of_dice(self):
        # TITLE.LST shows STARTER.FLI, then TITLE.GIF until a key: the picture held longest, as the web page picks it
        pic = Page(); pic.add_folder(DICE); px, pal = pic.title_picture(0)
        want = (C.c_uint8 * 64000)(); wpal = (C.c_uint8 * 768)(); gif = (DICE / 'TITLE.GIF').read_bytes()
        c().gp_gif.argtypes = [C.c_char_p, C.c_uint32, C.c_void_p, C.c_void_p]
        self.assertEqual(c().gp_gif(gif, len(gif), wpal, want), 0)
        self.assertEqual(bytes(px), bytes(want)); self.assertEqual(bytes(pal), bytes(wpal))
    def test_grid(self):
        p = Page(); p.add_folder(DICE); p.add('ALPHA', 'A'); p.add('BETA', 'B', gam='BETA.GAM'); p.add('LOOSE', 'L'); p.index(INDEX); p.finish()
        p.keys('tab', 'tab', 'tab')                                                           # Bizarre, Other, Starters (Roll the Dice)
        self.assertEqual(p.tab, STARTER)
        px = p.render(); png(OUT / 'games-grid.png', px)
        self.assertEqual(px[10 * W + 300], 0x000080)                                          # the navy header
        x, y, w, h = card_box(0)
        self.assertEqual(px[(y - 1) * W + x + 40], 0x6d8bff)                                  # the selected card's blue ring
        thumb = [px[(y + 1 + j) * W + x + 1 + i] for j in range(20, 100) for i in range(20, 170)]
        self.assertGreater(len(set(thumb)), 20)                                               # a title screen, not a stand-in
    def test_scrolled_page(self):
        p = many(14); p.keys('pgdn', 'down', 'right'); p.message('Chose Game 10')
        px = p.render(); png(OUT / 'games-grid-page2.png', px)
        self.assertEqual(p.pos, 10)
        x, y, w, h = card_box(10 % 6)
        self.assertEqual(px[(y - 1) * W + x + 40], 0x6d8bff)
    def test_details(self):
        p = Page(); p.add_folder(DICE); p.index(INDEX); p.finish(); p.key('enter')
        px = p.render(); png(OUT / 'games-details.png', px)
        self.assertEqual(p.view, DETAILS)
        gif = (DICE / 'TITLE.GIF').read_bytes(); want = (C.c_uint8 * 64000)(); wpal = (C.c_uint8 * 768)()
        c().gp_gif(gif, len(gif), wpal, want)
        rgb = lambda i: sum(((wpal[want[i] * 3 + k] & 63) << 2 | (wpal[want[i] * 3 + k] & 63) >> 4) << (16 - 8 * k) for k in range(3))
        self.assertEqual([px[(64 + y) * W + 36 + x] for y in (0, 99, 199) for x in (0, 160, 319)],
                         [rgb(y * 320 + x) for y in (0, 99, 199) for x in (0, 160, 319)])
        p.key('right'); png(OUT / 'games-details-back.png', p.render())
    def test_empty(self):
        px = page().render(); png(OUT / 'games-list-empty.png', px)
        self.assertTrue(any(px[y * W + x] == 0xeef0f6 for y in range(195, 220) for x in range(100, 540)))

class Build(unittest.TestCase):
    def test_font_source_is_current(self):
        self.assertEqual(subprocess.run([sys.executable, str(ROOT / 'tools/make_font.py'), '--check']).returncode, 0)
    def test_arm_program(self):
        cc = os.environ.get('TOOLCHAIN', '/Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/arm-none-eabi-') + 'gcc'
        if not pathlib.Path(cc).exists(): self.skipTest('no Arm GNU toolchain')
        subprocess.run(['make', '-s', '-C', str(ROOT), 'all'], check=True)
        d = (ROOT / 'build/GMOS.ELF').read_bytes()
        self.assertEqual(d[:7], b'\x7fELF\x01\x01\x01'); self.assertEqual(struct.unpack_from('<HH', d, 16), (2, 40))   # ELF32 LE, ET_EXEC, ARM
        self.assertEqual(struct.unpack_from('<I', d, 24)[0], 0x08000000)

if __name__ == '__main__': unittest.main()
