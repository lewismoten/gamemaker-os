# SPDX-License-Identifier: MIT
"""The Archive folder: its signature, the background watcher and the background CRC pass, through the host scan probe."""
import io, json, os, pathlib, subprocess, sys, tempfile, unittest, zipfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
N = 6

def make_zip(gam):
    b = io.BytesIO()
    with zipfile.ZipFile(b, 'w', zipfile.ZIP_DEFLATED) as z: z.writestr('DICE.GAM', gam)
    return b.getvalue()

class ArchiveFolder(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(); cls.work = pathlib.Path(cls.temp.name)
        (cls.work/'empty/GAMES').mkdir(parents=True)
        subprocess.run([sys.executable, str(ROOT/'tools/profile_scan.py'), str(ROOT), str(cls.work/'empty'), str(cls.work/'probe'), '1'], check=True, stdout=subprocess.DEVNULL)
        cls.probe = cls.work/'probe/scan-probe'
        cls.gam = bytearray((ROOT/'tests/games/DICE/DICE.GAM').read_bytes())
    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()
    def setUp(self):
        self.card = pathlib.Path(tempfile.mkdtemp(dir=self.work)); (self.card/'GAMES').mkdir(); (self.card/'ARCHIVE').mkdir()
        for k in range(N): self.put(k)
    def put(self, k, tag=None):
        g = bytearray(self.gam); g[-1] = k if tag is None else tag
        (self.card/('ARCHIVE/OLD%d.ZIP' % k)).write_bytes(make_zip(bytes(g)))
    def run_probe(self, **extra):
        env = dict(os.environ, GMOS_SCAN_CACHE='1', GMOS_SCAN_ARCH='1', GMOS_SCAN_FAT='1', **extra) # FAT-like keys: size and whole-second date only
        return json.loads(subprocess.check_output([str(self.probe), str(self.card)], env=env))
    def test_a_matching_signature_reads_no_zip(self):
        cold = self.run_probe(); self.assertEqual((cold['arch_zip_reads'], cold['arch_warm']), (N, 0))
        warm = self.run_probe(); self.assertEqual((warm['arch_zip_reads'], warm['arch_warm'], warm['arch_left'], warm['games']), (0, 1, 0, N))
    def test_one_added_removed_or_changed_zip_is_exactly_that_work(self):
        self.run_probe()
        self.put(N)                                                    # added
        self.assertEqual(self.run_probe()['arch_zip_reads'], 1)
        (self.card/'ARCHIVE/OLD1.ZIP').unlink()                        # removed: nothing read, the count drops
        r = self.run_probe(); self.assertEqual((r['arch_zip_reads'], r['games']), (0, N))
        self.put(2, 77); f = self.card/'ARCHIVE/OLD2.ZIP'; os.utime(f, (f.stat().st_atime, f.stat().st_mtime + 5))  # changed (a new date)
        self.assertEqual(self.run_probe()['arch_zip_reads'], 1)
        self.assertEqual(self.run_probe()['arch_zip_reads'], 0)
    def test_the_watcher_finds_a_change_and_updates_only_the_difference(self):
        self.run_probe()
        r = self.run_probe(GMOS_SCAN_CHECK='1'); self.assertEqual((r['check_changed'], r['arch_zip_reads']), (0, 0))  # nothing changed: no relist, no read
        hook = 'cp "%s/ARCHIVE/OLD0.ZIP" "%s/ARCHIVE/NEW.ZIP"' % (self.card, self.card)
        r = self.run_probe(GMOS_SCAN_CHECK='1', GMOS_SCAN_HOOK=hook)
        self.assertEqual((r['check_changed'], r['relisted'], r['arch_zip_reads'], r['games']), (1, 1, 1, N))  # a copy of a known game: its key is new, so one read; the game is a duplicate
    def test_the_watcher_reads_one_buffer_per_pass(self):
        for k in range(N, N + 120): self.put(k)
        self.run_probe()
        r = self.run_probe(GMOS_SCAN_CHECK='1')
        self.assertGreater(r['check_passes'], 3); self.assertLess(r['check_passes'], 40)   # 120 names take several 2 KiB buffers, not one call
        self.assertEqual(r['check_changed'], 0)
    def test_removed_zip_is_dropped_by_the_watcher(self):
        self.run_probe()
        r = self.run_probe(GMOS_SCAN_CHECK='1', GMOS_SCAN_HOOK='rm "%s/ARCHIVE/OLD3.ZIP"' % self.card)
        self.assertEqual((r['check_changed'], r['arch_zip_reads'], r['games']), (1, 0, N - 1))
    def test_crc_pass_chunks_equal_a_one_shot_crc_and_resume_across_boots(self):
        big = self.card/'ARCHIVE/OLD0.ZIP'; g = bytearray(self.gam)
        b = io.BytesIO()
        with zipfile.ZipFile(b, 'w', zipfile.ZIP_STORED) as z: z.writestr('DICE.GAM', bytes(g)); z.writestr('PAD.BIN', os.urandom(100000))
        big.write_bytes(b.getvalue())
        self.run_probe()
        first = self.run_probe(GMOS_SCAN_CRC='2')                      # two chunks only: the big ZIP is partly done
        self.assertEqual(first['crc_done'], 2)
        self.assertLess(first['crc_max_chunk'], 500000)               # microseconds per chunk on the host: bounded
        full = self.run_probe(GMOS_SCAN_CRC='100000')
        self.assertEqual(full['arch_zip_reads'], 0)
        # a second boot verifies each ZIP against the stored CRC: no mismatch, no re-read
        again = self.run_probe(GMOS_SCAN_CRC='100000'); self.assertEqual((again['relisted'], again['arch_zip_reads']), (0, 0))
        # in place, same size and (forced) same dates: the CRC differs, so that one ZIP is read again
        st = big.stat(); data = bytearray(big.read_bytes()); data[-100] ^= 0xff; big.write_bytes(bytes(data)); os.utime(big, ns=(st.st_atime_ns, st.st_mtime_ns))
        changed = self.run_probe(GMOS_SCAN_CRC='100000')
        self.assertEqual(changed['relisted'], 2)
    def test_the_crc_pass_chunk_is_32_kib(self):
        src = (ROOT/'src/main.c').read_text(); self.assertIn('#define CRC_CHUNK 32768u', src)
if __name__ == '__main__': unittest.main()
