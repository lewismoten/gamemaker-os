"""Audio: the integer OPL2 synth and CMF sequencer against the web engine's reference render, the PC-speaker
sequence, and the pump feeding a fake ring (tests/host/audio.c). The reference tests/data/opl-ref.raw is
tools/web_opl_render.ts's output for audio_cmf.make_cmf() (mono 16-bit, 48 kHz, 1 s, gain 0.8)."""
import array, math, os, subprocess, sys, tempfile, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ['src/opl.c', 'src/opl_tables.c', 'src/cmf.c', 'src/audio.c', 'src/pcspeaker.c']
sys.path.insert(0, str(ROOT / 'tests'))
from audio_cmf import make_cmf


def build(out, main, extra=()):
    flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all'] if os.getenv('AUDIO_SANITIZE') else ['-O2']
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', *SRC, main, *extra, '-o', out], cwd=ROOT, check=True)


def rms(v):
    return math.sqrt(sum(x * x for x in v) / max(1, len(v)))


class AudioTests(unittest.TestCase):
    def test_host_checks(self):
        with tempfile.TemporaryDirectory(prefix='audio-') as d:
            exe = str(Path(d) / 'audio')
            build(exe, 'tests/host/audio.c')
            r = subprocess.run([exe], cwd=ROOT, check=False, capture_output=True, text=True, timeout=60)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_song_matches_the_web_engine(self):
        self.assertEqual((ROOT / 'tests/data/syn.cmf').read_bytes(), make_cmf())
        with tempfile.TemporaryDirectory(prefix='audio-') as d:
            exe, wav = str(Path(d) / 'render'), str(Path(d) / 'c.wav')
            build(exe, 'tests/host/audio_render.c')
            subprocess.run([exe, str(ROOT / 'tests/data/syn.cmf'), '1', wav], check=True, timeout=60)
            ref, got = array.array('h'), array.array('h')
            ref.frombytes((ROOT / 'tests/data/opl-ref.raw').read_bytes())
            got.frombytes(Path(wav).read_bytes()[44:])
            self.assertEqual(len(ref), len(got))
            diff = [a - b for a, b in zip(ref, got)]
            self.assertLess(rms(diff), 0.05 * rms(ref))  # measured 1.9%
            for i in range(0, len(ref) - 4800, 4800):    # the 100 ms envelope follows within 5% of the loudest
                self.assertLess(abs(rms(ref[i:i + 4800]) - rms(got[i:i + 4800])), 0.05 * max(abs(x) for x in ref), i)
            # the first note starts on the same tick: the first sound within 1 ms
            first = lambda v: next(i for i, x in enumerate(v) if abs(x) > 30)
            self.assertLessEqual(abs(first(ref) - first(got)), 48)


if __name__ == '__main__':
    unittest.main(verbosity=2)
