# SPDX-License-Identifier: MIT
"""Only the six baseline remote keys drive production input handlers."""
import ctypes as C
import unittest
from gmos_page import Page, lib, png, OUT, GRID, DETAILS, MOVED, CHOSEN, QUIT, SCORES
from test_collections import fixture, SEARCH, FAVORITES, RECENT
from test_dosmenu import Launcher, MENU, SCORES as DOS_SCORES
from test_play import GS_CARDS, GS_PLAY, GS_DONE


class Remote:
    def __init__(self, page):
        self.p = page

    def keys(self, *keys):
        assert all(k in ('up', 'down', 'left', 'right', 'enter', 'esc') for k in keys)
        result = None
        for k in keys:
            result = self.p.key(k)
        return result

    def options(self, row):
        self.keys('esc', *(['down'] * row))
        return self.keys('enter')

    def letter(self, ch):
        # Navigate from a known top-left focus using only remote events.
        self.keys(*(['up'] * 4), *(['left'] * 8))
        i = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'.index(ch)
        self.keys(*(['down'] * (i // 9)), *(['right'] * (i % 9)), 'enter')

    def done(self):
        self.keys(*(['down'] * 4), *(['left'] * 5), 'right', 'right', 'right')
        return self.keys('enter')


class RemoteOnly(unittest.TestCase):
    def test_remote_overlays_render(self):
        p = fixture(); r = Remote(p)
        cards = p.render()
        r.keys('esc')
        options = p.render()
        self.assertNotEqual(cards, options)
        png(OUT / 'remote-options.png', options)
        r.keys(*(['down'] * 4), 'enter')
        r.letter('D')
        entry = p.render()
        self.assertNotEqual(options, entry)
        png(OUT / 'remote-open-name.png', entry)
        r.keys('esc')
        self.assertEqual(cards, p.render())

    def test_options_favorites_scores_setting_and_quit(self):
        p = fixture(); r = Remote(p)
        self.assertEqual(r.options(1), 10)  # F alternative
        self.assertEqual(lib().gm_tab_size(p.m, FAVORITES), 1)
        self.assertEqual(r.options(2), SCORES)  # H alternative
        self.assertEqual(r.options(3), 6)  # M alternative
        self.assertEqual(r.options(3), 6)
        r.keys('esc', 'down', 'esc')  # cancel without activating
        self.assertEqual(p.view, GRID)
        self.assertEqual(r.options(5), QUIT)  # Q alternative

    def test_open_name_edit_cancel_missing_and_details_play(self):
        p = fixture(); r = Remote(p)
        r.options(4)
        for ch in 'DICF': r.letter(ch)
        r.keys(*(['down'] * 4), *(['left'] * 5), 'right', 'enter')  # Delete
        r.letter('E')
        self.assertEqual(r.done(), 7)  # GM_OPEN_FOUND
        self.assertEqual((p.view, p.selected), (DETAILS, 0))
        self.assertEqual(r.keys('enter'), CHOSEN)
        r.keys('esc')
        r.options(4); r.letter('Z')
        self.assertEqual(r.done(), 8)  # miss preserves the selection
        self.assertEqual(p.selected, 0)
        r.options(4); r.letter('Q'); r.keys('esc')
        self.assertEqual(p.selected, 0)
        self.assertEqual(r.options(5), QUIT)

    def test_folder_alias_punctuation_and_empty_library_quit(self):
        p = Page(); p.add('A-_1', 'Alias'); p.finish(); r = Remote(p)
        r.options(4); r.letter('A')
        r.keys(*(['down'] * 4), 'right', 'right', 'right', 'right', 'enter', 'right', 'enter')
        r.letter('1')
        self.assertEqual(r.done(), 7)
        self.assertEqual(p.view, DETAILS)
        p = Page().finish(); r = Remote(p)
        self.assertEqual(r.options(2), MOVED)  # no scores without a game
        self.assertEqual(r.options(5), QUIT)

    def test_all_tabs_empty_collections_and_search_reentry(self):
        p = Page()
        for cat in ('starter', 'sandbox', 'bizarre', 'cd', 'other'):
            p.add(cat.upper(), cat.title())
            p.index('{"schema":"gmp-library","locator":"' + cat.upper() + '","title":"' + cat.title() + '","category":["' + cat + '"]}')
        p.finish(); r = Remote(p)
        r.keys('up')
        seen = {p.tab}
        for _ in range(8):
            r.keys('right'); seen.add(p.tab)
        self.assertEqual(seen, set(range(8)))
        while p.tab != SEARCH: r.keys('right')
        r.keys('enter', 'enter')  # A filters all five titles
        self.assertEqual(lib().gm_tab_size(p.m, SEARCH), 3)
        r.done(); r.keys('enter'); self.assertEqual(p.view, DETAILS)
        r.keys('esc', 'esc', 'esc', 'right', 'enter')
        self.assertEqual((p.tab, p.selected), (FAVORITES, -1))
        r.keys('up', 'right', 'enter')
        self.assertEqual((p.tab, p.selected), (RECENT, -1))
        # main.c records a launched game after the session begins.
        lib().gm_played.argtypes = [C.c_void_p, C.c_uint]
        lib().gm_played(p.m, 0)
        self.assertEqual(p.selected, 0)
        r.keys('enter'); self.assertEqual(p.view, DETAILS)
        r.keys('esc', 'up', 'up', 'enter')  # Options via Up from tabs
        self.assertEqual(p.view, GRID)

    def test_dos_pages_scores_pause_and_return(self):
        for row in (1, 2, 3):
            s = Launcher(); s.menu()
            for _ in range(row): s.key(108, 1)
            s.key(28, 1); s.advance()
            self.assertEqual(s.phase(), GS_CARDS)
            s.key(1, 1); self.assertEqual(s.phase(), MENU)
        s = Launcher(title=True); s.key(28, 1); s.menu()
        for _ in range(4): s.key(108, 1)
        s.key(28, 1); self.assertEqual(s.phase(), DOS_SCORES)
        s.c.gs_menu_return(s.s, s.t)
        s.key(108, 1); self.assertEqual(s.key(28, 1), 2)
        self.assertEqual(s.phase(), GS_DONE)
        s = Launcher(); s.menu(); s.key(28, 1)
        for _ in range(10):
            s.key(28, 1); s.advance()
            if s.phase() == GS_PLAY: break
        self.assertEqual(s.phase(), GS_PLAY)
        s.key(1, 1); self.assertEqual(s.phase(), 5)  # pause
        s.key(1, 1); self.assertEqual(s.phase(), GS_PLAY)
        s.key(1, 1)
        for _ in range(3): s.key(108, 1)
        s.key(28, 1); self.assertEqual(s.phase(), DOS_SCORES)
        s.c.gs_menu_return(s.s, s.t); s.menu()
        s.key(1, 1); self.assertEqual(s.phase(), GS_DONE)
