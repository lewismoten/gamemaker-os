"""Real isolated PC-speaker producer; no live SND/game dispatch changes."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PcSpeakerTests(unittest.TestCase):
    def test_real_isolated_producer(self):
        with tempfile.TemporaryDirectory(prefix='pcspeaker-') as d:
            exe = str(Path(d) / 'pcspeaker')
            flags = ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined',
                     '-fno-sanitize-recover=all'] if os.getenv('AUDIO_SANITIZE') else ['-O2']
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags,
                            '-Isrc', 'src/pcspeaker.c', 'tests/host/pcspeaker.c', '-o', exe],
                           cwd=ROOT, check=True)
            subprocess.run([exe], cwd=ROOT, check=True, timeout=30)

if __name__ == '__main__':
    unittest.main(verbosity=2)
