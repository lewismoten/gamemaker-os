# SPDX-License-Identifier: MIT
"""ZIP reader: independent zipfile writer; same read callback as game sessions."""
import ctypes as C
import io
import os
import pathlib
import struct
import subprocess
import tempfile
import unittest
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
DICE = ROOT/'tests/games/DICE'
READ = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint32, C.c_void_p, C.c_uint32)


def archive(method=zipfile.ZIP_STORED, prefix='', stub=b''):
    b = io.BytesIO()
    with zipfile.ZipFile(b, 'w', method) as z:
        for p in DICE.iterdir():
            z.writestr(prefix+p.name, p.read_bytes())
    return stub+b.getvalue()


class Zip(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.so = pathlib.Path(cls.temp.name)/'zip.so'
        if (ROOT/'src/zip.c').exists():
            extra = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined'] if 'asan' in os.environ.get('GMOS_HOST', '') else []
            subprocess.run(['cc', '-shared', '-fPIC', '-std=c11', '-Wall', '-Wextra', '-Werror', *extra,
                            '-I'+str(ROOT/'src'), '-I'+str(ROOT/'third_party/puff'),
                            str(ROOT/'src/zip.c'), str(ROOT/'src/asset_scratch.c'), str(ROOT/'src/library.c'),
                            str(ROOT/'third_party/puff/puff.c'), '-o', str(cls.so)], check=True)
            cls.c = C.CDLL(str(cls.so))
            cls.c.gz_bytes.restype = C.c_uint32
            cls.c.gz_open.argtypes = [C.c_void_p, READ, C.c_void_p, C.c_uint32]
            cls.c.gz_gam.argtypes = [C.c_void_p]; cls.c.gz_gam.restype = C.c_char_p
            cls.c.gz_read.argtypes = [C.c_void_p, C.c_char_p, C.c_void_p, C.c_uint32]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def open(self, data):
        self.assertTrue((ROOT/'src/zip.c').exists(), 'in-place ZIP reader not implemented')
        self.data = data
        def read(_, off, out, n):
            if off > len(data) or n > len(data)-off: return -1
            C.memmove(out, data[off:off+n], n)
            return 0
        self.read = READ(read)
        self.z = C.create_string_buffer(self.c.gz_bytes())
        return self.c.gz_open(self.z, self.read, None, len(data))

    def readfile(self, name, cap=2<<20):
        b = C.create_string_buffer(cap)
        n = self.c.gz_read(self.z, name.encode(), b, cap)
        return None if n < 0 else b.raw[:n]

    def test_deflated_wrapper_and_plain_zip_sfx_read_in_place(self):
        for prefix, stub in [('', b''), ('Long Game Folder/', b''), ('Long Game Folder/', b'MZ'+bytes(4094))]:
            with self.subTest(prefix=prefix, stub=len(stub)):
                self.assertEqual(self.open(archive(zipfile.ZIP_DEFLATED, prefix, stub)), 0)
                for p in DICE.iterdir():
                    self.assertEqual(self.readfile(p.name), p.read_bytes(), p.name)

    def test_rejects_ambiguous_or_deep_game_roots(self):
        for names in [('one/two/DICE.GAM',), ('one/DICE.GAM', 'two/OTHER.GAM'), ('DICE.GAM', 'one/OTHER.GAM')]:
            b = io.BytesIO()
            with zipfile.ZipFile(b, 'w') as z:
                for name in names: z.writestr(name, (DICE/'DICE.GAM').read_bytes())
            with self.subTest(names=names): self.assertEqual(self.open(b.getvalue()), -1)

    def test_malformed_and_unsafe_archives_fail_without_extraction(self):
        good = archive(zipfile.ZIP_DEFLATED)
        for data in (b'', b'MZ-not-a-ZIP', good[:-1], good[:30], good+b'junk'):
            self.assertEqual(self.open(data), -1)
        for name in ('../DICE.GAM', '/DICE.GAM', 'C:/DICE.GAM', 'ok/../DICE.GAM', 'ok//DICE.GAM', 'ok/./DICE.GAM'):
            b=io.BytesIO()
            with zipfile.ZipFile(b,'w') as z: z.writestr(name,(DICE/'DICE.GAM').read_bytes())
            self.assertEqual(self.open(b.getvalue()), -1, name)
        for method in (zipfile.ZIP_BZIP2, zipfile.ZIP_LZMA):
            self.assertEqual(self.open(archive(method)), -1)
        bad=bytearray(archive()); at=bad.index(b'PK\x01\x02'); struct.pack_into('<H',bad,at+8,1)
        self.assertEqual(self.open(bytes(bad)), -1)
        self.assertEqual(self.open(archive()),0)
        for name in ('../DICE.GAM', '/DICE.GAM', 'C:/DICE.GAM', 'a/../../DICE.GAM'):
            self.assertIsNone(self.readfile(name))

    def test_nested_assets_noise_crc_and_descriptor(self):
        class Streaming(io.BytesIO):
            def seekable(self): return False
            def seek(self,*a): raise io.UnsupportedOperation()
        b=Streaming()
        with zipfile.ZipFile(b,'w',zipfile.ZIP_DEFLATED) as z:
            z.writestr('Wrapper/DICE.GAM',(DICE/'DICE.GAM').read_bytes())
            z.writestr('Wrapper/Nested assets/Some file.gif',b'asset bytes'*500)
            z.writestr('__MACOSX/._DICE.GAM',b'junk')
        self.assertEqual(self.open(b.getvalue()),0)
        self.assertEqual(self.readfile('Nested assets/Some file.gif'), b'asset bytes'*500)
        self.assertIsNone(self.readfile('../__MACOSX/._DICE.GAM'))
        bad=bytearray(archive()); bad[30+len('DICE.GAM')]^=1
        # whichever first file was written, its data CRC must be checked even for prefix reads.
        self.assertEqual(self.open(bytes(bad)),0)
        with zipfile.ZipFile(io.BytesIO(archive())) as z: first=z.namelist()[0]
        self.assertIsNone(self.readfile(first, 1))

    def test_mutated_zip_bounds_and_error_atomic_reads(self):
        import random
        seed=archive(zipfile.ZIP_DEFLATED, 'Wrapper/')
        rng=random.Random(1108)
        for i in range(400):
            data=bytearray(seed)
            if i%2: data=data[:rng.randrange(len(data))]
            else:
                for _ in range(1+i%4): data[rng.randrange(len(data))]^=1<<rng.randrange(8)
            if self.open(bytes(data))==0:
                out=C.create_string_buffer(b'untouched guard')
                before=out.raw
                n=self.c.gz_read(self.z,self.c.gz_gam(self.z),out,1)
                if n<0: self.assertEqual(out.raw,before)
                else: self.assertEqual(out.raw[1:],before[1:])

    def test_directory_guards_symlinks_duplicates_and_limits(self):
        good=archive()
        at=good.index(b'PK\x01\x02')
        for offset,fmt,value in ((6,'<H',45),(8,'<H',1),(10,'<H',6),(20,'<I',0xffffffff),(24,'<I',1048577),(34,'<H',1),(42,'<I',0xffffffff)):
            bad=bytearray(good);struct.pack_into(fmt,bad,at+offset,value)
            self.assertEqual(self.open(bytes(bad)),-1,(offset,value))
        bad=bytearray(good)
        local=struct.unpack_from('<I',bad,at+42)[0]
        struct.pack_into('<H',bad,at+8,1);struct.pack_into('<H',bad,local+6,1)
        self.assertEqual(self.open(bytes(bad)),-1,'coherent encrypted flags, not local/central mismatch')
        b=io.BytesIO()
        with zipfile.ZipFile(b,'w') as z:
            link=zipfile.ZipInfo('DICE.GAM');link.create_system=3;link.external_attr=0o120777<<16
            z.writestr(link,(DICE/'DICE.GAM').read_bytes())
        self.assertEqual(self.open(b.getvalue()),-1)
        b=io.BytesIO()
        with zipfile.ZipFile(b,'w') as z:
            z.writestr('DICE.GAM',(DICE/'DICE.GAM').read_bytes());z.writestr('dice.gam',b'alias collision')
        self.assertEqual(self.open(b.getvalue()),-1)
        b=io.BytesIO()
        with zipfile.ZipFile(b,'w') as z:
            for i in range(1025):z.writestr(f'{i}.txt',b'')
        self.assertEqual(self.open(b.getvalue()),-1)

    def test_stored_dice_reads_every_asset(self):
        self.assertEqual(self.open(archive()), 0)
        self.assertEqual(self.c.gz_gam(self.z), b'DICE.GAM')
        for p in DICE.iterdir():
            self.assertEqual(self.readfile(p.name.lower()), p.read_bytes(), p.name)
        self.assertEqual(self.readfile('DICE.GAM', 9), (DICE/'DICE.GAM').read_bytes()[:9])
        self.assertIsNone(self.readfile('missing.gif'))
