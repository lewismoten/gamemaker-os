# SPDX-License-Identifier: MIT
"""GMOS's card log (src/gmoslog.c): the last 200 lines, in order, one bounded staged write every 30 s (tests/host/gmoslog.c)."""
import subprocess, tempfile, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class GmosLog(unittest.TestCase):
    def test_log(self):
        with tempfile.TemporaryDirectory(prefix='gmoslog-') as d:
            exe = str(Path(d) / 'gmoslog')
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2', '-Isrc', 'src/hiscore.c', 'src/savefile.c',
                            'src/gmoslog.c', 'tests/host/gmoslog.c', '-o', exe], cwd=ROOT, check=True)
            r = subprocess.run([exe], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


if __name__ == '__main__':
    unittest.main()
