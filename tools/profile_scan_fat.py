#!/usr/bin/env python3
"""Optional host-only actual FAT scanner transport (no device access).
Usage: profile_scan_fat.py GMOS_REPO KERNEL_REPO FAT_IMAGE OUTDIR [REPEATS]
IMAGE MUST BE A DISPOSABLE COPY. GMOS_SCAN_CACHE=1 enables cache writes to it.
Runs the actual GMOS scanner over actual assets.c/fatput.c. This bypasses user
address translation (covered by kernel syscall tests), SD hardware and USB/UART.
"""
import hashlib,json,os,pathlib,subprocess,sys
repo,kernel,image,out=map(lambda p:pathlib.Path(p).resolve(),sys.argv[1:5])
out.mkdir(parents=True,exist_ok=True)
empty=out/'empty';empty.mkdir(exist_ok=True)
subprocess.run([sys.executable,str(pathlib.Path(__file__).with_name('profile_scan.py')),str(repo),str(empty),str(out/'posix-build'),'1'],check=True,stdout=subprocess.DEVNULL)
build=out/'posix-build';source=(build/'probe.c').read_text()
transport=r'''
#include <stdbool.h>
#include "assets.h"
#include "fatput.h"
static FILE *disk;
static uint64_t fat_reads,fat_writes;
bool sd_read(uint32_t lba,void *b){fat_reads++;return fseek(disk,(long)lba*512,SEEK_SET)==0&&fread(b,1,512,disk)==512;}
bool sd_write(uint32_t lba,const void *b){fat_writes++;return fseek(disk,(long)lba*512,SEEK_SET)==0&&fwrite(b,1,512,disk)==512&&fflush(disk)==0;}
uint32_t sd_sectors(void){long p=ftell(disk);fseek(disk,0,SEEK_END);long n=ftell(disk);fseek(disk,p,SEEK_SET);return (uint32_t)(n/512);}
void watchdog_feed(void){}
static uint8_t writebuf[FATPUT_MAX];static uint32_t writeused;static char writename[13];
static struct {bool have;struct asset_dirent d;uint32_t meta[3];} pending[8];
int32_t sbx_open(const char*p,uint32_t n,uint32_t flags,uint64_t*out){
 opens++;if(n<3||memcmp(p,"/sd",3))return -2;
 if(flags){if(!getenv("GMOS_SCAN_CACHE")||flags!=0x601||n<14||n>25||memcmp(p,"/sd/OS/CACHE/",13))return -30;
  memcpy(writename,p+13,n-13);writename[n-13]=0;writeused=0;*out=0xfeed;return 0;}
 int h=asset_open(p+3,n-3,0);if(h<0)return h;*out=(uint64_t)h;pending[h&7].have=false;
 const char *last=p+n;while(last>p&&last[-1]!='/')last--;bool file=false;for(const char*s=last;s<p+n;s++)file|=*s=='.';
 if(file)file_opens++;else dir_opens++;return 0;
}
int32_t sbx_read(uint64_t h,void*b,uint32_t n,uint64_t*got){read_calls++;int r=asset_read((int)h,b,n);if(r<0)return r;*got=(uint32_t)r;read_bytes+=(uint32_t)r;return 0;}
int32_t sbx_close(uint64_t h){return h==0xfeed?0:asset_close((int)h);}
int32_t sbx_seek(uint64_t h,int64_t off,uint64_t*pos){seeks++;if(off<0)return -22;int r=asset_seek((int)h,(uint64_t)off);if(!r)*pos=(uint64_t)off;return r;}
static int32_t fat_dents(uint64_t h,void*b,uint32_t cap,uint64_t*got,int meta){
 dent_calls++;uint32_t at=0;unsigned slot=(unsigned)h&7;
 for(;;){struct asset_dirent d;uint32_t m[3];
  if(pending[slot].have){d=pending[slot].d;memcpy(m,pending[slot].meta,12);pending[slot].have=false;}
  else {int r=asset_readdir_meta((int)h,&d,m);if(r<0)return r;if(!r)break;}
  unsigned nb=(unsigned)strlen(d.name),head=meta?24:8,len=(head+nb+7)&~7u;
  if(len>cap-at){pending[slot].have=true;pending[slot].d=d;memcpy(pending[slot].meta,m,12);if(!at)return -22;break;}
  sbx_dirent e={(uint16_t)len,d.is_dir?2:1,(uint8_t)nb,d.size};memset((char*)b+at,0,len);memcpy((char*)b+at,&e,8);
  if(meta)memcpy((char*)b+at+8,m,12);memcpy((char*)b+at+head,d.name,nb);at+=len;dent_entries++;
 }*got=at;return 0;
}
int32_t sbx_getdents(uint64_t h,void*b,uint32_t n,uint64_t*g){return fat_dents(h,b,n,g,0);}
int32_t sbx_getdents_meta(uint64_t h,void*b,uint32_t n,uint64_t*g){return fat_dents(h,b,n,g,1);}
int32_t sbx_write(uint64_t h,const void*b,uint32_t n,uint64_t*got){if(h!=0xfeed)return -9;if(n>sizeof writebuf-writeused)return -28;memcpy(writebuf+writeused,b,n);writeused+=n;*got=n;return 0;}
int32_t sbx_fsync(uint64_t h){return h==0xfeed?fatcache_put(writename,writebuf,writeused):-9;}
int32_t sbx_rename(const char*a,uint32_t an,const char*b,uint32_t bn){
 if(an<14||an>25||bn<14||bn>25||memcmp(a,"/sd/OS/CACHE/",13)||memcmp(b,"/sd/OS/CACHE/",13))return -30;
 char from[13],to[13];memcpy(from,a+13,an-13);from[an-13]=0;memcpy(to,b+13,bn-13);to[bn-13]=0;return fatcache_rename(from,to,writebuf,sizeof writebuf);
}
'''
a=source.index('int32_t sbx_open(const char*p,');b=source.index('int32_t sbx_log(uint64_t h',a)
if 'asset_readdir_meta' not in (kernel/'include/assets.h').read_text():
 transport=transport.replace('static FILE *disk;', '''
static int asset_readdir_meta(int h,struct asset_dirent *d,uint32_t m[3]){memset(m,0,12);return asset_readdir(h,d);}
static int fatcache_put(const char*n,const uint8_t*b,uint32_t z){(void)n;(void)b;(void)z;return -38;}
static int fatcache_rename(const char*a,const char*b,uint8_t*c,uint32_t n){(void)a;(void)b;(void)c;(void)n;return -38;}
static FILE *disk;''')
source=source[:a]+transport+source[b:]
source=source.replace('root=argv[1];','root=argv[1];disk=fopen(root,"r+b");if(!disk)return 3;if(asset_mount())return 4;',1)
source=source.replace('FIELD(opens);','FIELD(fat_reads);FIELD(fat_writes);FIELD(opens);',1)
(out/'fat-probe.c').write_text(source)
meta=json.loads((build/'results.json').read_text());commands=[]
flags=['-O2','-ffunction-sections','-fdata-sections','-I'+str(build),'-I'+str(repo/'sdk/include'),'-I'+str(repo/'src'),'-I'+str(kernel/'include')]
for name,src in [('fat-probe',out/'fat-probe.c'),('assets',kernel/'core/assets.c'),('fatput',kernel/'core/fatput.c')]:
 wrap=['-Dgm_crc32=__wrap_gm_crc32','-Dgm_library_line=__wrap_gm_library_line'] if name=='fat-probe' else []
 cmd=['cc',*flags,*wrap,'-c',str(src),'-o',str(out/(name+'.o'))];commands.append(cmd);subprocess.run(cmd,check=True)
# Reuse the production scanner's same separately compiled pure dependencies.
objects=[str(build/(name+'.o')) for name in ['sdpack','menu','library','gam','gmp','puff']]
if (repo/'src/scan_cache.c').exists():objects.append(str(build/'scan_cache.o'))
gcflag='-Wl,-dead_strip' if sys.platform=='darwin' else '-Wl,--gc-sections'
cmd=['c++',gcflag,*[str(out/(name+'.o')) for name in ['fat-probe','assets','fatput']],*objects,'-o',str(out/'fat-probe')]
commands.append(cmd);subprocess.run(cmd,check=True)
runs=[json.loads(subprocess.check_output([str(out/'fat-probe'),str(image)])) for _ in range(int(sys.argv[5]) if len(sys.argv)>5 else 5)]
meta.update(method=__doc__,commands=meta['commands']+commands,image=str(image),runs=runs,kernel=str(kernel),kernel_sources={p:hashlib.sha256((kernel/p).read_bytes()).hexdigest() for p in ['core/assets.c','core/fatput.c']})
(out/'results.json').write_text(json.dumps(meta,indent=2))
print(json.dumps([{k:v for k,v in r.items() if k!='list'} for r in runs],indent=2))
