#!/usr/bin/env python3
"""Build a host-only probe of the actual main.c scanner. Never writes the card.
Usage: python3 profile_scan.py REPO CARD OUTDIR [REPEATS]
CARD contains GAMES and optionally POCKET, OS. Native readdir order, not FAT.
"""
import sys,pathlib,subprocess,hashlib,json,platform,os,shlex
repo,card,out=map(lambda s:pathlib.Path(s).resolve(),sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
text=(repo/'src/main.c').read_text(); scan=text.split('static void open_input(void)')[0]
assert scan.endswith('}\n') and 'static void find_games(void)' in scan
shim=out/'sandbox';shim.mkdir(exist_ok=True)
(shim/'syscall.h').write_text('''#pragma once
#include <sandbox/abi.h>
int32_t sbx_open(const char*,uint32_t,uint32_t,uint64_t*);
int32_t sbx_read(uint64_t,void*,uint32_t,uint64_t*);
int32_t sbx_close(uint64_t);
int32_t sbx_getdents(uint64_t,void*,uint32_t,uint64_t*);
int32_t sbx_seek(uint64_t,int64_t,uint64_t*);
int32_t sbx_health(uint32_t,uint64_t);
int32_t sbx_log(uint64_t,const char*,uint32_t);
''')
# Rename definitions only, so original calls pass through inclusive timers.
protos='static void find_gam(const char*,unsigned,char[13]);\nstatic void add_game(const char*,unsigned);\nstatic void read_index(void);\nstatic void add_pack(const struct sp_pack*);\nstatic void find_packs(uint8_t);\n'
for name in ['find_gam','add_game','read_index','add_pack','find_packs']:
 scan=scan.replace('static void '+name+'(', 'static void '+name+'_impl(',1)
# Declarations follow real headers, before scanner's first declaration.
scan=scan.replace('static uint64_t log_fd',protos+'static uint64_t log_fd',1)
source=r'''
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
static const char *root;
static struct {DIR *d; int f; char path[4096];} handles[64];
static uint64_t opens,dir_opens,file_opens,read_calls,read_bytes,dent_calls,dent_entries,logs,log_bytes,healths,seeks;
static uint64_t io_ns,crc_ns,crc_calls,crc_bytes,lib_ns,lib_calls;
static uint64_t phase_ns[5],phase_calls[5];
'''+scan+r'''
static void find_gam(const char*p,unsigned n,char b[13]){uint64_t t=ns();find_gam_impl(p,n,b);phase_ns[0]+=ns()-t;phase_calls[0]++;}
static void add_game(const char*p,unsigned n){uint64_t t=ns();add_game_impl(p,n);phase_ns[1]+=ns()-t;phase_calls[1]++;}
static void read_index(void){uint64_t t=ns();read_index_impl();phase_ns[2]+=ns()-t;phase_calls[2]++;}
static void add_pack(const struct sp_pack*p){uint64_t t=ns();add_pack_impl(p);phase_ns[3]+=ns()-t;phase_calls[3]++;}
static void find_packs(uint8_t f){uint64_t t=ns();find_packs_impl(f);phase_ns[4]+=ns()-t;phase_calls[4]++;}
uint32_t __real_gm_crc32(uint32_t,const uint8_t*,uint32_t);
uint32_t __wrap_gm_crc32(uint32_t c,const uint8_t*b,uint32_t n){uint64_t t=ns();uint32_t r=__real_gm_crc32(c,b,n);crc_ns+=ns()-t;crc_calls++;crc_bytes+=n;return r;}
int __real_gm_library_line(struct gm_menu*,const char*,uint32_t);
int __wrap_gm_library_line(struct gm_menu*m,const char*b,uint32_t n){uint64_t t=ns();int r=__real_gm_library_line(m,b,n);lib_ns+=ns()-t;lib_calls++;return r;}
int32_t sbx_open(const char*p,uint32_t n,uint32_t flags,uint64_t*out){
 (void)flags;uint64_t t=ns();opens++;if(n<3||memcmp(p,"/sd",3))return -2;
 unsigned h;for(h=1;h<64;h++)if(!handles[h].d&&handles[h].f<0)break;if(h==64)return -24;
 snprintf(handles[h].path,sizeof handles[h].path,"%s%.*s",root,(int)n-3,p+3);
 struct stat st;if(stat(handles[h].path,&st)){io_ns+=ns()-t;return -2;}
 if(S_ISDIR(st.st_mode)){handles[h].d=opendir(handles[h].path);if(!handles[h].d)return -2;dir_opens++;}
 else {handles[h].f=open(handles[h].path,O_RDONLY);if(handles[h].f<0)return -2;file_opens++;}
 *out=h;io_ns+=ns()-t;return 0;
}
int32_t sbx_read(uint64_t h,void*b,uint32_t n,uint64_t*got){uint64_t t=ns();read_calls++;ssize_t r=read(handles[h].f,b,n);io_ns+=ns()-t;if(r<0)return -5;*got=r;read_bytes+=r;return 0;}
int32_t sbx_close(uint64_t h){if(handles[h].d){closedir(handles[h].d);handles[h].d=0;}else if(handles[h].f>=0){close(handles[h].f);handles[h].f=-1;}return 0;}
int32_t sbx_seek(uint64_t h,int64_t off,uint64_t*out){uint64_t t=ns();seeks++;off_t p=lseek(handles[h].f,off,SEEK_SET);io_ns+=ns()-t;if(p<0)return -5;*out=p;return 0;}
int32_t sbx_getdents(uint64_t h,void*b,uint32_t cap,uint64_t*got){
 uint64_t t=ns();dent_calls++;uint32_t at=0;struct dirent*e;
 while(1){long pos=telldir(handles[h].d);e=readdir(handles[h].d);if(!e)break;if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
 size_t nb=strlen(e->d_name);uint32_t len=(8+nb+7)&~7u;if(nb>255)continue;if(len>cap-at){seekdir(handles[h].d,pos);break;}
 struct stat st; if(fstatat(dirfd(handles[h].d),e->d_name,&st,0))continue;
 sbx_dirent d={(uint16_t)len,S_ISDIR(st.st_mode)?SBX_DT_DIR:SBX_DT_FILE,(uint8_t)nb,S_ISDIR(st.st_mode)?0:(uint32_t)st.st_size};
 memset((char*)b+at,0,len);memcpy((char*)b+at,&d,8);memcpy((char*)b+at+8,e->d_name,nb);at+=len;dent_entries++;
 }*got=at;io_ns+=ns()-t;return 0;
}
int32_t sbx_log(uint64_t h,const char*b,uint32_t n){(void)h;(void)b;logs++;log_bytes+=n;return 0;}
int32_t sbx_health(uint32_t g,uint64_t s){(void)g;(void)s;healths++;return 0;}
static void jstr(const char*s){putchar('"');for(;*s;s++){unsigned char c=*s;if(c=='"'||c=='\\')printf("\\%c",c);else if(c<32||c>=127)printf("\\u%04x",c);else putchar(c);}putchar('"');}
int main(int argc,char**argv){if(argc!=2)return 2;root=argv[1];for(int i=0;i<64;i++)handles[i].f=-1;sp_set_abi(1,3);uint64_t t=ns();find_games();t=ns()-t;
 printf("{\"elapsed_ns\":%llu,\"games\":%u,\"menu_crc\":%u,\"where_crc\":%u",(unsigned long long)t,menu.count,__real_gm_crc32(0,(const uint8_t*)&menu,sizeof menu),__real_gm_crc32(0,(const uint8_t*)where,sizeof where));
 #define FIELD(x) printf(",\"" #x "\":%llu",(unsigned long long)x)
 FIELD(opens);FIELD(dir_opens);FIELD(file_opens);FIELD(read_calls);FIELD(read_bytes);FIELD(dent_calls);FIELD(dent_entries);FIELD(logs);FIELD(log_bytes);FIELD(healths);FIELD(seeks);FIELD(io_ns);FIELD(crc_ns);FIELD(crc_calls);FIELD(crc_bytes);FIELD(lib_ns);FIELD(lib_calls);
 printf(",\"inclusive_phase_ns\":[");for(unsigned i=0;i<5;i++)printf("%s%llu",i?",":"",(unsigned long long)phase_ns[i]);printf("],\"phase_calls\":[");for(unsigned i=0;i<5;i++)printf("%s%llu",i?",":"",(unsigned long long)phase_calls[i]);printf("],\"list\":[");
 for(unsigned j=0;j<menu.count;j++){
  unsigned i=menu.order[j];const struct gm_game*g=&menu.games[i];
  printf("%s{\"dir\":",j?",":"");jstr(g->dir);printf(",\"gam\":");jstr(g->gam);
  printf(",\"title\":");jstr(g->title);printf(",\"company\":");jstr(g->company);printf(",\"blurb\":");jstr(g->blurb);
  printf(",\"storage\":%u,\"crc\":%u,\"cat\":%u,\"course\":%d,\"year\":%d,\"packed\":%u",i,g->crc,g->cat,g->course,g->year,where[i].packed);
  if(where[i].packed){const struct sp_pack*p=&where[i].pack;printf(",\"pack_folder\":%u,\"pack_base\":",p->folder);jstr(p->base);printf(",\"volume_sizes\":[");for(unsigned v=0;v<p->volumes;v++)printf("%s%u",v?",":"",p->size[v]);printf("]");}
  printf("}");
 }puts("]}");for(int i=1;i<64;i++)if(handles[i].d||handles[i].f>=0)sbx_close(i);return 0;}
'''
(out/'probe.c').write_text(source)
extra=shlex.split(os.environ.get('GMOS_SCAN_CFLAGS',''))
flags=['-O2',*extra,'-ffunction-sections','-fdata-sections','-I'+str(out),'-I'+str(repo/'sdk/include'),'-I'+str(repo/'src'),'-I'+str(repo/'third_party/gmcore'),'-I'+str(repo/'third_party/puff')]
commands=[]
for name,src,compiler in [('probe','probe.c','cc'),('sdpack',str(repo/'src/sdpack.c'),'cc'),('menu',str(repo/'src/menu.c'),'cc'),('library',str(repo/'src/library.c'),'cc'),('gam',str(repo/'src/gam.c'),'cc'),('gmp',str(repo/'src/gmp.cpp'),'c++'),('puff',str(repo/'third_party/puff/puff.c'),'cc')]:
 cmd=[compiler,*flags,'-c',src,'-o',name+'.o'];commands.append(cmd);subprocess.run(cmd,cwd=out,check=True)
cmd=['c++',*extra,'-Wl,--gc-sections','-Wl,--wrap=gm_crc32','-Wl,--wrap=gm_library_line',*[str(out/(x+'.o')) for x in ['probe','sdpack','menu','library','gam','gmp','puff']],'-o',str(out/'scan-probe')];commands.append(cmd);subprocess.run(cmd,check=True)
runs=[json.loads(subprocess.check_output([str(out/'scan-probe'),str(card)])) for _ in range(int(sys.argv[4]) if len(sys.argv)>4 else 5)]
result=dict(environment=platform.platform(),method='Native POSIX transport; original scanner prefix with inclusive timing wrappers; no FAT, UART, USB or rendering. Log sink counts only.',repo=str(repo),card=str(card),source_sha256=hashlib.sha256(text.encode()).hexdigest(),phases=['find_gam','add_game','read_index','add_pack','find_packs'],commands=commands,runs=runs)
(out/'results.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in runs[0].items() if k!='list'},indent=2))
