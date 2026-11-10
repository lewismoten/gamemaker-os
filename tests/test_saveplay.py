"""Drive production save-menu input against a fake card, including transient read errors."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_play import ROOT, OUT, lib


class SavePlayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        lib()
        cls.tmp = tempfile.TemporaryDirectory(prefix='saveplay-')
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = str(Path(cls.tmp.name) / 'saveplay')
        subprocess.run([os.environ.get('HOSTCC', 'cc'), '-std=c11', '-O1',
                        '-Wall', '-Wextra', '-Werror', '-Wcast-qual', '-Wshadow',
                        '-Wmissing-prototypes', '-Wstrict-prototypes', '-Wformat=2',
                        '-Isrc', '-Itests/host', 'tests/host/saveplay.c',
                        str(OUT / 'libgmos.so'), '-o', cls.exe], cwd=ROOT, check=True)

    def probe(self, scenario):
        r = subprocess.run([self.exe, str(scenario)], cwd=ROOT, capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('ALL PASS', r.stdout)

    def test_remote_name_and_save(self): self.probe(7)

    def test_names_open_error_preserves_all_names(self): self.probe(0)
    def test_names_partial_read_error_preserves_all_names(self): self.probe(1)
    def test_missing_names_can_be_created(self): self.probe(2)
    def test_short_names_preserve_existing_records(self): self.probe(3)
    def test_oversized_names_preserve_first_ten_records(self): self.probe(4)
    def test_names_update_preserves_other_nine_slots(self): self.probe(5)
    def test_names_write_error_is_reported(self): self.probe(6)


if __name__ == '__main__':
    unittest.main(verbosity=2)
