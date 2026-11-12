# SPDX-License-Identifier: MIT
"""Native playback power regression with real sessions and fake raw syscalls."""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Awake(unittest.TestCase):
    def test_playback_lifecycle(self):
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        result = subprocess.run([str(host / 'awake_check'), str(ROOT / 'tests/games/DICE')],
                                capture_output=True, text=True, check=True)
        self.assertEqual(result.stdout.count('PASS'), 5, result.stdout)
