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
struct cache_source {uint32_t crc,record_bytes,valid;char gam[13],title[GM_TITLE_MAX+1];uint8_t reserved[2];};
_Static_assert(sizeof(struct cache_source)==48,"cache source wire layout");
static uint8_t cache_value[sizeof(struct cache_source)+4096];
static void cache_copy(void *out,const void *in,unsigned n) {uint8_t *a=out;const uint8_t *b=in;while(n--)*a++=*b++;}
static int cache_equal(const void *a,const void *b,unsigned n) {const uint8_t *p=a,*q=b;while(n--)if(*p++!=*q++)return 0;return 1;}
static int cache_same(const char *a,const char *b) {while(*a&&*a==*b){a++;b++;}return *a==*b;}
/* Validate the whole record before accessing name bytes. */
static int cache_dent(const sbx_dirent_meta *e,uint32_t left) {
 return left>=sizeof *e&&e->length>=sizeof *e&&e->length<=left&&!(e->length&7u)&&
        e->name_bytes<=e->length-sizeof *e&&!e->reserved;
}
static unsigned cache_inventory(const char *path,struct cache_file *out) {
 uint64_t fd,got;_Alignas(8) static uint8_t list[2048];unsigned count=0;
 int r=sbx_open(path,length(path),SBX_O_RDONLY,&fd);
 if(r){if(r!=-SBX_ENOENT)cache_enabled=0;return 0;}
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
static int cache_find_gam(const char *path,char name[13],uint8_t key[16]) {
 uint64_t fd,got;_Alignas(8) static uint8_t list[512];name[0]=0;
 if(!cache_enabled)return 0;
 int r=sbx_open(path,length(path),SBX_O_RDONLY,&fd);
 if(r){cache_enabled=0;return 0;}
 while(!(r=sbx_getdents_meta(fd,list,sizeof list,&got))&&got){
  for(uint32_t at=0;at<got;){
   const sbx_dirent_meta *e=(const sbx_dirent_meta *)(const void *)(list+at);
   if(!cache_dent(e,(uint32_t)got-at)){cache_enabled=0;break;}
   const char *s=(const char *)(list+at+sizeof *e);
   if(e->type==SBX_DT_FILE&&e->name_bytes<=12&&ends_with_gam(s,e->name_bytes)){
    cache_copy(name,s,e->name_bytes);name[e->name_bytes]=0;cache_copy(key,&e->size,16);sbx_close(fd);return 1;
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
 if(p&&n>=sizeof *s){
  cache_copy(s,p,sizeof *s);
  if(s->gam[12]==0&&s->title[GM_TITLE_MAX]==0&&s->valid<=1&&!s->reserved[0]&&!s->reserved[1]&&s->record_bytes<=4096&&n==sizeof *s+s->record_bytes){
   cache_copy(cache_value,p,n);cache_hits++;return 1;
  }
 }
 cache_misses++;return 0;
}
static void cache_source_put(const char *path,const void *key,unsigned kn,const struct cache_source *s,const void *record) {
 if(!cache_enabled||!kn)return;
 cache_copy(cache_value,s,sizeof *s);if(s->record_bytes)cache_copy(cache_value+sizeof *s,record,s->record_bytes);
 gc_add(&scan_cache,path,key,kn,cache_value,sizeof *s+s->record_bytes);
}
static void cache_end(void) {
 if(!cache_enabled)return;
 uint32_t n=gc_finish(&scan_cache);
 if(!n){say("GMOS cache capacity exceeded; live scan retained");return;}
 if(scan_cache.valid&&n==scan_cache.oldbytes&&cache_equal(cache_new,cache_old,n))return; /* no warm writes */
 uint64_t fd,got;int bad=sbx_open("/sd/OS/CACHE/GAMES.TMP",22,SBX_O_WRONLY|SBX_O_CREAT|SBX_O_TRUNC,&fd);
 if(!bad){
  for(uint32_t at=0;at<n;){uint32_t take=n-at<65536?n-at:65536;
   if(sbx_write(fd,cache_new+at,take,&got)||got!=take){bad=1;break;}at+=take;heartbeat();}
  if(!bad)bad=sbx_fsync(fd);
  heartbeat(); /* first syscall after bounded storage work: report progress */
  sbx_close(fd);
  if(!bad){bad=sbx_rename("/sd/OS/CACHE/GAMES.TMP",22,"/sd/OS/CACHE/GAMES.IDX",22);heartbeat();}
 }
 if(bad)say("GMOS cache write unavailable; live scan retained");
}
