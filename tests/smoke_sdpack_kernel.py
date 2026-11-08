#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Optional cross-repo smoke: GMOS board reader -> real kernel dispatcher/FAT.
python3 tests/smoke_sdpack_kernel.py /path/to/kernel-source
Uses host C/C++; never executes ARM instructions or writes a physical card.
"""
import ctypes as C
import json
import pathlib
import random
import struct
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from test_sdpack import PackInfo
from test_gmp import bundle, stored, DICE, DATA


def run(kernel):
    sys.path.insert(0, str(kernel / 'tests'))
    import fatgen
    import test_m1_user as m1
    with tempfile.TemporaryDirectory(dir=ROOT / 'build') as tmp:
        work = pathlib.Path(tmp)
        puff, gmp, bridge = work/'puff.o', work/'gmp.o', work/'bridge.so'
        subprocess.run(['cc', '-fPIC', '-c', str(ROOT/'third_party/puff/puff.c'), '-o', str(puff)], check=True)
        subprocess.run(['c++', '-fPIC', '-std=c++17', '-I'+str(ROOT/'src'), '-I'+str(ROOT/'third_party/gmcore'),
                        '-I'+str(ROOT/'third_party/puff'), '-c', str(ROOT/'src/gmp.cpp'), '-o', str(gmp)], check=True)
        subprocess.run(['cc', '-shared', '-fPIC', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-I'+str(ROOT/'tests/host/sdpack'), '-I'+str(ROOT/'sdk/include'), '-I'+str(ROOT/'src'),
                        str(ROOT/'src/sdpack.c'), str(ROOT/'src/asset_scratch.c'), str(ROOT/'tests/host/sdpack_kernel.c'),
                        str(puff), str(gmp), '-lc++' if sys.platform == 'darwin' else '-lstdc++', '-o', str(bridge)], check=True)
        c = C.CDLL(str(bridge)); k = m1.lib(work)
        fixture = m1.CallsBase(); fixture.c = k
        c.sp_list.argtypes = [C.c_uint8, C.POINTER(PackInfo), C.c_uint, C.c_void_p]
        c.sp_open.argtypes = [C.POINTER(PackInfo)]; c.sp_open.restype = C.c_void_p
        c.gmp_read_at.argtypes = [C.c_void_p, C.c_uint32, C.c_void_p, C.c_uint32]
        c.gmp_read.argtypes = [C.c_void_p, C.c_char_p, C.c_void_p, C.c_uint32]
        c.gmp_entry.argtypes = [C.c_void_p, C.c_char_p, C.c_void_p, C.c_uint32]
        counts = {}; failures = []
        CALLBACK = C.CFUNCTYPE(C.c_int32, C.c_uint32, C.c_void_p, C.c_uint32)

        def transport(call, address, size):
            try:
                counts[call] = counts.get(call, 0) + 1
                raw = bytearray(C.string_at(address, size)); hostbuf = None
                if call == 1:
                    ptr, n = struct.unpack_from('<QQ', raw, 16)
                    m1.poke(k, fixture.BUF, C.string_at(ptr, n)); struct.pack_into('<Q', raw, 16, fixture.BUF)
                elif call in (2, 11):
                    hostbuf, n = struct.unpack_from('<QQ', raw, 24)
                    struct.pack_into('<QQ', raw, 24, fixture.BUF, min(n, 4096))
                status = fixture.call(call, bytes(raw))
                result = m1.peek(k, fixture.REC, size)
                if not status:
                    if hostbuf:
                        got = struct.unpack_from('<Q', result, 40)[0]
                        C.memmove(hostbuf, m1.peek(k, fixture.BUF, got), got)
                    C.memmove(address, result, size)
                return status
            except Exception as exc:
                failures.append(repr(exc)); return -5

        callback = CALLBACK(transport); c.sdk_bridge.argtypes = [CALLBACK]; c.sdk_bridge(callback)
        gam = stored((DICE/'DICE.GAM').read_bytes())
        prefix = bundle({'dos/DICE.GAM': gam, 'zzpayload': b''})
        large = bundle({'dos/DICE.GAM': gam, 'zzpayload': random.Random(470).randbytes(2400000-len(prefix))})
        dice = (DATA/'dice.gmp').read_bytes()
        cases = [('dice-single', dice, [0, len(dice)]),
                 ('dice-index-split', dice, [0, 1000, 1001+len(dice)//3, len(dice)]),
                 ('large-three-volume', large, [0, 800000, 1600000, len(large)])]
        results = []
        for label, data, cuts in cases:
            tree = {'OS': {'DIAG.ELF': m1.program()}, 'GAMES': {}}
            for v, (a,b) in enumerate(zip(cuts,cuts[1:])):
                tree['GAMES']['DICE.GMP' if not v else f'DICE.G{v:02}'] = data[a:b]
            disk = work/'card.img'; fatgen.build(disk, tree, spc=2, fragment=True)
            k.open_disk(str(disk).encode()); assert k.asset_mount() == 0
            k.fb_setup(0, True); assert k.os_start(b'/OS/DIAG.ELF') == 0
            c.sp_set_abi(1, 3)
            packs = (PackInfo*4)(); assert c.sp_list(1, packs, 4, None) == 1
            assert packs[0].volumes == len(cuts)-1
            g = c.sp_open(C.byref(packs[0])); assert g
            offsets = [(len(data)-100, 100), (3000, 500), (4000, 100), (3000, 500), (len(data), 0)]
            offsets += [(b-11, 37) for b in cuts[1:-1]]
            offsets += [(random.Random(i).randrange(len(data)-1024), 1024) for i in range(16)]
            for off, n in offsets:
                out = C.create_string_buffer(max(1,n)); assert c.gmp_read_at(g, off, out, n) == 0
                assert out.raw[:n] == data[off:off+n], (label,off)
            out = C.create_string_buffer(2<<20)
            if label.startswith('dice'):
                for f in DICE.iterdir():
                    n = c.gmp_read(g, f.name.encode(), out, len(out)); assert n >= 0
                    assert out.raw[:n] == f.read_bytes(), f.name
            else:
                n = c.gmp_read(g, b'DICE.GAM', out, len(out)); assert out.raw[:n] == (DICE/'DICE.GAM').read_bytes()
            assert c.gmp_read_at(g, len(data)+1, out, 0) == -1
            assert c.gmp_read_at(g, len(data), out, 1) == -1
            assert k.write_count() == 0
            # Also host-load the newly built GMOS ELF on this kernel, no ARM execution.
            k.os_request_stop(); k.os_task(); assert not k.os_running()
            if (ROOT/'build/GMOS.ELF').exists():
                blob = (ROOT/'build/GMOS.ELF').read_bytes(); plan = m1.Plan()
                assert k.elf_check(blob, len(blob), C.byref(plan)) is None
                assert k.elf_load(blob, C.byref(plan)) == 0
            k.close_disk()
            results.append({'case':label, 'bytes':len(data), 'volumes':len(cuts)-1, 'random_reads':len(offsets), 'card_writes':0})
        assert not failures, failures
        assert counts.get(9,0) > 0
        print(json.dumps({'cases':results, 'real_syscall_counts':counts, 'arm_execution':False}, indent=2))

if __name__ == '__main__': run(pathlib.Path(sys.argv[1]).resolve())
