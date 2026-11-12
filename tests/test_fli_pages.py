# SPDX-License-Identifier: MIT
"""A long animation page keeps the kernel's heartbeat (tests/host/fli_pages_check.c), on the game's own FLI files."""
import os
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
GAME = pathlib.Path(os.environ.get('GMOS_YEAR2000', '/Users/lewismoten/dev/GameMaker/games/year2000/disk'))


class FliPages(unittest.TestCase):
    @unittest.skipUnless((GAME / 'liftoff.fli').exists() or (GAME / 'LIFTOFF.FLI').exists(), 'the game folder is not here')
    def test_animation_pages_beat_the_heartbeat(self):
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        p = subprocess.run([str(host / 'fli_pages_check'), str(GAME)], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn('100 ms wakes PASS', p.stdout)
