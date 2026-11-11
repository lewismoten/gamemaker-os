# SPDX-License-Identifier: MIT
"""Production input queue and launcher policy driven by fake USB events."""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Input(unittest.TestCase):
    def test_event_streams(self):
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        result = subprocess.run([str(host / 'input_check'), str(ROOT / 'tests/games/DICE')], capture_output=True,
                                text=True, check=True, timeout=10)
        self.assertEqual(result.stdout.count('PASS'), 4, result.stdout)
