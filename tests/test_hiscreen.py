"""The Top Ten flow: name entry, list, the card's copy and the game's own .HIS as a seed.
Also renders the screens to PNGs in build/host/top-ten (see docs/saves.md)."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))

class HiScreenTests(unittest.TestCase):
    def test_flow_and_screens(self):
        from gmos_page import png, OUT
        shots = OUT / 'top-ten'; shots.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='hiscreen-') as d:
            exe = str(Path(d) / 'hiscreen')
            flags = ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined',
                     '-fno-sanitize-recover=all'] if os.getenv('AUDIO_SANITIZE') else ['-O2']
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc',
                            'src/hiscreen.c', 'src/hisload.c', 'src/hiscore.c', 'src/savefile.c', 'src/text.c',
                            'src/font8_data.c', 'tests/host/hiscreen.c', '-o', exe], cwd=ROOT, check=True)
            r = subprocess.run([exe, str(shots)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn('ALL PASS', r.stdout)
        for ppm in sorted(shots.glob('*.ppm')):
            head = ppm.read_bytes().split(b'\n', 3)
            w, h = map(int, head[1].split()); data = head[3]
            png(ppm.with_suffix('.png'), [data[i] << 16 | data[i + 1] << 8 | data[i + 2] for i in range(0, len(data), 3)], w, h)
            ppm.unlink()
        self.assertTrue((shots / 'top-ten-after-entry.png').exists())

if __name__ == '__main__':
    unittest.main(verbosity=2)
