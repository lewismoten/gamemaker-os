# SPDX-License-Identifier: MIT
"""Menu UI sounds: cues per action, the off switch and bounded bursts (tests/host/uisound_check.c)."""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UiSounds(unittest.TestCase):
    def test_cues_off_switch_and_burst(self):
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        result = subprocess.run([str(host / 'uisound_check')], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(result.stdout.count('PASS'), 1, result.stdout)
