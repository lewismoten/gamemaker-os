# SPDX-License-Identifier: MIT
"""The heartbeat call reports health first (tests/host/beat_check.c): a late one survives the kernel's 2 s rule."""
import os
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Beat(unittest.TestCase):
    def test_health_first(self):
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        p = subprocess.run([str(host / 'beat_check')], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn('PASS', p.stdout)
