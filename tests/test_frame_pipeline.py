# SPDX-License-Identifier: MIT
"""Native oracle covers padded buffers, palette/geometry changes and tick debt."""
import os
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class FramePipeline(unittest.TestCase):
    def test_pixels_and_animation_with_sparse_draws(self):
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        result = subprocess.run([str(host / 'frame_bench'),
                                 str(ROOT / 'tests/games/DICE'), 'DICE.GAM', '64'],
                                check=True, capture_output=True, text=True)
        self.assertIn('dense vs sparse/catch-up identical', result.stdout)
        self.assertIn('unchanged rows PASS', result.stdout)
