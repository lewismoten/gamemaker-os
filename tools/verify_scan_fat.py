#!/usr/bin/env python3
"""Validate actual-FAT cold/warm probe snapshots. Not a Pi assertion."""
import json,pathlib,sys
r=json.loads(pathlib.Path(sys.argv[1]).read_text())['runs']
assert len(r)>=2
for v in r[1:]:
    assert v['list']==r[0]['list'], 'cold/warm full ordered mapping mismatch'
    assert v['menu_crc']==r[0]['menu_crc'], 'cold/warm menu bytes differ'
    assert v['where_crc']==r[0]['where_crc'], 'cold/warm reader mapping bytes differ'
    assert v['crc_calls']==0, 'warm GAM payload CRC repeated'
    assert v['fat_writes']==0, 'warm cache should not rewrite card'
print('PASS:',len(r),'actual-FAT runs; cold/warm menu, mapping and warm I/O assertions')
