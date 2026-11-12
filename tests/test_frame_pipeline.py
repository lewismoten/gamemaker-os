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

    def test_displayed_scroll_follows_the_view_every_tick(self):
        # Right held from tick 1: the picture gs_draw makes moves 4..5 px on every tick (PLAYGAME's
        # 4 px plus the fine pan), not 16 px every fourth tick with stills between.
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        result = subprocess.run([str(host / 'scroll_check'), str(ROOT / 'tests/games/DICE'),
                                 'DICE.GAM', '40', '120', '4', '5'],
                                check=True, capture_output=True, text=True)
        self.assertIn('PASS', result.stdout)
