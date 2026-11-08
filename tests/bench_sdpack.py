#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reproducible host index+entry timings and transport work, not SD timing.
python3 tests/bench_sdpack.py [rounds=5] [repeats=5]
Compares ABI 1.2 fallback (original reopen/replay algorithm) with ABI 1.3.
"""
import ctypes as C
import json
import pathlib
import random
import statistics
import sys
import time
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from test_sdpack import SDPack
from test_gmp import bundle, stored, DICE


def run(rounds=5, repeats=5):
    SDPack.setUpClass(); case = SDPack()
    case.setUp()
    try:
        rng = random.Random(470); gam = stored((DICE/'DICE.GAM').read_bytes())
        entries = {'dos/DICE.GAM':gam, **{f'payload{i:02}':b'' for i in range(6)}}
        available = 2400000-len(bundle(entries))
        for i in range(6): entries[f'payload{i:02}'] = rng.randbytes(available//6+(i < available%6))
        data = bundle(entries); assert len(data) == 2400000
        report = []
        for volumes, cuts in [(1,[0,len(data)]), (3,[0,800000,1600000,len(data)])]:
            case.install(data,cuts)
            for minor in (2,3):
                samples = []; work = []
                for _ in range(repeats):
                    case.setup(minor)
                    start = time.perf_counter_ns(); case.open()
                    out = C.create_string_buffer(256)
                    for _ in range(rounds):
                        for i in [5,4,3,2,1,0,3,1,5,0]:
                            key = f'payload{i:02}'
                            n = case.c.test_sp_entry(key.encode(), out, 256)
                            assert n == 256 and out.raw[:n] == entries[key][:n]
                    samples.append((time.perf_counter_ns()-start)/1e6)
                    work.append({k:case.c.test_sp_count(i) for i,k in [(0,'file_opens'),(2,'seeks'),(3,'read_bytes')]})
                assert all(w == work[0] for w in work)
                report.append({'abi':f'1.{minor}', 'volumes':volumes, 'pack_bytes':len(data),
                               'entry_reads':rounds*10, 'median_ms':statistics.median(samples),
                               'samples_ms':samples, **work[0]})
        print(json.dumps({'timing_scope':'warm host stdio; listing + index open + entry reads; no FAT/SD or hardware speed claim',
                          'repeats':repeats, 'results':report}, indent=2))
    finally: case.doCleanups()

if __name__ == '__main__': run(*(int(a) for a in sys.argv[1:]))
