"""The streaming VOC player (src/vocplay.c) and its mixing in au_render, on the host."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class VocPlayTests(unittest.TestCase):
    def test_streaming_limits_priority_mixing(self):
        with tempfile.TemporaryDirectory(prefix='vocplay-') as d:
            exe = str(Path(d) / 'vocplay_test')
            flags = ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined',
                     '-fno-sanitize-recover=all'] if os.getenv('AUDIO_SANITIZE') else ['-O2']
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc',
                            'src/vocplay.c', 'src/audio.c', 'src/opl.c', 'src/opl_tables.c', 'src/cmf.c', 'src/pcspeaker.c',
                            'tests/host/vocplay_test.c', '-o', exe], cwd=ROOT, check=True)
            r = subprocess.run([exe], cwd=ROOT, timeout=60, capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

if __name__ == '__main__':
    unittest.main(verbosity=2)
