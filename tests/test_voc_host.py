"""Actual unlinked production VOC assertions; optional repository corpus oracle."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class VocHostTests(unittest.TestCase):
    def test_real_production_decoder_resampler_mixer(self):
        with tempfile.TemporaryDirectory(prefix='voc-host-') as d:
            exe = str(Path(d) / 'voc-host')
            flags = ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined',
                     '-fno-sanitize-recover=all'] if os.getenv('AUDIO_SANITIZE') else ['-O2']
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags,
                            '-Isrc', 'src/voc_host.c', 'src/pcspeaker.c',
                            'tests/host/voc_host.c', '-o', exe], cwd=ROOT, check=True)
            subprocess.run([exe], cwd=ROOT, check=True, timeout=30)

if __name__ == '__main__':
    unittest.main(verbosity=2)
