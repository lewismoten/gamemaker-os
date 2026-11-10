# SPDX-License-Identifier: MIT
"""Library search/navigation and the production list codec over fake save I/O."""
import ctypes as C
import pathlib
import unittest
from gmos_page import Page, lib, png, GRID, DETAILS, OUT

SEARCH, FAVORITES, RECENT = 5, 6, 7

class List(C.Structure):
    _fields_ = [('count', C.c_uint), ('key', (C.c_char * 9) * 32)]

OPEN = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_char_p, C.POINTER(C.c_uint64))
WRITE = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint64, C.c_void_p, C.c_uint32, C.POINTER(C.c_uint32))
FD = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint64)
RENAME = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_char_p, C.c_char_p)
class Ops(C.Structure):
    _fields_ = [('ctx', C.c_void_p), ('open_write', OPEN), ('write', WRITE),
                ('sync', FD), ('close', FD), ('rename', RENAME), ('open_read', OPEN), ('read', WRITE)]

class Store:
    def __init__(self):
        self.files = {}; self.path = None; self.at = 0; self.fail_rename = False
        def opening(ctx, path, fd, write=False):
            self.path = path; self.at = 0; fd[0] = 1
            if write: self.files[path] = b''
            return 0 if path in self.files else -2
        def write(ctx, fd, buf, n, done):
            n = min(n, 3)  # exercise short writes
            self.files[self.path] += C.string_at(buf, n); done[0] = n; return 0
        def read(ctx, fd, buf, n, got):
            data = self.files[self.path][self.at:self.at + min(n, 5)]
            C.memmove(buf, data, len(data)); got[0] = len(data); self.at += len(data); return 0
        def rename(ctx, src, dst):
            if self.fail_rename: return -5
            self.files[dst] = self.files.pop(src); return 0
        self.ops = Ops(None, OPEN(lambda c,p,f: opening(c,p,f,True)), WRITE(write),
                       FD(lambda c,f: 0), FD(lambda c,f: 0), RENAME(rename),
                       OPEN(opening), WRITE(read))


def fixture():
    p = Page()
    for folder, title, company in [('DICE', 'Roll the Dice', 'RSD'), ('ALPHA', 'Alpha World', 'Dice Works'),
                                    ('BETA', 'Beta World', 'Acme')]:
        p.add(folder, title)
        p.index(f'{{"schema":"gmp-library","locator":"{folder}","title":"{title}","company":"{company}"}}')
    return p.finish()


def type_text(p, text):
    codes = dict(zip('qwertyuiopasdfghjklzxcvbnm', [16,17,18,19,20,21,22,23,24,25,30,31,32,33,34,35,36,37,38,44,45,46,47,48,49,50]))
    codes[' '] = 57
    for ch in text.lower(): p.key(codes[ch])


def search_tab(p):
    p.key('up'); p.key('right'); p.key('enter')
    assert p.tab == SEARCH


class Collections(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        c = lib()
        c.gm_search.argtypes = [C.c_void_p, C.c_char_p]
        c.gm_played.argtypes = [C.c_void_p, C.c_uint]
        for name in ('gm_list_toggle', 'gm_list_recent', 'gm_list_has'):
            getattr(c, name).argtypes = [C.POINTER(List), C.c_char_p]
        for name in ('gm_list_load', 'gm_list_save'):
            getattr(c, name).argtypes = [C.POINTER(List), C.POINTER(Ops), C.c_int]

    def size(self, p, tab): return lib().gm_tab_size(p.m, tab)

    def test_live_search_company_case_space_escape_and_details(self):
        p = fixture(); search_tab(p)
        type_text(p, 'DICE')
        self.assertEqual(self.size(p, SEARCH), 2)
        self.assertEqual(p.key('esc', 2), 0)
        self.assertEqual(self.size(p, SEARCH), 2)
        self.assertEqual(p.key(33), 1)  # F is text while searching, never favorite
        self.assertEqual(self.size(p, SEARCH), 0)
        p.key('backspace'); p.key('right'); chosen = p.selected
        p.key('enter'); self.assertEqual((p.view, p.selected), (DETAILS, chosen))
        p.key('esc'); self.assertEqual(p.view, GRID)
        p.key('esc'); self.assertEqual(self.size(p, SEARCH), 3)  # clear first
        type_text(p, 'alpha world'); self.assertEqual(self.size(p, SEARCH), 1)
        p.key('esc'); p.key('esc'); p.key('right'); p.key('enter')
        self.assertEqual(p.tab, FAVORITES)  # empty page reachable with remote keys

    def test_remote_search_keyboard_and_favorite_button(self):
        p = fixture(); search_tab(p)
        p.keys(*(['right'] * 3)); p.key('enter')  # D
        p.keys(*(['right'] * 5)); p.key('enter')  # I
        p.keys(*(['left'] * 6)); p.key('enter')   # C
        p.keys('right', 'right', 'enter')        # E
        self.assertEqual(self.size(p, SEARCH), 2)
        p.keys('down', 'down', 'down', 'down', 'right', 'right', 'enter')  # Done
        p.key('enter'); self.assertEqual(p.view, DETAILS)
        p.keys('down', 'down', 'enter')          # Favorite
        self.assertEqual(self.size(p, FAVORITES), 1)
        p.key('esc'); self.assertEqual(p.view, GRID)
        p.key('esc'); self.assertEqual(self.size(p, SEARCH), 3)
        p.key('esc'); p.key('right'); p.key('enter')
        self.assertEqual(p.tab, FAVORITES)

    def test_search_capacity_and_no_matches(self):
        p = Page()
        for i in range(256): p.add(f'G{i}', f'Title {i}')
        p.finish()
        self.assertEqual(self.size(p, SEARCH), 256)
        lib().gm_search(p.m, b'TITLE 255')
        self.assertEqual(self.size(p, SEARCH), 1)
        lib().gm_search(p.m, b'absent')
        self.assertEqual(self.size(p, SEARCH), 0)
        self.assertEqual(self.size(p, 4), 256)

    def test_favorites_counts_details_removal_and_recent_order(self):
        p = fixture(); chosen = p.selected
        self.assertEqual(p.key(33), 10)
        self.assertEqual(self.size(p, FAVORITES), 1)
        self.assertEqual(p.key(33, 2), 0)
        p.key('up'); p.key('right'); p.key('right'); p.key('enter')
        self.assertEqual((p.tab, p.selected), (FAVORITES, chosen))
        p.key('enter'); p.key('down'); p.key('down'); p.key('enter')
        self.assertEqual(self.size(p, FAVORITES), 0)
        self.assertEqual(p.selected, chosen)  # details retains identity after unstar
        p.key('esc'); self.assertEqual(p.selected, -1)
        for i in (0, 2, 1, 2): lib().gm_played(p.m, i)
        self.assertEqual(self.size(p, RECENT), 3)
        self.assertEqual([lib().gmos_tab_game(p.m, RECENT, k) for k in range(3)], [2,1,0])
        self.assertEqual([self.size(p, t) for t in range(5)], [0,0,0,0,3])

    def test_capacity_dedup_and_newest_first(self):
        c = lib(); pins = List(); recent = List()
        for i in range(32): self.assertEqual(c.gm_list_toggle(C.byref(pins), f'G{i}'.encode()), 1)
        self.assertEqual(c.gm_list_toggle(C.byref(pins), b'OVER'), -1)
        self.assertEqual(c.gm_list_toggle(C.byref(pins), b'G0'), 0)
        for i in range(25): c.gm_list_recent(C.byref(recent), f'G{i}'.encode())
        self.assertEqual(recent.count, 20)
        self.assertEqual(recent.key[0].value, b'G24')
        self.assertEqual(c.gm_list_has(C.byref(recent), b'G4'), -1)
        c.gm_list_recent(C.byref(recent), b'G10')
        self.assertEqual((recent.count, recent.key[0].value), (20, b'G10'))
        self.assertEqual(c.gm_list_recent(C.byref(recent), b'G10'), 0)

    def test_text_persistence_missing_corrupt_and_atomic_replace(self):
        c = lib(); store = Store(); pins = List(); restored = List()
        for recent in (0, 1):
            self.assertEqual(c.gm_list_load(C.byref(restored), C.byref(store.ops), recent), 0)
            c.gm_list_toggle(C.byref(pins), b'GAME1234')
            self.assertEqual(c.gm_list_save(C.byref(pins), C.byref(store.ops), recent), 0)
            self.assertEqual(c.gm_list_load(C.byref(restored), C.byref(store.ops), recent), 0)
            self.assertEqual(restored.count, pins.count)
        path = b'/sd/OS/SAVES/GMOS/FAVS.TXT'
        self.assertEqual(store.files[path], b'GMOS 1\nGAME1234\n')
        store.fail_rename = True
        self.assertEqual(c.gm_list_save(C.byref(pins), C.byref(store.ops), 0), -5)
        self.assertEqual(store.files[path], b'GMOS 1\nGAME1234\n')
        store.files[path] = b'GMOS 1\nGOOD\n../BAD\nGOOD\nTOOLONG123\nLAST'  # truncated last record ignored
        c.gm_list_load(C.byref(restored), C.byref(store.ops), 0)
        self.assertEqual((restored.count, restored.key[0].value), (1, b'GOOD'))
        for bad in (b'', b'wrong\n', b'GMOS 2\nGOOD\n', b'x' * 321):
            store.files[path] = bad
            self.assertNotEqual(c.gm_list_load(C.byref(restored), C.byref(store.ops), 0), 0)
            self.assertEqual(restored.count, 0)

    def test_screenshots(self):
        out = OUT
        p = fixture(); p.paths[0] = pathlib.Path(__file__).resolve().parent / 'games/DICE'
        p.key('end'); p.key(33); lib().gm_played(p.m, 2)
        search_tab(p)
        png(out / 'library-keyboard.png', p.render())
        type_text(p, 'dice')
        png(out / 'library-search.png', p.render())
        p.key('esc'); p.key('esc'); p.key('right'); p.key('enter')
        png(out / 'library-favorites.png', p.render())
        p.key('enter')
        png(out / 'library-favorite-details.png', p.render())
        p.key('esc')
        p.key('up'); p.key('right'); p.key('enter')
        png(out / 'library-recent.png', p.render())
