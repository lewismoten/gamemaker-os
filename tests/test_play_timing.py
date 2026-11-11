# SPDX-License-Identifier: MIT
"""Long-session native regression: no pointer conversion through ctypes."""
import os
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class PlayTiming(unittest.TestCase):
    def test_long_session_tick_boundaries(self):
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        p = subprocess.run([str(host / 'timing_check'), str(ROOT / 'tests/games/DICE')],
                           capture_output=True, text=True, check=True)
        self.assertIn('modular total IDs PASS', p.stdout)
        self.assertIn('long-duration conversion boundaries PASS', p.stdout)
        self.assertIn('resume reanchors level clock PASS', p.stdout)
