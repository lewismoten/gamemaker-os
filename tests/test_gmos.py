# SPDX-License-Identifier: MIT
"""Host tests for the pure parts of GMOS.ELF: the .GAM title reader, the library
index, the games page (tabs, grid, details) and its keys, the title screens on
the cards, and the 960x540 layout, rendered to PNG files in build/host/ for a
person to look at. GMOS_GAMES may name a folder of game folders (each with a
.GAM) to title real games too."""
import ctypes as C
import os, pathlib, struct, subprocess, sys, unittest, zlib
from gmos_page import (Page, lib, png, crc, q, rgb, gam_of, W, H, TW, TH, PAGE, NONE, MOVED, CHOSEN, QUIT, RESYNC, SCORES,
                       STARTER, SANDBOX, BIZARRE, CD, OTHER, ARCHIVE, GRID, DETAILS)
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / os.environ.get('GMOS_HOST', 'build/host')  # make test-asan: build/asan
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
    class Rec(C.Structure): _fields_ = [('title', C.c_char * 64), ('company', C.c_char * 32), ('blurb', C.c_char * 640),  # struct gm_record
                                        ('category', (C.c_char * 32) * 2), ('dos', C.c_char * 9), ('locator', C.c_char * 32),
                                        ('crc', C.c_uint32), ('has_crc', C.c_int), ('year', C.c_int32), ('course', C.c_int32)]
    def test_index_fills_titles_and_tabs(self):
        p = Page(); p.add_folder(DICE); p.add('ALPHA', 'ALPHA GAME'); p.add('BETA', 'BETA', gam='BETA.GAM'); p.add('LOOSE', 'LOOSE GAME')
        self.assertEqual(p.index(INDEX), 3)                    # by CRC-32 (DICE), folder (ALPHA), DOS name (BETA); bad lines are skipped
        p.finish()
        sizes = [c().gm_tab_size(p.m, t) for t in range(5)]
        self.assertEqual(sizes, [1, 1, 1, 0, 1])               # Starters, Sandbox, Bizarre, CD, Other
        self.assertEqual(p.tab, SANDBOX)                       # the web page opens on Sandbox
        px = p.render(); png(OUT / 'games-index.png', px)
    def test_record_parser(self):
        Rec = Library.Rec
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
    def test_background_music_requires_owner_opt_in(self):
        p = many(2)
        c().gmos_menu_music.argtypes = [C.c_void_p]
        self.assertEqual(c().gmos_menu_music(p.m), 0)
        self.assertEqual(p.key(50, 0), NONE)  # M release
        self.assertEqual(p.key(50, 2), NONE)  # M repeat
        self.assertEqual(c().gmos_menu_music(p.m), 0)
        self.assertEqual(p.key(50), 6)
        self.assertEqual(c().gmos_menu_music(p.m), 1)
        p.key('enter')  # the setting also works in details
        self.assertEqual(p.key(50), 6)
        self.assertEqual(c().gmos_menu_music(p.m), 0)
        self.assertEqual(c().gmos_menu_music(many(1).m), 0)

    def test_grid_moves_and_pages(self):
        p = many(14)
        self.assertEqual((p.tab, p.pos, p.view), (SANDBOX, 0, GRID))
        self.assertEqual(p.key('up'), NONE); self.assertEqual(p.key('left'), NONE)           # the first card stays put
        self.assertEqual(p.key('right'), MOVED); self.assertEqual(p.pos, 1)
        self.assertEqual(p.key('down'), MOVED); self.assertEqual(p.pos, 5)                    # four across
        self.assertEqual(p.key('down'), MOVED); self.assertEqual(p.pos, 9)                    # onto the next page
        self.assertEqual(p.key('pgdn'), MOVED); self.assertEqual(p.pos, 13)                   # eight on, held at the last
        self.assertEqual(p.key('down'), NONE); self.assertEqual(p.key('right'), NONE)
        self.assertEqual(p.key('pgup'), MOVED); self.assertEqual(p.pos, 5)
        self.assertEqual(p.key('home'), MOVED); self.assertEqual(p.pos, 0)
        self.assertEqual(p.key('end'), MOVED); self.assertEqual(p.pos, 13)
        p.key('home'); p.key('down'); p.key('down')                                           # 0, 4, 8
        self.assertEqual(p.key('down'), MOVED); self.assertEqual(p.pos, 12)                   # 12
        self.assertEqual(p.key('down'), NONE)                                                 # 12 is on the last row
        p.key('pgup'); p.key('pgup'); p.key('right'); p.key('right')                          # 2
        self.assertEqual(p.key('right', 2), MOVED); self.assertEqual(p.key('right', 0), NONE) # repeats move, releases do not
    def test_short_row_below(self):
        p = many(5); p.key('right'); p.key('right')                                           # 2: the row below has only 4
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
        self.assertEqual(rgb(px[10 * W + 300]), q(0x000080))                                 # the navy header
        x, y, w, h = card_box(0)
        self.assertEqual([x, y, w, h], [16, 112, 220, 180]); self.assertEqual(card_box(7)[:2], [16 + 3 * 236, 112 + 192])  # four across, two down
        self.assertEqual(rgb(px[(y - 1) * W + x + 40]), q(0x6d8bff))                          # the selected card's blue ring
        thumb = [px[(y + 1 + j) * W + x + 1 + i] for j in range(20, 120) for i in range(20, 200)]
        self.assertGreater(len(set(thumb)), 20)                                               # a title screen, not a stand-in
    def test_scrolled_page(self):
        p = many(14); p.keys('pgdn', 'down', 'right'); p.message('Chose Game 13')
        px = p.render(); png(OUT / 'games-grid-page2.png', px)
        self.assertEqual(p.pos, 13)
        x, y, w, h = card_box(13 % PAGE)
        self.assertEqual(rgb(px[(y - 1) * W + x + 40]), q(0x6d8bff))
    def test_details(self):
        p = Page(); p.add_folder(DICE); p.index(INDEX); p.finish(); p.key('enter')
        px = p.render(); png(OUT / 'games-details.png', px)
        self.assertEqual(p.view, DETAILS)
        gif = (DICE / 'TITLE.GIF').read_bytes(); want = (C.c_uint8 * 64000)(); wpal = (C.c_uint8 * 768)()
        c().gp_gif(gif, len(gif), wpal, want)
        vga = lambda i: sum(((wpal[want[i] * 3 + k] & 63) << 2 | (wpal[want[i] * 3 + k] & 63) >> 4) << (16 - 8 * k) for k in range(3))
        for dy, dx in (0, 0), (1, 1):                                                        # each pixel doubled, at (36, 68)
            self.assertEqual([rgb(px[(68 + 2 * y + dy) * W + 36 + 2 * x + dx]) for y in (0, 99, 199) for x in (0, 160, 319)],
                             [q(vga(y * 320 + x)) for y in (0, 99, 199) for x in (0, 160, 319)])
        p.key('right'); png(OUT / 'games-details-back.png', p.render())
    def test_high_scores_key(self):
        p = Page(); p.add_folder(DICE); p.index(INDEX); p.finish()
        self.assertEqual(p.key('h'), SCORES)                  # the grid: the selected game's Top Ten
        self.assertEqual(p.key('h', 0), NONE)                 # a release does nothing
        p.key('enter'); self.assertEqual(p.view, DETAILS)
        self.assertEqual(p.key('h'), SCORES); self.assertEqual(p.view, DETAILS)
        px = p.render(); png(OUT / 'games-details-hint.png', px)
        p.key('esc'); png(OUT / 'games-grid-hint.png', p.render())
        self.assertEqual(Page().finish().key('h'), NONE)      # no games: nothing to show
    def test_empty(self):
        px = page().render(); png(OUT / 'games-list-empty.png', px)
        self.assertTrue(any(rgb(px[y * W + x]) == q(0xeef0f6) for y in range(230, 256) for x in range(100, 860)))

class ArchiveTab(unittest.TestCase):
    """/sd/ARCHIVE (365 old user-made games on the board's card) is its own tab with its own room: it never crowds the
    main tabs (before, every game shared 256 slots and fell into Other unless the index named it)."""
    def setUp(self):
        c().gm_add_archive.argtypes = [C.c_void_p, C.c_char_p, C.c_char_p, C.c_char_p, C.c_uint32]
        c().gm_add_archive.restype = C.c_bool
    def build(self, n=365, shuffle=False, after_finish=False):
        p = Page(); lines = []
        for i in range(40):
            p.add(f'M{i:02d}', f'Main {i:02d}', crc=1000 + i); lines.append(f'{{"schema":"gmp-library","title":"Main {i:02d}","category":["sandbox"],"locator":"M{i:02d}"}}')
        p.index('\n'.join(lines))
        order = list(range(n))
        if shuffle: order = [(i * 7919) % n for i in order]
        if not after_finish:
            for i in order: self.assertTrue(p.add_archive(f'A{i:03d}', f'Old {i:03d}', crc=5000 + i))
        p.finish()
        if after_finish:
            for i in order: self.assertTrue(p.add_archive(f'A{i:03d}', f'Old {i:03d}', crc=5000 + i))
        return p
    def dirs(self, p, tab=ARCHIVE):
        c().gm_tab_game.restype = C.c_void_p; c().gm_tab_game.argtypes = [C.c_void_p, C.c_uint, C.c_uint]
        c().gm_tab_size.argtypes = [C.c_void_p, C.c_uint]
        return [C.string_at(c().gm_tab_game(p.m, tab, i), 9).split(b'\0')[0] for i in range(c().gm_tab_size(p.m, tab))]
    def test_512_archive_entries_stay_compact(self):
        # The loaded image has 8 MiB; an Archive entry is a short record (no company or blurb), not a full game slot.
        # Main tabs: 256 full slots (one struct gm_game and one 672-byte text each). Archive: at most 112 bytes a game
        # and 2 in order[]. (main.c's _Static_assert budgets the rest, the where_a table, at under 100 KiB in all.)
        main = 256 * (112 + 672) + 256 * 2
        extra = c().gm_menu_bytes() - main
        self.assertLess(extra, 512 * (112 + 2) + 4096)
        self.assertLess(extra, 100 * 1024)
    def test_archive_entries_have_no_index_words(self):
        p = self.build(n=3)
        self.assertEqual(p.index('{"schema":"gmp-library","title":"Words","company":"Acme","blurb":"Text","category":["sandbox"],"locator":"A001"}'), 1)
        p.finish()
        self.assertEqual(c().gm_tab_size(p.m, ARCHIVE), 3)   # still in the Archive tab, never moved
    def test_365_archive_games_leave_the_main_tabs_alone(self):
        p = self.build()
        c().gm_tab_size.argtypes = [C.c_void_p, C.c_uint]
        self.assertEqual(c().gm_tab_size(p.m, SANDBOX), 40); self.assertEqual(c().gm_tab_size(p.m, OTHER), 0)
        self.assertEqual(c().gm_tab_size(p.m, ARCHIVE), 365)
        self.assertEqual(p.tab, SANDBOX)
    def test_a_late_main_game_still_fits_after_the_archive_scan(self):
        p = Page()
        for i in range(256): self.assertTrue(p.add(f'M{i:03d}', f'Main {i}', crc=i + 1))
        self.assertTrue(p.add_archive('A', 'Old')) and self.assertFalse(p.add('X', 'late'))  # main room is 256 whatever the archive holds
        self.assertTrue(all(p.add_archive(f'B{i:03d}', f'Old {i}', crc=9000 + i) for i in range(500)))  # and the archive's room is its own
    def test_archive_room_is_bounded(self):
        p = Page()
        self.assertTrue(all(p.add_archive(f'A{i:03d}', f'Old {i}', crc=i + 1) for i in range(512)))
        self.assertFalse(p.add_archive('Z', 'one too many'))
    def test_index_never_moves_an_archive_game(self):
        p = Page(); p.add_archive('OLD', 'Old', crc=77)
        self.assertEqual(p.index('{"schema":"gmp-library","title":"New words","category":["sandbox"],"locator":"OLD"}'), 1)
        p.finish(); self.assertEqual(p.tab, ARCHIVE)
    def test_stable_order_however_found(self):
        a, b, d = self.build(), self.build(shuffle=True), self.build(shuffle=True, after_finish=True)
        want = [f'A{i:03d}'.encode() for i in range(365)]
        for x in (a, b, d): self.assertEqual(self.dirs(x), want)
    def test_incremental_insert_keeps_the_tab_sorted(self):
        p = self.build(shuffle=True, after_finish=True)
        got = self.dirs(p); self.assertEqual(got, sorted(got)); self.assertEqual(len(set(got)), 365)
        # the selected card keeps its game while more arrive in front of it
        q = Page(); q.add('M', 'Main', crc=1); q.finish(); q.add_archive('Z', 'Zed', crc=2); q.key('tab'); self.assertEqual(q.tab, ARCHIVE)
        c().gm_selected.restype = C.c_void_p; c().gm_selected.argtypes = [C.c_void_p]
        before = c().gm_selected(q.m)
        for i in range(30): q.add_archive(f'A{i:02d}', f'A {i}', crc=100 + i)
        self.assertEqual(c().gm_selected(q.m), before)
    def test_one_insert_touches_only_the_archive_tab(self):
        p = self.build(after_finish=True); sand = [p.c.gmos_tab_game(p.m, SANDBOX, i) for i in range(40)]
        p.add_archive('A000', 'Old 000', crc=1); self.assertEqual([p.c.gmos_tab_game(p.m, SANDBOX, i) for i in range(40)], sand)

class Memory(unittest.TestCase):
    """Regression tests from the AddressSanitizer pass (make test-asan): what the C side writes fits what it is given."""
    def test_record_mirror_is_the_c_size(self):
        # gm_record_parse zeroes and fills sizeof(struct gm_record) bytes of the caller's record
        self.assertEqual(C.sizeof(Library.Rec), c().gmos_record_bytes())
    def test_menu_holds_256_games_with_the_longest_text(self):
        long = 'W' + 'O' * 300; p = Page()
        self.assertTrue(all(p.add('ABCDEFGHIJ', long, gam='ABCDEFGHIJKLMNOP.GAM') for _ in range(256)))
        self.assertFalse(p.add('D', 'T'))
        line = ('{"schema":"gmp-library","title":"%s","company":"%s","blurb":"%s","category":["sandbox"],"locator":"ABCDEFGH"}'
                % (long, long, ('\u00e9 ' + 'O' * 900) * 4))
        self.assertEqual(p.index(line), 1); p.finish()
        g = C.cast(C.c_void_p(C.addressof(p.m)), C.POINTER(C.c_char * 9))
        self.assertEqual(g.contents.value, b'ABCDEFGH')                                        # dir[9]: 8 and a NUL
        p.render(load=False); p.key('enter'); self.assertEqual(p.view, DETAILS); p.render()
    def test_folder_names_the_board_lists(self):
        ok = lambda n: c().gm_dir_ok(n, len(n))
        self.assertTrue(ok(b'DICE')); self.assertTrue(ok(b'ABCDEFGH'))
        self.assertFalse(ok(b'')); self.assertFalse(ok(b'ABCDEFGHI')); self.assertFalse(ok(b'SHAKESPEARE-IN-SWEATPANTS')); self.assertFalse(ok(b'A.B'))
    def test_gif_decode_does_not_depend_on_the_one_before(self):
        # 4x1, min code size 2: clear, then code 7, never defined (gif.ts's fresh table reads it as 0), then the end
        bad = (b'GIF89a' + struct.pack('<HH', 4, 1) + bytes([0x81, 0, 0]) + bytes(12) + b'\x2c' + struct.pack('<4H', 0, 0, 4, 1)
               + bytes([0, 2, 2, 0x7c, 0x01, 0]) + b'\x3b')
        def draw(d):
            px, pal = (C.c_uint8 * 64000)(), (C.c_uint8 * 768)(); px[0] = px[1] = 99
            self.assertEqual(c().gp_gif(d, len(d), pal, px), 0); return bytes(px[:4])
        c().gp_gif.argtypes = [C.c_char_p, C.c_uint32, C.c_void_p, C.c_void_p]
        first = draw(bad); draw((DICE / 'TITLE.GIF').read_bytes())                              # a min-size-8 GIF fills codes 0-255
        self.assertEqual(draw(bad), first); self.assertEqual(first, bytes(4))

class Board(unittest.TestCase):
    def test_same_frames_as_the_board(self):
        """The frames the board logged (tests/data/board-list.txt), rendered from a card folder gmosCard.ts wrote: set GMOS_CARD."""
        card = os.environ.get('GMOS_CARD')
        if not card: self.skipTest('set GMOS_CARD to a folder written by the Sandbox Edition\'s web/scripts/gmosCard.ts')
        games = pathlib.Path(card) / 'GAMES'; rows = [l.split() for l in (ROOT / 'tests/data/board-list.txt').read_text().splitlines() if l and l[0] != '#']
        if any(r[0] == 'stale' for r in rows): self.skipTest('tests/data/board-list.txt was recorded from the 640x480 page: record it again')
        old = OUT / 'DICEOLD'; old.mkdir(parents=True, exist_ok=True)
        for f in DICE.iterdir(): (old / f.name).write_bytes(f.read_bytes())
        (old / 'DICE.GAM').write_bytes((ROOT / 'tests/data/dice-old.gam').read_bytes())
        for _, keys, want in (r for r in rows if r[0] == 'frame'):
            p = Page()
            for _, d, src in (r for r in rows if r[0] == 'game'): p.add_folder(old if src == '-' else games / src, d)
            p.index((games / 'LIBRARY.JSL').read_text()); p.finish()
            for k in keys.split(',') if keys != '-' else []: p.key(k)
            px = p.render()
            with self.subTest(keys): self.assertEqual(crc(px), int(want))
            if keys == '-': png(OUT / 'games-list-board.png', px)

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
