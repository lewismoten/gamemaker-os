# SPDX-License-Identifier: MIT
"""Board log parsing without a serial device or board actions."""
import collections
import importlib.util
import pathlib
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    'board_play', pathlib.Path(__file__).resolve().parents[1] / 'tools/board_play.py')
assert SPEC is not None and SPEC.loader is not None
board_play = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(board_play)


class SelectedGame(Exception):
    pass


class BoardList(unittest.TestCase):
    def test_current_categories(self):
        for category in ('starter', 'sandbox', 'bizarre', 'cd', 'other', 'future'):
            with self.subTest(category=category):
                self.assertEqual(board_play.listed_games(
                    [f'GMOS game DICE [{category}]: Roll the Dice']), ['DICE'])

    def test_legacy_names_and_titles_are_not_split_or_normalized(self):
        self.assertEqual(board_play.listed_games([
            'GMOS game DICE: Roll the Dice',
            'GMOS game mixed-Case_1.GMP: A title: with [brackets]',
            'GMOS game EMPTY:',
            'GMOS game DICE: repeated entry',
        ]), ['DICE', 'mixed-Case_1.GMP', 'EMPTY', 'DICE'])

    def test_supported_log_wrappers_and_line_endings(self):
        for prefix in ('', 'LOG OS: ', '   22.644 ', '   22.644 LOG OS: '):
            for suffix in ('', '\n', '\r\n'):
                for entry in ('DICE: Roll the Dice', 'DICE [starter]: Roll the Dice'):
                    with self.subTest(prefix=prefix, suffix=suffix, entry=entry):
                        self.assertEqual(board_play.listed_games(
                            [prefix + 'GMOS game ' + entry + suffix]), ['DICE'])

    def test_unrelated_and_malformed_lines_are_ignored(self):
        lines = [
            '', 'OK run /OS/GMOS.ELF', 'GMOS 122 games in /sd/GAMES',
            'GMOS game ', 'GMOS game : title', 'GMOS game DICE',
            'GMOS game DICE [starter] title', 'GMOS game DICE [starter: title',
            'GMOS game DICE starter]: title', 'GMOS game DICE []: title',
            'GMOS game DICE [ ]: title', 'GMOS game DICE [[starter]]: title',
            'GMOS game DICE [starter] [other]: title',
            'GMOS game DICE [starter]: title\nGMOS game LOGIC: title',
            'GMOS game DICE EXTRA: title',
            'unrelated GMOS game DICE: title', 'LOG OTHER: GMOS game DICE: title',
            'GMOS gameplay DICE: title',
        ]

        for line in lines:
            with self.subTest(line=line):
                self.assertEqual(board_play.listed_games([line]), [])

    def test_mixed_listing_keeps_order_and_folder_names(self):
        self.assertEqual(board_play.listed_games([
            'LOG OS: GMOS game DICE [starter]: Roll the Dice',
            'LOG OS: GMOS game malformed',
            'LOG OS: GMOS game LOGIC: Ohm Sweet Ohm',
            'LOG OS: GMOS game SANDCAST [sandbox]: Sandcastle Knight',
        ]), ['DICE', 'LOGIC', 'SANDCAST'])

    def test_drive_selects_category_game_by_folder_not_title(self):
        # Exact listing text from scan-speed-1108/mac-logs/base1.log.
        pending = collections.deque(['OK run /OS/GMOS.ELF',
                        'LOG OS: GMOS game DICE [starter]: Roll the Dice',
                        'LOG OS: GMOS game LOGIC [sandbox]: Ohm Sweet Ohm'])
        sent = []

        def send(data):
            sent.append(data)
            if data == b'KEY 28 down\n':
                raise SelectedGame
            if data.startswith(b'KEY '):
                pending.append('OK key')

        def line(timeout):
            try:
                return pending.popleft()
            except IndexError:
                raise TimeoutError from None

        port = types.SimpleNamespace(send=send, line=line, close=lambda: None)
        usb = types.SimpleNamespace(Serial=lambda path: port, handshake=lambda port: 'OK')
        with tempfile.TemporaryDirectory() as tmp, patch.dict(sys.modules, usb_kernel=usb), \
                patch.object(sys, 'path', sys.path.copy()):
            with self.assertRaises(SelectedGame):
                board_play.drive(tmp, 'unused', pathlib.Path(tmp) / 'board.log',
                                 'LOGIC', None, False, False, False)
        self.assertEqual(sent, [b'RUN GMOS.ELF\n', b'KEY 108 down\n',
                                b'KEY 108 up\n', b'KEY 28 down\n'])


if __name__ == '__main__':
    unittest.main()
