#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Optional exact-kernel integration: scanner + board readers + real game session.
Usage: GMOS_HOST=build/host python3 tests/smoke_drag_drop_kernel.py KERNEL OUTDIR
Host C, checked SDK records, real FAT/syscall dispatcher; no ARM execution/Pi.
Synthetic wire FAT image with real DICE assets; not a macOS/Windows writer.
"""
import ctypes as C
import hashlib
import io
import json
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import zipfile
import zlib

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from test_play import Session, TITLE_AT, INTRO_AT, LEVEL_AT, PLAY_AT, tick_ns
from test_zip import archive, DICE

BRIDGE=r'''
int32_t sbx_getdents_meta(uint64_t f,void *p,uint32_t n,uint64_t *done){
 sbx_dirents_rec r={H(sbx_dirents_rec),f,(uintptr_t)p,n,0};int s=dispatch(18,&r,sizeof r);if(!s)*done=r.transferred;return s;}

int32_t sbx_write(uint64_t f,const void *p,uint32_t n,uint64_t *done){
 sbx_io_rec r={H(sbx_io_rec),f,(uintptr_t)p,n,0};int s=dispatch(3,&r,sizeof r);if(!s)*done=r.transferred;return s;}
int32_t sbx_fsync(uint64_t f){sbx_close_rec r={H(sbx_close_rec),f};return dispatch(12,&r,sizeof r);}
int32_t sbx_rename(const char*a,uint32_t an,const char*b,uint32_t bn){sbx_rename_rec r={H(sbx_rename_rec),(uintptr_t)a,an,(uintptr_t)b,bn};return dispatch(19,&r,sizeof r);}
int32_t sbx_health(uint32_t g,uint64_t n){(void)g;(void)n;return 0;}
int32_t sbx_log(uint64_t f,const char *p,uint32_t n){(void)f;(void)p;(void)n;return 0;}
'''
EXPORTS=r'''
#include <string.h>
static struct gs_session probe_session;
void probe_release(void){struct sp_pack empty={0};sz_close();sp_open(&empty);}
unsigned probe_scan(unsigned minor){probe_release();memset(&menu,0,sizeof menu);memset(where,0,sizeof where);abi_minor=minor;sp_set_abi(1,minor);find_games();return menu.count;}
unsigned probe_kind(unsigned i){return where[i].packed;}
const char *probe_path(unsigned i){return where[i].path;}
const char *probe_gam(unsigned i){return where[i].gam;}
int32_t probe_read(unsigned i,const char *name,uint8_t *out,uint32_t cap){void *u;gp_read_fn r=reader(&menu.games[i],&u);return r?r(u,name,out,cap):-1;}
int probe_start(unsigned i){void *u;gp_read_fn r=reader(&menu.games[i],&u);memset(&probe_session,0,sizeof probe_session);return r?gs_start(&probe_session,r,u,where[i].gam,0):-1;}
int probe_step(uint64_t t){return gs_step(&probe_session,t);}
int probe_event(unsigned code,int value){return gs_event(&probe_session,1,(uint16_t)code,value);}
unsigned probe_ticks(void){return probe_session.total_ticks;}
unsigned probe_phase(void){return (unsigned)probe_session.phase;}
unsigned probe_hits(void){return cache_hits;}
uint32_t probe_frame(void){static uint32_t fb[640*480];gs_draw(&probe_session,fb,640);return gm_crc32(0,(const uint8_t *)fb,sizeof fb);}
'''

def run(kernel,out):
    out.mkdir(parents=True,exist_ok=True)
    sys.path.insert(0,str(kernel/'tests'))
    import fatlfn
    import test_m1_user as m1
    # Same immutable play core, reference reads directly from loose fixture.
    reference=Session();assert reference.result==0
    frames=[]
    for t in (TITLE_AT,INTRO_AT,LEVEL_AT,PLAY_AT):
        reference.step(t)
        frames.append(zlib.crc32(b''.join(struct.pack('<I',p) for p in reference.frame())))
    hashes=[]
    for tick in range(1,91):
        if tick==3:reference.key(106,1)
        if tick==60:reference.key(106,0)
        reference.step(PLAY_AT+tick_ns(tick));hashes.append(reference.c.gp_frame_hash())
    assert reference.ticks()==90
    # Generate host declarations, preserving exact production scanner/reader bodies.
    shim=out/'sandbox';shim.mkdir(exist_ok=True)
    header=(ROOT/'tests/host/sdpack/sandbox/syscall.h').read_text()+r'''
int32_t sbx_getdents_meta(uint64_t,void*,uint32_t,uint64_t*);
int32_t sbx_getdents_long_meta(uint64_t,void*,uint32_t,uint64_t*);
int32_t sbx_write(uint64_t,const void*,uint32_t,uint64_t*);
int32_t sbx_fsync(uint64_t);
int32_t sbx_rename(const char*,uint32_t,const char*,uint32_t);
int32_t sbx_health(uint32_t,uint64_t);
int32_t sbx_log(uint64_t,const char*,uint32_t);
'''
    (shim/'syscall.h').write_text(header)
    main=(ROOT/'src/main.c').read_text()
    reader=main[main.index('static int32_t sd_read('):main.index('static uint64_t clock_ns(')]
    scanner=main.split('static void open_input(void)')[0]
    (out/'scanner.c').write_text(scanner+reader+EXPORTS)
    (out/'bridge.c').write_text((ROOT/'tests/host/sdpack_kernel.c').read_text()+BRIDGE)
    so=out/'scan-kernel.so';host=(ROOT/os.environ.get('GMOS_HOST','build/host')).resolve()
    sanitizer=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=undefined'] if 'asan' in str(host) else []
    cmd=['cc','-shared','-fPIC','-O1',*sanitizer,'-std=c11','-I'+str(out),'-I'+str(ROOT/'sdk/include'),'-I'+str(ROOT/'src'),
         str(out/'scanner.c'),str(out/'bridge.c'),str(ROOT/'src/sdpack.c'),str(ROOT/'src/sdzip.c'),str(host/'libgmos.so'),'-Wl,-rpath,'+str(host),'-o',str(so)]
    subprocess.run(cmd,check=True)
    if sanitizer:
        import hostlib
        kernel_cmd=['cc','-shared','-fPIC','-O1','-fno-builtin','-DHOST_TEST','-D_FORTIFY_SOURCE=0',*sanitizer,
                    *(['-Wl,-Bsymbolic'] if sys.platform.startswith('linux') else []),
                    '-I'+str(kernel/'tests/stubs'),'-I'+str(kernel/'include'),
                    *[str(kernel/s) for s in hostlib.SOURCES],'-o',str(out/'core.dylib')]
        subprocess.run(kernel_cmd,check=True)
    c=C.CDLL(str(so));c.probe_read.argtypes=[C.c_uint,C.c_char_p,C.c_void_p,C.c_uint32]
    for f in ('probe_path','probe_gam'):getattr(c,f).restype=C.c_char_p
    c.probe_step.argtypes=[C.c_uint64];c.gp_frame_hash.restype=C.c_uint32;c.probe_frame.restype=C.c_uint32
    CALLBACK=C.CFUNCTYPE(C.c_int32,C.c_uint32,C.c_void_p,C.c_uint32)
    counts={};failures=[];k=None;fixture=None;staging=0x09000000
    def poke(va,data):
        for at in range(0,len(data),4096):m1.poke(k,va+at,data[at:at+4096])
    def peek(va,n):return b''.join(m1.peek(k,va+at,min(4096,n-at)) for at in range(0,n,4096))
    def transport(call,address,size):
        try:
            counts[call]=counts.get(call,0)+1
            raw=bytearray(C.string_at(address,size));hostbuf=None
            if call==1:
                ptr,n=struct.unpack_from('<QQ',raw,16);poke(staging,C.string_at(ptr,n));struct.pack_into('<Q',raw,16,staging)
            elif call in (2,3,11,18,20):
                hostbuf,n=struct.unpack_from('<QQ',raw,24);assert n<=1<<20
                if call==3:poke(staging,C.string_at(hostbuf,n))
                struct.pack_into('<Q',raw,24,staging)
            elif call==19:
                a,an,b,bn=struct.unpack_from('<QQQQ',raw,16)
                poke(staging,C.string_at(a,an));poke(staging+4096,C.string_at(b,bn));struct.pack_into('<QQQQ',raw,16,staging,an,staging+4096,bn)
            status=fixture.call(call,bytes(raw));result=m1.peek(k,fixture.REC,size)
            if not status:
                if hostbuf and call!=3:
                    got=struct.unpack_from('<Q',result,40)[0];C.memmove(hostbuf,peek(staging,got),got)
                C.memmove(address,result,size)
            return status
        except Exception as exc:failures.append(repr(exc));return -5
    callback=CALLBACK(transport);c.sdk_bridge.argtypes=[CALLBACK];c.sdk_bridge(callback)
    assets={p.name:p.read_bytes() for p in DICE.iterdir()}
    loose=dict(assets);loose['NESTED']={'DEEP.BIN':b'nested payload'}
    pack=(ROOT/'tests/data/dice.gmp').read_bytes()
    cases=[('long-loose',{'LONGFO~1':loose},{'/GAMES/LONGFO~1':'Long Game Folder with spaces','/GAMES/LONGFO~1/NESTED':'Long nested asset directory','/GAMES/LONGFO~1/NESTED/DEEP.BIN':'Long asset filename.bin'},0,'/sd/GAMES/Long Game Folder with spaces'),
           ('parent-and-child',{'PARENT':{**assets,'NESTED':{**assets,'DICE.GAM':assets['DICE.GAM']+b'\0'}}},{},0,'/sd/GAMES/PARENT/NESTED'),
           ('long-main-gam',{'LONGFO~1':loose},{'/GAMES/LONGFO~1':'Café Game Folder','/GAMES/LONGFO~1/DICE.GAM':'Game Metadata with long name.gam'},0,'/sd/GAMES/Café Game Folder'),
           ('long-gmp',{'LONGGA~1.GMP':pack},{'/GAMES/LONGGA~1.GMP':'Long renamed packed game.GMP'},1,None),
           ('nested-loose',{'COLLEC~1':{'LONGFO~1':loose}},{'/GAMES/COLLEC~1':'Collection with spaces','/GAMES/COLLEC~1/LONGFO~1':'Nested long Game Folder'},0,'/sd/GAMES/Collection with spaces/Nested long Game Folder'),
           ('stored-zip',{'LONGAR~1.ZIP':archive()},{'/GAMES/LONGAR~1.ZIP':'An original style stored archive.zip'},2,'/sd/GAMES/An original style stored archive.zip'),
           ('deflated-wrapper-sfx',{'LONGAR~1.EXE':archive(zipfile.ZIP_DEFLATED,'Wrapper Folder/',b'MZ'+bytes(4094))},{'/GAMES/LONGAR~1.EXE':'Long ZIP SFX Archive.exe'},2,'/sd/GAMES/Long ZIP SFX Archive.exe'),
           ('gmp-wins',{'LONGFO~1':loose,'ARCHIVE.ZIP':archive(),'DICE.GMP':pack},{'/GAMES/LONGFO~1':'Long folder duplicate'},1,'/sd/GAMES/DICE.GMP'),
           ('folder-wins',{'LONGFO~1':loose,'ARCHIVE.ZIP':archive()},{'/GAMES/LONGFO~1':'Long folder duplicate'},0,'/sd/GAMES/Long folder duplicate'),
           ('desktop-noise',{'JUNK.GMP':pack,'JUNK.ZIP':archive(),'JUNKDIR':loose},
            {'/GAMES/JUNK.GMP':'._DICE.GMP','/GAMES/JUNK.ZIP':'._DICE.ZIP','/GAMES/JUNKDIR':'System Volume Information'},-1,'')]
    corpus={}
    for num,p in enumerate(filter(None,os.environ.get('GMOS_RSD_ARCHIVES','').split(os.pathsep))):
        p=pathlib.Path(p).resolve();data=p.read_bytes();label=f'legacy-{num}'
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            payload={i.filename:z.read(i) for i in z.infolist() if not i.is_dir()}
        corpus[label]=(p,hashlib.sha256(data).hexdigest(),payload)
        cases.append((label,{'LEGACY.ZIP':data},{'/GAMES/LEGACY.ZIP':p.name},2,'/sd/GAMES/'+p.name))
    results=[]
    for label,games,names,kind,path in cases:
        selected=1 if label=='parent-and-child' else 0
        expected_count=2 if label=='parent-and-child' else 1
        k=m1.lib(out);fixture=m1.CallsBase();fixture.c=k
        disk=out/(label+'.img');tree={'OS':{'DIAG.ELF':m1.program()},'GAMES':games}
        if os.environ.get('MTOOLS_BIN'):
            import fatgen
            fatgen.build(disk,{})
            subprocess.run([shutil.which('mkfs.fat'),'-F','32','-s','1','-h','2048','--offset','2048',str(disk)],check=True,stdout=subprocess.DEVNULL)
            source=out/(label+'-desktop-source');source.mkdir(exist_ok=True)
            def materialize(tree,base,prefix=''):
                for alias,value in tree.items():
                    full=prefix+'/'+alias;dest=base/names.get(full,alias)
                    if isinstance(value,dict):dest.mkdir(exist_ok=True);materialize(value,dest,full)
                    else:dest.write_bytes(value)
            materialize(tree,source)
            subprocess.run([str(pathlib.Path(os.environ['MTOOLS_BIN'])/'mcopy'),'-i',str(disk)+'@@1048576','-s',str(source/'GAMES'),str(source/'OS'),'::/'],check=True,capture_output=True)
        else:fatlfn.build(disk,tree,names,spc=2,fragment=True)
        k.open_disk(os.fsencode(disk));assert k.asset_mount()==0;k.fb_setup(0,True);assert k.os_start(b'/OS/DIAG.ELF')==0
        assert k.us_map(staging,1<<20,m1.R|m1.W)==0
        found=c.probe_scan(5)
        if kind<0:
            assert found==0,(label,found)
            c.probe_release();k.close_disk();results.append(dict(case=label,games=0));continue
        assert found==expected_count,label
        assert c.probe_kind(selected)==kind,(label,c.probe_kind(selected))
        if path is None:path=c.probe_path(selected).decode()
        assert c.probe_path(selected).decode()==path,(label,c.probe_path(selected))
        # The second scan uses the actual persisted FAT cache, not POSIX metadata.
        writes=k.write_count()
        assert c.probe_scan(5)==expected_count,label
        assert c.probe_hits()>0 and k.write_count()==writes,(label,'warm cache missing/re-written')
        outbuf=C.create_string_buffer(2<<20)
        expected_assets=corpus[label][2] if label in corpus else assets
        if label=='parent-and-child':expected_assets={**assets,'DICE.GAM':assets['DICE.GAM']+b'\0'}
        if label=='long-main-gam':
            expected_assets=dict(assets);expected_assets['Game Metadata with long name.gam']=expected_assets.pop('DICE.GAM')
            assert c.probe_gam(selected)==b'Game Metadata with long name.gam'
        for name,data in expected_assets.items():
            n=c.probe_read(selected,name.encode(),outbuf,len(outbuf));assert n==len(data),(label,name,n,len(data));assert outbuf.raw[:n]==data,(label,name)
        if kind==0 and label!='parent-and-child':
            nested=b'Long nested asset directory/Long asset filename.bin' if label=='long-loose' else b'NESTED/DEEP.BIN'
            n=c.probe_read(selected,nested,outbuf,len(outbuf));assert outbuf.raw[:n]==b'nested payload'
        assert c.probe_read(selected,b'../DICE.GAM',outbuf,len(outbuf))==-1
        if label in corpus:
            # Independent zipfile extraction is an oracle, never extraction by GMOS.
            folder=out/(label+'-oracle');folder.mkdir(exist_ok=True)
            for name,data in expected_assets.items():
                dest=(folder/name).resolve();assert dest.is_relative_to(folder.resolve())
                dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
            oracle=Session(folder,c.probe_gam(selected));assert oracle.result==0,(label,'oracle cannot start')
            expected=[];schedule=[];oldticks=0
            for step in range(1,6001):
                t=step*20000000
                if step%50==0:oracle.key(57,1);oracle.key(57,0)
                oracle.step(t);schedule.append((step,t))
                if oracle.ticks()!=oldticks:expected.append(oracle.c.gp_frame_hash());oldticks=oracle.ticks()
                if oracle.ticks()>=90:break
            assert oracle.ticks()>=90,(label,'oracle did not reach play',oracle.log)
            assert c.probe_start(selected)==0,(label,'board reader cannot start')
            actual=[];oldticks=0
            for step,t in schedule:
                if step%50==0:c.probe_event(57,1);c.probe_event(57,0)
                c.probe_step(t)
                if c.probe_ticks()!=oldticks:actual.append(c.gp_frame_hash());oldticks=c.probe_ticks()
            assert actual==expected and c.probe_ticks()>=90,label
            c.probe_release();k.close_disk()
            results.append(dict(case=label,source=str(corpus[label][0]),sha256=corpus[label][1],assets=len(expected_assets),ticks=c.probe_ticks(),tick_sha256=hashlib.sha256(struct.pack('<'+'I'*len(actual),*actual)).hexdigest()))
            continue
        assert c.probe_start(selected)==0,label
        actualframes=[]
        for t in (TITLE_AT,INTRO_AT,LEVEL_AT,PLAY_AT):
            c.probe_step(t)
            # Compare framebuffer CRC against the same reference frame bytes.
            actualframes.append(c.probe_frame())
        assert actualframes==frames,(label,'rendered card/level frames differ')
        assert c.probe_phase()==1,label
        got=[]
        for tick in range(1,91):
            if tick==3:c.probe_event(106,1)
            if tick==60:c.probe_event(106,0)
            c.probe_step(PLAY_AT+tick_ns(tick));got.append(c.gp_frame_hash())
        assert c.probe_ticks()==90 and got==hashes,label
        assert c.probe_event(1,1)&2,label
        c.probe_release()
        k.os_request_stop();k.os_task()
        blob=(ROOT/'build/GMOS.ELF').read_bytes();plan=m1.Plan();reason=k.elf_check(blob,len(blob),C.byref(plan));assert reason is None,reason
        assert k.elf_load(blob,C.byref(plan))==0
        results.append(dict(case=label,kind=kind,path=path,assets=len(assets),ticks=90,frame_crc=actualframes,kernel_elf_bytes=plan.bytes,card_writes=k.write_count()))
        k.close_disk()
    assert not failures,failures
    assert counts.get(20,0)>0 and counts.get(19,0)>0 and counts.get(12,0)>0,counts
    report=dict(method=__doc__,sanitizers=bool(sanitizer),independent_fat_writer=bool(os.environ.get('MTOOLS_BIN')),compile=cmd,cases=results,syscall_counts=counts,arm_execution=False,physical_card=False,reference_tick_sha256=hashlib.sha256(struct.pack('<90I',*hashes)).hexdigest())
    (out/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

if __name__=='__main__':run(pathlib.Path(sys.argv[1]).resolve(),pathlib.Path(sys.argv[2]).resolve())
