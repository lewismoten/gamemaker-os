# SPDX-License-Identifier: MIT
"""Run board sdpack.c, not the independent hostio GMP adapter."""
import ctypes as C
import pathlib
import tempfile
import unittest
from test_gmp import bundle, stored, dice_pack, DICE
from test_play import OUT

class PackInfo(C.Structure):
    _fields_ = [('folder', C.c_uint8), ('volumes', C.c_uint8), ('base', C.c_char * 9), ('size', C.c_uint32 * 16)]

class SDPack(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import os
        cls.c = C.CDLL(str(pathlib.Path(os.environ.get('GMOS_HOST', 'build/host')) / 'libsdpack.so'))
        c = cls.c
        c.test_sp_setup.argtypes = [C.c_char_p, C.c_uint, C.c_uint]
        c.test_sp_list.argtypes = [C.c_uint8, C.POINTER(PackInfo), C.c_uint]
        c.test_sp_open.argtypes = [C.POINTER(PackInfo)]
        c.test_sp_count.argtypes = [C.c_uint]; c.test_sp_count.restype = C.c_uint64
        c.test_sp_read.argtypes = [C.c_uint32, C.c_void_p, C.c_uint32]
        c.test_sp_volume.argtypes = [C.c_uint, C.c_uint32, C.c_void_p, C.c_uint32]
        for name in ('test_sp_dos', 'test_sp_entry'):
            getattr(c, name).argtypes = [C.c_char_p, C.c_void_p, C.c_uint32]

    def setUp(self):
        OUT.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=OUT); self.addCleanup(self.tmp.cleanup)
        self.root = pathlib.Path(self.tmp.name); (self.root / 'sd/GAMES').mkdir(parents=True)
        self.data = dice_pack()
        self.install(self.data)
        self.setup(3)

    def setup(self, minor, major=1):
        self.c.test_sp_setup(str(self.root).encode(), major, minor)

    def install(self, data, cuts=None):
        cuts = cuts or [0, len(data)]
        for v, (a, b) in enumerate(zip(cuts, cuts[1:])):
            name = 'DICE.GMP' if not v else f'DICE.G{v:02}'
            (self.root / 'sd/GAMES' / name).write_bytes(data[a:b])

    def listing(self):
        packs = (PackInfo * 8)(); n = self.c.test_sp_list(1, packs, 8)
        self.assertEqual(n, 1); return packs[0]

    def open(self):
        self.info = self.listing(); self.assertEqual(self.c.test_sp_open(C.byref(self.info)), 1)

    def read(self, off, n):
        out = C.create_string_buffer(max(n, 1)); self.assertEqual(self.c.test_sp_read(off, out, n), 0)
        self.assertEqual(out.raw[:n], self.data[off:off+n])

    def test_backward_uses_open_volume_without_replay(self):
        self.open()
        self.read(len(self.data) - 100, 50)
        opens, read = self.c.test_sp_count(0), self.c.test_sp_count(3)
        self.read(3000, 29)
        self.assertEqual(self.c.test_sp_count(0), opens, 'backward read reopened volume')
        self.assertEqual(self.c.test_sp_count(3) - read, 29, 'replayed discarded payload')
        self.assertGreater(self.c.test_sp_count(2), 0)

    def test_volume_bounds_do_not_mutate_open_cursor(self):
        self.open(); self.read(4000, 32)
        out = C.create_string_buffer(32)
        before = [self.c.test_sp_count(i) for i in range(6)]
        for v, off, n in [(0, len(self.data)+1, 0), (0, len(self.data)-1, 2), (1, 0, 1), (16, 0, 1)]:
            self.assertEqual(self.c.test_sp_volume(v, off, out, n), -1)
        self.assertEqual([self.c.test_sp_count(i) for i in range(6)], before)
        self.read(4032, 32)

    def test_forward_backward_repeated_eof_and_logical_bounds(self):
        self.open()
        for off, n in [(3000, 71), (10000, 37), (3000, 71), (3000, 71),
                       (len(self.data)-1, 1), (len(self.data), 0), (0, 8), (3001, 99)]:
            self.read(off, n)
        out = C.create_string_buffer(8)
        before = [self.c.test_sp_count(i) for i in range(6)]
        for off, n in [(len(self.data)+1, 0), (len(self.data), 1), (0xffffffff, 8)]:
            self.assertEqual(self.c.test_sp_read(off, out, n), -1)
        self.assertEqual([self.c.test_sp_count(i) for i in range(6)], before)
        self.assertEqual(self.c.test_sp_volume(0, len(self.data), out, 0), 0)
        self.read(5000, 8)

    def test_old_kernels_never_issue_seek(self):
        for major, minor in [(1, 0), (1, 1), (1, 2), (0, 3), (2, 3)]:
            with self.subTest(major=major, minor=minor):
                self.setup(minor, major); self.open()
                self.read(len(self.data)-100, 50); opens = self.c.test_sp_count(0)
                self.read(3000, 10)
                self.assertEqual(self.c.test_sp_count(0), opens+1)
                self.assertEqual(self.c.test_sp_count(2), 0)

    def test_enosys_disables_seek_and_retains_legacy_correctness(self):
        self.open(); self.read(10000, 40)
        before = self.c.test_sp_count(2); opens = self.c.test_sp_count(0)
        self.c.test_sp_error(38); self.read(3000, 40)
        self.assertEqual(self.c.test_sp_count(0), opens+1)
        self.read(9000, 40); self.read(4000, 40)
        self.assertEqual(self.c.test_sp_count(2), before+1)

    def test_forward_enosys_replays_safely(self):
        self.open(); self.c.test_sp_error(38)
        self.read(10000, 73); self.read(3000, 73)
        self.assertEqual(self.c.test_sp_count(2), 1)

    def test_seek_errors_propagate_without_fallback_or_cursor_change(self):
        for error in (5, 9, 22, 29):
            with self.subTest(error=error):
                self.setup(3); self.open(); self.read(10000, 40)
                before = [self.c.test_sp_count(i) for i in range(6)]
                self.c.test_sp_error(error); out = C.create_string_buffer(b'!'*40)
                self.assertEqual(self.c.test_sp_read(3000, out, 40), -1)
                self.assertEqual(out.raw[:40], b'!'*40)
                self.assertEqual(self.c.test_sp_count(0), before[0])
                self.assertEqual(self.c.test_sp_count(3), before[3])
                self.read(10040, 40)  # failed seek left both cursors unchanged
                self.assertEqual(self.c.test_sp_count(2), before[2]+1)
                self.read(3000, 40)
                self.assertEqual(self.c.test_sp_count(2), before[2]+2)

    def test_dice_fixture_and_three_volumes_match_files_and_listing(self):
        from test_gmp import DATA
        self.data = (DATA / 'dice.gmp').read_bytes()
        for cuts in ([0, len(self.data)], [0, 1000, 1001+len(self.data)//3, len(self.data)]):
            self.install(self.data, cuts)
            for minor in (2, 3):
                self.setup(minor); self.open()
                self.assertEqual(self.info.base, b'DICE')
                self.assertEqual(self.info.volumes, len(cuts)-1)
                self.assertEqual(list(self.info.size)[:self.info.volumes], [b-a for a,b in zip(cuts,cuts[1:])])
                for f in sorted(DICE.iterdir(), reverse=True):
                    out = C.create_string_buffer(2<<20)
                    n = self.c.test_sp_dos(f.name.encode(), out, len(out))
                    self.assertEqual(out.raw[:n], f.read_bytes(), f.name)
                for boundary in cuts[1:-1]: self.read(boundary-7, 19)
                self.read(0, len(self.data))

    def test_large_generated_pack_three_volumes(self):
        import random
        gam = stored((DICE / 'DICE.GAM').read_bytes())
        head = bundle({'dos/DICE.GAM': gam, 'zzpayload': b''})
        payload = random.Random(470).randbytes(2400000-len(head))
        self.data = bundle({'dos/DICE.GAM': gam, 'zzpayload': payload})
        self.assertEqual(len(self.data), 2400000)
        cuts = [0, 800000, 1600000, len(self.data)]
        self.install(self.data, cuts)
        for minor in (2, 3):
            self.setup(minor); self.open()
            for off, n in [(799990, 30), (1599990, 30), (1700000, 431), (1650000, 77),
                           (1650000, 77), (2399990, 10), (1000, 400), (800000, 1), (1600000, 1)]:
                self.read(off, n)
            for _ in range(32):
                off = random.Random(_).randrange(len(self.data)-1024); self.read(off, 1024)

    def test_short_reads_and_truncated_physical_volume(self):
        self.open(); self.c.test_sp_short(7); self.read(3000, 100)
        self.assertGreater(self.c.test_sp_count(4), 10)
        # Advertised size is unchanged but the physical file becomes short.
        self.info = self.listing()
        (self.root / 'sd/GAMES/DICE.GMP').write_bytes(self.data[:4000])
        self.assertEqual(self.c.test_sp_open(C.byref(self.info)), 1)
        out = C.create_string_buffer(100)
        self.assertEqual(self.c.test_sp_read(3900, out, 100), 0)
        self.assertEqual(self.c.test_sp_read(4000, out, 1), -1)
        self.assertEqual(self.c.test_sp_read(5000, out, 1), -1)
        self.read(3000, 8)

    def test_malformed_and_missing_volume(self):
        import struct
        for data in (b'', b'GMS2'+self.data[4:], self.data[:100],
                     b'GMS1'+struct.pack('<I', 5000), bundle({'not-a-game': b'x'})):
            self.install(data); self.setup(3)
            info = self.listing(); self.assertEqual(self.c.test_sp_open(C.byref(info)), 0)
        self.install(self.data, [0, 1000, len(self.data)//2, len(self.data)])
        (self.root / 'sd/GAMES/DICE.G01').unlink()
        self.setup(3); info = self.listing(); self.assertEqual(info.volumes, 1)
        self.assertEqual(self.c.test_sp_open(C.byref(info)), 0)

if __name__ == '__main__': unittest.main()
