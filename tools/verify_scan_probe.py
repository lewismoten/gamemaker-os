#!/usr/bin/env python3
"""Verify baseline probe semantics against real repository fixtures, not cache tests.
Usage: python3 verify_probe.py REPO COMPILED_PROBE OUTDIR
"""
import pathlib,sys,json,shutil
r,exe,out=map(lambda s:pathlib.Path(s).resolve(),sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
import subprocess
rows=[]
def run(label,setup):
 c=out/label;(c/'GAMES').mkdir(parents=True);setup(c)
 d=json.loads(subprocess.check_output([str(exe),str(c)]));rows.append(dict(case=label,**d));return d

def dice(c):shutil.copytree(r/'tests/games/DICE',c/'GAMES/DICE')
def indexed(c):dice(c);shutil.copyfile(r/'tests/data/library.jsl',c/'GAMES/LIBRARY.JSL')
a=run('missing-index',dice);assert a['games']==1 and a['crc_calls']==1
b=run('valid-index',indexed)
def identities(d):return [{k:g[k] for k in ('dir','gam','crc','packed','storage')} for g in d['list']]
assert b['games']==1 and identities(b)==identities(a) and b['crc_calls']==1
assert b['list'][0]['title']=='Roll the Dice'

def corrupt(c):dice(c);(c/'GAMES/LIBRARY.JSL').write_text('{bad JSON\n')
x=run('corrupt-index',corrupt);assert x['list']==a['list']
def stale(c):dice(c);(c/'GAMES/LIBRARY.JSL').write_text('{"schema":"gmp-library","title":"Missing","locator":"ABSENT","gamCrc32":"12345678"}\n')
x=run('stale-index',stale);assert x['list']==a['list']
def added(c):indexed(c);shutil.copytree(c/'GAMES/DICE',c/'GAMES/SECOND');p=c/'GAMES/SECOND/DICE.GAM';b=bytearray(p.read_bytes());b[285:289]=b'TEST';p.write_bytes(b)
x=run('added-distinct-folder',added);assert x['games']==2 and x['crc_calls']==2
def removed(c):indexed(c);shutil.rmtree(c/'GAMES/DICE')
x=run('removed-folder',removed);assert x['games']==0 and x['crc_calls']==0
def dupe(c):indexed(c);shutil.copytree(c/'GAMES/DICE',c/'GAMES/SECOND')
x=run('duplicate-folders',dupe);assert x['games']==1 and x['crc_calls']==2
def packed(c):indexed(c);(c/'POCKET').mkdir();shutil.copyfile(r/'tests/data/dice.gmp',c/'POCKET/DICE.GMP')
x=run('packed-priority',packed);assert x['games']==1 and x['list'][0]['packed']==1 and x['crc_calls']==2
(out/'verification.json').write_text(json.dumps(dict(passed=len(rows),cases=rows),indent=2));print('Verified',len(rows),'baseline cases; these verify current scanner, not a future cache.')
