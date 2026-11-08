#!/usr/bin/env python3
"""Generate a SYNTHETIC 135-game FAT fixture, never a purported real SD copy.
Usage: make_scan_fixture.py GMOS_REPO KERNEL_REPO OUT_IMAGE
Uses one repository DICE GAM, 135 last-byte variants, one duplicate DICE pack,
an empty folder and generated JSL titles. 2-sector clusters, fragmented chains.
"""
import hashlib,json,pathlib,sys,importlib.util
repo,kernel,target=map(lambda x:pathlib.Path(x).resolve(),sys.argv[1:4])
spec=importlib.util.spec_from_file_location('scan_fat_fixture_generator',kernel/'tests/fatgen.py')
assert spec and spec.loader
fatgen=importlib.util.module_from_spec(spec);spec.loader.exec_module(fatgen)
data=(repo/'tests/games/DICE/DICE.GAM').read_bytes();games={};records=[]
for i in range(135):
 name=f'G{i:07}';d=bytearray(data);d[-1]^=i;games[name]={'DICE.GAM':bytes(d)}
 records.append(json.dumps(dict(schema='gmp-library',title=f'Synthetic {i:03}',locator='DICE' if i==0 else name,category=['sandbox'])))
games['EMPTY']={};games['LIBRARY.JSL']=('\n'.join(records)+'\n').encode()
target.parent.mkdir(parents=True,exist_ok=True)
v=fatgen.build(target,{'OS':{},'POCKET':{'DICE.GMP':(repo/'tests/data/dice.gmp').read_bytes()},'GAMES':games},spc=2,fragment=True)
metadata=dict(kind=__doc__,gam_sha256=hashlib.sha256(data).hexdigest(),expected_games=135,spc=2,fragmented=True,
 entries={k:v.entry_offset(k) for k in v.entries},chains=v.chains)
target.with_suffix('.json').write_text(json.dumps(metadata,indent=2))
print(target)
