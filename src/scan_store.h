/* SPDX-License-Identifier: MIT; board scan-cache transport. Included by main.c
 * after its small helpers. Discovery ALWAYS uses the live scanner; this file
 * memoizes candidate parsing only. No cached visible-survivor list or pointers.
 * /OS/CACHE is a kernel capability, enabled only on ABI >=1.4. */
#include "scan_cache.h"
static struct gc_cache scan_cache;
static uint8_t cache_old[GC_CAPACITY], cache_new[GC_CAPACITY];
static int cache_enabled;
static uint32_t cache_hits, cache_misses;
struct cache_file {char name[13];uint8_t key[16];};
static struct cache_file cache_files[3][512]; /* GAMES, POCKET, OS */
static unsigned cache_counts[3];
struct cache_source {uint32_t crc,record_bytes,valid;char gam[256],title[GM_TITLE_MAX+1];uint8_t reserved[3];};
_Static_assert(sizeof(struct cache_source)==292,"cache source wire layout");
static uint8_t cache_value[sizeof(struct cache_source)+4096];
static uint8_t cache_wire[34+255+4096];
static void cache_copy(void *out,const void *in,unsigned n) {uint8_t *a=out;const uint8_t *b=in;while(n--)*a++=*b++;}
static int cache_equal(const void *a,const void *b,unsigned n) {const uint8_t *p=a,*q=b;while(n--)if(*p++!=*q++)return 0;return 1;}
static int cache_same(const char *a,const char *b) {while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int scan_list(uint64_t fd,void *out,uint32_t cap,uint64_t *got) {
 return abi_minor>=5?sbx_getdents_long_meta(fd,out,cap,got):abi_minor>=4?sbx_getdents_meta(fd,out,cap,got):sbx_getdents(fd,out,cap,got);
}
static unsigned scan_head(void) {return abi_minor>=4?24:8;}
static int scan_dent(const sbx_dirent *e,uint32_t left) {
 return left>=scan_head()&&e->length>=scan_head()&&e->length<=left&&!(e->length&7u)&&e->name_bytes&&e->name_bytes<=e->length-scan_head();
}
static int scan_join(char out[256],const char *base,const char *name) {
 unsigned a=length(base),b=length(name);if(a+b+1>255)return 0;
 put(put(out,base),"/");put(out+a+1,name);return 1;
}
/* name_bytes is one byte, so the 256-byte name buffers below always hold a name and its NUL. */
_Static_assert(sizeof(((sbx_dirent *)0)->name_bytes)==1,"scan name buffers are 256 bytes");
/* Validate the whole record before accessing name bytes. */
static int cache_dent(const sbx_dirent_meta *e,uint32_t left) {
 return left>=sizeof *e&&e->length>=sizeof *e&&e->length<=left&&!(e->length&7u)&&
        e->name_bytes<=e->length-sizeof *e&&!e->reserved;
}
static unsigned cache_inventory(const char *path,struct cache_file *out) {
 uint64_t fd,got;_Alignas(8) static uint8_t list[2048];unsigned count=0;
 int r=sbx_open(path,length(path),SBX_O_RDONLY,&fd);
 if(r){if(r!=-SBX_ENOENT)cache_enabled=0;return 0;}
 /* Packs still use canonical volume aliases; ZIP keys come from scan_tree. */
 while(!(r=sbx_getdents_meta(fd,list,sizeof list,&got))&&got) {
  for(uint32_t at=0;at<got;){
   const sbx_dirent_meta *e=(const sbx_dirent_meta *)(const void *)(list+at);
   if(!cache_dent(e,(uint32_t)got-at)){cache_enabled=0;break;}
   if(e->type==SBX_DT_FILE&&e->name_bytes<=12){
    if(count==512){cache_enabled=0;break;}
    struct cache_file *f=out+count++;for(unsigned i=0;i<13;i++)f->name[i]=0;
    cache_copy(f->name,list+at+sizeof *e,e->name_bytes);cache_copy(f->key,&e->size,16);
   }
   at+=e->length;
  }
  if(!cache_enabled)break;
  heartbeat();
 }
 if(r)cache_enabled=0;
 sbx_close(fd);return count;
}
static const uint8_t *cache_file_key(unsigned folder,const char *name) {
 if(!cache_enabled)return 0;
 for(unsigned i=0;i<cache_counts[folder];i++)if(cache_same(cache_files[folder][i].name,name))return cache_files[folder][i].key;
 return 0;
}
static void cache_begin(void) {
 uint64_t fd,got;uint32_t n=0;cache_enabled=abi_minor>=4;cache_hits=cache_misses=0;
 if(!cache_enabled)return;
 if(!sbx_open("/sd/OS/CACHE/GAMES.IDX",22,SBX_O_RDONLY,&fd)){
  int r=0;
  while(n<sizeof cache_old&&!(r=sbx_read(fd,cache_old+n,sizeof cache_old-n,&got))&&got){n+=(uint32_t)got;heartbeat();}
  uint8_t extra;
  if(r||sbx_read(fd,&extra,1,&got)||got)n=0; /* oversized/truncated/I/O: rebuild */
  sbx_close(fd);
 }
 gc_init(&scan_cache,cache_old,n,cache_new,sizeof cache_new);
 cache_counts[0]=cache_inventory("/sd/GAMES",cache_files[0]);
 if(cache_enabled)cache_counts[1]=cache_inventory("/sd/POCKET",cache_files[1]);
 if(cache_enabled)cache_counts[2]=cache_inventory("/sd/OS",cache_files[2]);
}
/* Return the same first GAM as find_gam, with exact metadata from that entry.
 * Always enumerate even previously empty folders: parent dates aren't enough. */
static int cache_find_gam(const char *path,char name[256],uint8_t key[16]) {
 uint64_t fd,got;_Alignas(8) static uint8_t list[512];name[0]=0;
 if(!cache_enabled)return 0;
 int r=sbx_open(path,length(path),SBX_O_RDONLY,&fd);
 if(r){cache_enabled=0;return 0;}
 while(!(r=scan_list(fd,list,sizeof list,&got))&&got){
  for(uint32_t at=0;at<got;){
   const sbx_dirent_meta *e=(const sbx_dirent_meta *)(const void *)(list+at);
   if(!cache_dent(e,(uint32_t)got-at)){cache_enabled=0;break;}
   const char *s=(const char *)(list+at+sizeof *e);
   char candidate[256];cache_copy(candidate,s,e->name_bytes);candidate[e->name_bytes]=0;
   if(collect_children)scan_collect(path,(const sbx_dirent *)(const void *)e,candidate,&e->size);
   if(!name[0]&&e->type==SBX_DT_FILE&&!gz_noise(candidate)&&ends_with_gam(s,e->name_bytes)){
    cache_copy(name,s,e->name_bytes);name[e->name_bytes]=0;cache_copy(key,&e->size,16);
    if(!collect_children){sbx_close(fd);return 1;}
   }
   at+=e->length;
  }
  if(!cache_enabled)break;
  heartbeat();
 }
 if(r)cache_enabled=0;
 sbx_close(fd);return cache_enabled; /* 1 + empty name is a completed no-game scan */
}
static int cache_pack_key(const struct sp_pack *p,uint8_t key[256]) {
 if(!cache_enabled)return 0;
 for(unsigned i=0;i<256;i++)key[i]=0;
 for(unsigned v=0;v<p->volumes;v++){
  char name[13];if(gmp_volume_name(p->base,v,name))return 0;
  const uint8_t *k=cache_file_key(p->folder==SP_FOLDER_POCKET?1:0,name);if(!k)return 0;
  cache_copy(key+16*v,k,16);
 }
 return 1;
}
static int cache_source_get(const char *path,const void *key,unsigned kn,struct cache_source *s) {
 uint32_t n=0;const uint8_t *p=cache_enabled?gc_get(&scan_cache,path,key,kn,&n):0;
 if(p&&n>=34){
  *s=(struct cache_source){0};cache_copy(s,p,12);unsigned gn=p[12];
  cache_copy(s->title,p+13,GM_TITLE_MAX+1);
  if(s->title[GM_TITLE_MAX]==0&&s->valid<=1&&s->record_bytes<=4096&&n==34+gn+s->record_bytes){
   cache_copy(s->gam,p+34,gn);
   if(length(s->gam)==gn){
    cache_copy(cache_value,s,sizeof *s);cache_copy(cache_value+sizeof *s,p+34+gn,s->record_bytes);cache_hits++;return 1;
   }
  }
 }
 cache_misses++;return 0;
}
static void cache_source_put(const char *path,const void *key,unsigned kn,const struct cache_source *s,const void *record) {
 if(!cache_enabled||!kn)return;
 unsigned gn=length(s->gam);if(gn>255)return;
 /* Compact wire value: crc/record length/valid, u8 GAM length, title[21],
  * GAM bytes, optional library record. Long-name capacity costs no warm I/O. */
 if(s->record_bytes)cache_copy(cache_wire+34+gn,record,s->record_bytes);
 cache_copy(cache_wire,s,12);cache_wire[12]=(uint8_t)gn;
 cache_copy(cache_wire+13,s->title,GM_TITLE_MAX+1);cache_copy(cache_wire+34,s->gam,gn);
 gc_add(&scan_cache,path,key,kn,cache_wire,34+gn+s->record_bytes);
}
static uint32_t cache_pending; /* bytes of the fresh cache still to persist: cache_flush writes them */
/* The scan is done: close the fresh cache in memory. Writing it (an fsync and a rename can block for half a second)
   waits for cache_flush, which GMOS calls once the games list is on screen. */
static void cache_end(void) {
 cache_pending=0;
 if(!cache_enabled)return;
 uint32_t n=gc_finish(&scan_cache);
 if(!n){say("GMOS cache capacity exceeded; live scan retained");return;}
 if(scan_cache.valid&&n==scan_cache.oldbytes&&cache_equal(cache_new,cache_old,n))return; /* no warm writes */
 cache_pending=n;
}
/* Persist the fresh cache: write temp file, fsync, rename (power-cut safe). Each step's time is logged once. */
static void cache_flush(void) {
 uint32_t n=cache_pending;cache_pending=0;if(!n)return;
 uint64_t t0=0,t1=0,fd,got,wmax=0,wsum=0;uint32_t chunks=0;
 sbx_clock_ns(&t0);
 int bad=sbx_open("/sd/OS/CACHE/GAMES.TMP",22,SBX_O_WRONLY|SBX_O_CREAT|SBX_O_TRUNC,&fd);
 sbx_clock_ns(&t1);uint64_t open_ms=(t1-t0)/1000000u;t0=t1;
 uint64_t fs_ms=0,rn_ms=0;
 if(!bad){
  for(uint32_t at=0;at<n;){uint32_t take=n-at<4096?n-at:4096; /* small chunks, a checkpoint after each */
   uint64_t a0=0,a1=0;sbx_clock_ns(&a0);
   if(sbx_write(fd,cache_new+at,take,&got)||got!=take){bad=1;break;}at+=take;
   sbx_clock_ns(&a1);a1-=a0;wsum+=a1;if(a1>wmax)wmax=a1;chunks++;heartbeat();}
  sbx_clock_ns(&t1);t0=t1;
  if(!bad)bad=sbx_fsync(fd);
  heartbeat(); /* first syscall after bounded storage work: report progress */
  sbx_clock_ns(&t1);fs_ms=(t1-t0)/1000000u;t0=t1;
  sbx_close(fd);
  if(!bad){bad=sbx_rename("/sd/OS/CACHE/GAMES.TMP",22,"/sd/OS/CACHE/GAMES.IDX",22);heartbeat();}
  sbx_clock_ns(&t1);rn_ms=(t1-t0)/1000000u;
 }
 {char l[160],*p=put(l,"GMOS cache write ");p=num(p,n);p=put(p," bytes: open ");p=num(p,(uint32_t)open_ms);p=put(p," ms, ");p=num(p,chunks);p=put(p," writes ");p=num(p,(uint32_t)(wsum/1000000u));
  p=put(p," ms (worst ");p=num(p,(uint32_t)(wmax/1000000u));p=put(p,"), fsync ");p=num(p,(uint32_t)fs_ms);p=put(p," ms, close+rename ");p=num(p,(uint32_t)rn_ms);put(p," ms");say(l);}
 if(bad)say("GMOS cache write unavailable; live scan retained");
}
