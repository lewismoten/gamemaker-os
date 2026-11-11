# SPDX-License-Identifier: MIT
"""The Mac helper's actual key sequence drives the production menu model."""
import ctypes as C
import importlib.util
import pathlib
import unittest
from gmos_page import Page, lib, SANDBOX, OTHER, DETAILS, GRID, MOVED, CHOSEN

spec = importlib.util.spec_from_file_location(
    'open_game', pathlib.Path(__file__).resolve().parents[1] / 'tools/open_game.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
FOUND, MISSING, PLAY = 7, 8, 9


def send(page, folder, play=False):
    result = None
    for key in helper.request_keys(folder, play):
        name, _, state = key.partition(':')
        code = 28 if name == 'enter' else int(name)
        if state:
            page.key(code, int(state == 'down'))
        else:
            result = page.key(code)
            page.key(code, 0)
    return result


def fixture():
    p = Page()
    # Deliberately reverse insertion order: find the position in sorted order.
    for i in reversed(range(24)):
        p.add(f'G{i:02}', f'Game {i:02}')
    p.add('START', 'Starter')
    p.index('{"schema":"gmp-library","title":"Starter","category":["sandbox"],"locator":"START"}')
    return p.finish()


class OpenGame(unittest.TestCase):
    def test_switch_tab_page_details_and_play_focus(self):
        p = fixture()
        self.assertEqual(p.tab, SANDBOX)
        self.assertEqual(send(p, 'g19'), FOUND)
        self.assertEqual((p.tab, p.pos, p.view, p.button), (OTHER, 19, DETAILS, 0))
        self.assertIn(p.selected, p.page_games())
        p.key('esc')
        self.assertEqual((p.view, p.pos), (GRID, 19))
        p.key('enter'); p.key('right')  # request while Back has focus
        self.assertEqual(send(p, 'START'), FOUND)
        self.assertEqual((p.tab, p.pos, p.button), (SANDBOX, 0, 0))
        self.assertEqual(p.key('enter'), CHOSEN)
        # The library remains usable after the caller returns from play.
        self.assertEqual(send(p, 'G00'), FOUND)
        self.assertEqual((p.tab, p.pos), (OTHER, 0))

    def test_missing_never_plays_or_changes_selection(self):
        p = fixture()
        send(p, 'G19')
        before = (p.tab, p.pos, p.view, p.button, p.selected)
        self.assertEqual(send(p, 'ABSENT', play=True), MISSING)
        self.assertEqual((p.tab, p.pos, p.view, p.button, p.selected), before)
        self.assertEqual(send(Page().finish(), 'DICE'), MISSING)
        self.assertEqual(send(p, 'G01', play=True), PLAY)
        self.assertEqual((p.tab, p.pos, p.button), (OTHER, 1, 0))

    def test_shortcut_letters_digits_punctuation_and_full_alias(self):
        p = Page()
        for name in ('QHM', 'A_0-Z123', 'DICE'):
            p.add(name, name)
        p.finish()
        for name in ('QHM', 'A_0-Z123', 'DICE'):
            self.assertEqual(send(p, name), FOUND)
        lib().gmos_menu_music.argtypes = [C.c_void_p]
        self.assertEqual(lib().gmos_menu_music(p.m), 1)
        self.assertEqual(helper.request_keys('dice'), ['53', '32', '23', '46', '18', 'enter'])
        for bad in ('', '../DICE', '123456789', 'A B', 'é'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                helper.request_keys(bad)

    def test_edit_cancel_reset_repeat_and_resync(self):
        p = Page(); p.add('DICE', 'Dice'); p.finish()
        p.key(53); p.key(32); p.key(23); p.key(46); p.key(18)
        self.assertEqual(p.key(18, 2), 0)  # repeats cannot append or submit
        p.key(14); p.key(18)
        self.assertEqual(p.key(28), FOUND)
        p.key(53); p.key(16)
        self.assertEqual(p.key(1), MOVED)  # Esc cancels without leaving details
        self.assertEqual(p.view, DETAILS)
        p.key(53); p.key(16)
        self.assertEqual(send(p, 'DICE'), FOUND)  # slash resets an unfinished request
        p.key(53); p.key(32)
        self.assertEqual(lib().gm_event(p.m, 0, 3, 0), 4)
        self.assertEqual(p.key(28), CHOSEN)  # resync canceled text entry
        p.key(53)
        for _ in range(20):
            p.key(32)
        self.assertEqual(p.key(28), MISSING)  # no truncated prefix match
