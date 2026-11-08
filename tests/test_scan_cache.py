# SPDX-License-Identifier: MIT
import ctypes as C
import pathlib, subprocess, tempfile, unittest, os, struct, zlib
ROOT = pathlib.Path(__file__).resolve().parents[1]
class CacheCodec(unittest.TestCase):
    def test_exact_metadata_lookup_and_corruption_rejection(self):
        self.assertTrue((ROOT/'src/scan_cache.c').exists(), 'persistent cache codec is missing')
        with tempfile.TemporaryDirectory() as tmp:
            so = pathlib.Path(tmp)/'cache.so'
            extra = ['-fsanitize=address,undefined', '-fno-sanitize-recover=undefined'] if 'asan' in os.environ.get('GMOS_HOST', '') else []
            subprocess.run(['cc', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror', *extra, '-I'+str(ROOT/'src'), str(ROOT/'src/scan_cache.c'), '-o', str(so)], check=True)
            c = C.CDLL(str(so))
            class Cache(C.Structure):
                _fields_ = [('old', C.c_void_p), ('out', C.c_void_p), ('oldbytes', C.c_uint32), ('used', C.c_uint32), ('capacity', C.c_uint32), ('valid', C.c_uint32), ('overflow', C.c_uint32)]
            c.gc_init.argtypes = [C.POINTER(Cache), C.c_void_p, C.c_uint32, C.c_void_p, C.c_uint32]
            c.gc_add.argtypes = [C.POINTER(Cache), C.c_char_p, C.c_void_p, C.c_uint32, C.c_void_p, C.c_uint32]
            c.gc_get.argtypes = [C.POINTER(Cache), C.c_char_p, C.c_void_p, C.c_uint32, C.POINTER(C.c_uint32)]
            c.gc_get.restype = C.c_void_p
            c.gc_finish.argtypes = [C.POINTER(Cache)]
            a, b, state = C.create_string_buffer(4096), C.create_string_buffer(4096), Cache()
            c.gc_init(C.byref(state), None, 0, a, len(a))
            self.assertEqual(c.gc_add(C.byref(state), b'/sd/GAMES/DICE/DICE.GAM', b'meta', 4, b'title/crc', 9), 1)
            size = c.gc_finish(C.byref(state)); self.assertGreater(size, 9)
            c.gc_init(C.byref(state), a, size, b, len(b)); self.assertEqual(state.valid, 1)
            n = C.c_uint32()
            p = c.gc_get(C.byref(state), b'/sd/GAMES/DICE/DICE.GAM', b'meta', 4, C.byref(n))
            self.assertEqual(C.string_at(p, n.value), b'title/crc')
            self.assertFalse(c.gc_get(C.byref(state), b'/sd/GAMES/DICE/DICE.GAM', b'edit', 4, C.byref(n)))
            valid = a.raw[:size]
            a[size-1] = bytes([a.raw[size-1] ^ 1])
            c.gc_init(C.byref(state), a, size, b, len(b)); self.assertEqual(state.valid, 0)
            self.assertFalse(c.gc_get(C.byref(state), b'/sd/GAMES/DICE/DICE.GAM', b'meta', 4, C.byref(n)))
            # Independently repaired CRCs remove checksum shielding from the
            # individual count and length guards.
            for offset, fmt, value in ((8, '<I', 1025), (8, '<I', 0), (16, '<H', 0),
                                       (16, '<H', 64), (18, '<H', 0), (18, '<H', 257), (20, '<I', 0xffffffff)):
                bad = bytearray(valid); struct.pack_into(fmt, bad, offset, value)
                struct.pack_into('<I', bad, 12, zlib.crc32(bad[:12] + bad[16:]))
                raw = C.create_string_buffer(bytes(bad), len(bad))
                c.gc_init(C.byref(state), raw, len(bad), b, len(b)); self.assertEqual(state.valid, 0)
            for cap in range(size + 1):
                out = C.create_string_buffer(cap or 1)
                c.gc_init(C.byref(state), None, 0, out, cap)
                added = c.gc_add(C.byref(state), b'/sd/GAMES/DICE/DICE.GAM', b'meta', 4, b'title/crc', 9)
                self.assertEqual(added, int(cap == size))
                self.assertEqual(c.gc_finish(C.byref(state)), size if cap == size else 0)
                if cap == size: self.assertEqual(out.raw, valid)
if __name__ == '__main__': unittest.main()
