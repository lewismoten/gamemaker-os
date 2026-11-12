# SPDX-License-Identifier: MIT
"""The name-entry loop never lets the audio pump or the heartbeat go quiet (tests/host/entry_check.c)."""
import os
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Entry(unittest.TestCase):
    def test_pump_and_heartbeat_never_quiet(self):
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        p = subprocess.run([str(host / 'entry_check')], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn('PASS', p.stdout)
