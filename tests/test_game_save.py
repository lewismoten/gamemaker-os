"""DOS GAME0 wire codec vs an independent synthetic fwrite-order oracle."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from game_save_fixture import dos_save

ROOT = Path(__file__).resolve().parents[1]


class GameSaveTests(unittest.TestCase):
    def test_dos_layout_and_rejection(self):
        with tempfile.TemporaryDirectory(prefix='game-save-') as d:
            exe = str(Path(d) / 'game-save')
            flags = ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined',
                     '-fno-sanitize-recover=all'] if os.getenv('AUDIO_SANITIZE') else ['-O2']
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags,
                            '-Isrc', 'src/hiscore.c', 'src/savefile.c',
                            'tests/host/game_save.c', '-o', exe], cwd=ROOT, check=True)
            # Cover every reachable block count and every recorder count. This
            # is a boundary matrix, not all 151*1001 Cartesian combinations.
            # Exhaustive short input/output lengths run once on the largest file.
            cases = {(m, (0, 1, 999, 1000)[m % 4]) for m in range(151)}
            cases.update((r % 151, r) for r in range(1001))
            cases.add((150, 1000))
            for blocks, records in sorted(cases):
                with self.subTest(blocks=blocks, records=records):
                    fixture = dos_save(blocks, records)
                    src, dst = Path(d) / 'input.sav', Path(d) / 'output.sav'
                    src.write_bytes(fixture)
                    run = subprocess.run([exe, str(src), str(dst), str(blocks), str(records)],
                                         capture_output=True, text=True, timeout=90)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    self.assertIn('GAME0 ALL PASS', run.stdout)
                    self.assertEqual(dst.read_bytes(), fixture)


if __name__ == '__main__':
    unittest.main(verbosity=2)
