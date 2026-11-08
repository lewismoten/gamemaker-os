#!/usr/bin/env python3
"""Measure actual kernel asset reader SD-read amplification on synthetic FAT.
Usage: python3 profile_fat.py KERNEL OUTDIR. No hardware; no kernel changes.
"""
import pathlib,subprocess,sys,json,ctypes as C,time
k,o=map(lambda s:pathlib.Path(s).resolve(),sys.argv[1:3]);o.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(k/'tests'));import fatgen
(o/'shim.c').write_text('''#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
static FILE *f;static uint64_t reads;
void open_disk(const char*p){f=fopen(p,"rb");reads=0;}
void close_disk(void){fclose(f);}
uint64_t read_count(void){return reads;}
uint32_t sd_sectors(void){return 264192;}
bool sd_read(uint32_t lba,void*p){reads++;return !fseek(f,(long)lba*512,SEEK_SET)&&fread(p,1,512,f)==512;}
''')
cmd=['cc','-shared','-fPIC','-O2','-DHOST_TEST','-I'+str(k/'include'),str(k/'core/assets.c'),str(o/'shim.c'),'-o',str(o/'assets.so')];subprocess.run(cmd,check=True)
c=C.CDLL(str(o/'assets.so'));c.open_disk.argtypes=[C.c_char_p];c.read_count.restype=C.c_uint64
class Entry(C.Structure):_fields_=[('name',C.c_char*13),('attr',C.c_uint8),('is_dir',C.c_uint8),('reserved',C.c_uint8),('size',C.c_uint32)]
c.asset_open.argtypes=[C.c_char_p,C.c_uint32,C.c_uint32];c.asset_readdir.argtypes=[C.c_int,C.POINTER(Entry)]
rows=[]
for n in [1,16,64,122,135]:
 tree={'GAMES':{f'G{i:07}':{'GAME.GAM':bytes(1315)} for i in range(n)}}
 disk=o/'card.img';fatgen.build(disk,tree,spc=2,fragment=True);c.open_disk(str(disk).encode());assert c.asset_mount()==0
 def opened(p):
  p=p.encode();h=c.asset_open(p,len(p),0);assert h>=0,(p,h);return h
 def listing(h):
  e=Entry();names=[]
  while True:
   r=c.asset_readdir(h,C.byref(e));assert r>=0
   if not r:break
   names.append(e.name.decode())
  return names
 start=c.read_count();h=opened('/GAMES');names=listing(h);c.asset_close(h);enum_reads=c.read_count()-start
 start=c.read_count();t=time.perf_counter_ns()
 for name in names:
  h=opened('/GAMES/'+name);assert listing(h)==['GAME.GAM'];c.asset_close(h)
  h=opened('/GAMES/'+name+'/GAME.GAM');c.asset_close(h)
 elapsed=time.perf_counter_ns()-t;path_reads=c.read_count()-start
 c.close_disk();rows.append(dict(folders=n,enumeration_sd_read_calls=enum_reads,two_opens_per_folder_sd_read_calls=path_reads,host_elapsed_ns=elapsed))
(o/'results.json').write_text(json.dumps(dict(method='Actual unchanged core/assets.c, synthetic fragmented FAT32, 2 sectors/cluster; RAM/stdio-backed 512-byte sd_read counts, no real SD latency. Full GAMES listing then directory open/list and GAM open (no payload read) per folder.',kernel=str(k),command=cmd,rows=rows),indent=2));print(json.dumps(rows,indent=2));disk.unlink()
