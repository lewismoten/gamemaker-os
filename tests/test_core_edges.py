# SPDX-License-Identifier: MIT
"""Synthetic regressions for the pinned play core's map and score limits."""
import os
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class CoreEdges(unittest.TestCase):
    def test_map_entries_links_and_scores(self):
        if not os.environ.get('GMOS_HOST'):
            subprocess.run(['make', '-s', '-C', str(ROOT), 'host'], check=True)
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        subprocess.run([str(host / 'coreedgetest')], check=True)
