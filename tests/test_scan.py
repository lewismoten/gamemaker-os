# SPDX-License-Identifier: MIT
"""Exercise the board scanner itself through the host profiling transport.

These are uncached scans, not tests of a persistent on-card cache or FAT order.
"""
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Scan(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.work = pathlib.Path(cls.temp.name)
        empty = cls.work/'empty'
        (empty/'GAMES').mkdir(parents=True)
        env = dict(os.environ)
        if 'asan' in os.environ.get('GMOS_HOST', ''):
            env['GMOS_SCAN_CFLAGS'] = '-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined'
        subprocess.run([sys.executable, str(ROOT/'tools/profile_scan.py'), str(ROOT),
                        str(empty), str(cls.work/'probe'), '1'], env=env,
                       check=True, stdout=subprocess.DEVNULL)
        cls.probe = cls.work/'probe/scan-probe'

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.card = pathlib.Path(tempfile.mkdtemp(dir=self.work))
        (self.card/'GAMES').mkdir()

    def dice(self, name='DICE'):
        target = self.card/'GAMES'/name
        shutil.copytree(ROOT/'tests/games/DICE', target)
        return target

    def scan(self):
        return json.loads(subprocess.check_output([str(self.probe), str(self.card)]))

    def index(self, text):
        (self.card/'GAMES/LIBRARY.JSL').write_text(text)

    def test_per_entry_logs_are_bounded_by_default(self):
        self.dice()
        for i in range(140):
            (self.card/'GAMES'/f'not-a-dos-folder-{i}').mkdir()
        for i in range(20): self.dice(f'DUP{i:05}')
        d = self.scan()
        self.assertEqual(d['games'], 1)
        self.assertEqual(d['crc_calls'], 21)  # duplicates still CRC checked
        self.assertLessEqual(d['logs'], 3)
        self.assertLessEqual(d['log_bytes'], 256)
        self.assertGreaterEqual(d['healths'], 161)

    def test_missing_corrupt_and_stale_metadata_still_discover(self):
        self.dice()
        expected = self.scan()['list']
        for text in ('{bad JSON\n', '', '{"schema":"gmp-library","title":"Absent","locator":"MISSING"}\n'):
            self.index(text)
            self.assertEqual(self.scan()['list'], expected)

    def test_index_metadata_is_fresh_and_later_records_win(self):
        self.dice()
        record = lambda t: json.dumps(dict(schema='gmp-library', title=t, locator='DICE', category=['sandbox']))+'\n'
        self.index(record('First')+record('Second'))
        d = self.scan()
        self.assertEqual((d['list'][0]['title'], d['list'][0]['cat']), ('Second', 1))
        self.index(record('Updated'))
        self.assertEqual(self.scan()['list'][0]['title'], 'Updated')

    def test_add_remove_and_new_game_inside_previously_empty_folder(self):
        (self.card/'GAMES/EMPTY').mkdir()
        self.assertEqual(self.scan()['games'], 0)
        shutil.copyfile(ROOT/'tests/games/DICE/DICE.GAM', self.card/'GAMES/EMPTY/DICE.GAM')
        self.assertEqual(self.scan()['list'][0]['dir'], 'EMPTY')
        (self.card/'GAMES/EMPTY/DICE.GAM').unlink()
        self.assertEqual(self.scan()['games'], 0)
        self.dice()
        self.assertEqual(self.scan()['games'], 1)
        shutil.rmtree(self.card/'GAMES/DICE')
        self.assertEqual(self.scan()['games'], 0)

    def test_duplicates_reappear_after_winner_removed(self):
        self.dice('FIRST'); self.dice('SECOND')
        d = self.scan()
        self.assertEqual((d['games'], d['crc_calls']), (1, 2))
        winner = d['list'][0]['dir']  # native encounter order, not alphabetical
        shutil.rmtree(self.card/'GAMES'/winner)
        d = self.scan()
        self.assertEqual(d['games'], 1)
        self.assertNotEqual(d['list'][0]['dir'], winner)

    def test_pack_priority_and_launch_mapping_preserved(self):
        self.dice()
        (self.card/'POCKET').mkdir()
        for folder in ('POCKET', 'GAMES'):
            shutil.copyfile(ROOT/'tests/data/dice.gmp', self.card/folder/'DICE.GMP')
        d = self.scan()
        self.assertEqual((d['games'], d['crc_calls']), (1, 3))
        self.assertEqual((d['list'][0]['packed'], d['list'][0]['pack_folder'], d['list'][0]['pack_base']), (1, 2, 'DICE'))
        self.assertEqual(len(d['list'][0]['volume_sizes']), 1)
        (self.card/'POCKET/DICE.GMP').unlink()
        self.assertEqual(self.scan()['list'][0]['pack_folder'], 1)
        (self.card/'GAMES/DICE.GMP').unlink()
        self.assertEqual(self.scan()['list'][0]['packed'], 0)

    def test_invalid_pack_and_zip_do_not_hide_loose_game(self):
        self.dice()
        (self.card/'GAMES/BAD.GMP').write_bytes(b'not a pack')
        shutil.copyfile(ROOT/'tests/data/dice.gmp', self.card/'GAMES/DICE.ZIP')
        self.assertEqual(self.scan()['list'][0]['packed'], 0)
        self.assertEqual(self.scan()['games'], 1)

    def test_same_size_payload_edit_is_not_trusted_as_unchanged(self):
        folder = self.dice()
        old = self.scan()['list'][0]['crc']
        p = folder/'DICE.GAM'
        data = bytearray(p.read_bytes()); data[-1] ^= 1; p.write_bytes(data)
        self.assertNotEqual(self.scan()['list'][0]['crc'], old)
