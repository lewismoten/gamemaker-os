# SPDX-License-Identifier: MIT
"""Native power settings use the production menu, codec and SDK wrappers."""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Settings(unittest.TestCase):
    def test_remote_persistence_and_timeouts(self):
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        result = subprocess.run([str(host / 'settings_check')],
                                capture_output=True, text=True, check=True)
        self.assertEqual(result.stdout.count('PASS'), 4, result.stdout)
