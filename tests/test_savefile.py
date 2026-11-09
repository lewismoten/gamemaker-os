"""Saves and high scores: DOS file layout, atomic replace, a power cut after every step."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class SaveFileTests(unittest.TestCase):
    def test_host_suite(self):
        with tempfile.TemporaryDirectory(prefix='savefile-') as d:
            exe = str(Path(d) / 'savefile')
            flags = ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined',
                     '-fno-sanitize-recover=all'] if os.getenv('AUDIO_SANITIZE') else ['-O2']
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc',
                            'src/hiscore.c', 'src/savefile.c', 'tests/host/savefile.c', '-o', exe],
                           cwd=ROOT, check=True)
            r = subprocess.run([exe], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn('ALL PASS', r.stdout)

if __name__ == '__main__':
    unittest.main(verbosity=2)
