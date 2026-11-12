/* SPDX-License-Identifier: MIT; GMOS.ELF, the first Game-Maker OS program.
 * Built only against the sandbox-kernel SDK (sdk/). It reports readiness,
 * lists the games in /sd/GAMES (each a folder holding a .GAM file, or a game
 * packed in one .GMP file there or in /sd/POCKET, sdpack.h and gmp.h; titled
 * from its .GAM and from the library index /sd/GAMES/LIBRARY.JSL when it
 * lists the game), draws the games page on /dev/fb0 (menu.h, view.h: tabs,
 * cards with title screens read from each game's files a card at a time, a
 * details view) and moves through it with /dev/input/event0. The page is
 * 960x540 at 16 bpp (ABI 1.2), which the GPU doubles to fill a 1080p screen.
 * Play on the details view plays the game (session.h) in 640x480x32 as
 * before, the mode every program starts in, at 18.2 Hz on the kernel's clock,
 * sleeping between ticks; Esc or the game's end returns to the list, and Esc
 * or Q on the grid quits. */
#include <sandbox/syscall.h>
#include "gam.h"
#include "input.h"
#include "hiscreen.h"
#include "entryloop.h"
#include "gmoslog.h"
#include "hisload.h"
#include "menu.h"
#include "sdpack.h"
#include "saveplay.h"
#include "sdzip.h"
#include "session.h"
#include "thumb.h"
#include "fli.h"
#include "asset_scratch.h"
#include "view.h"
#include "bootart.h"
#include "eink.h"
#include "audio.h"
#include "beat.h"
#include "font.h"
#include "ostune.h"
#include "awake.h"
#ifndef GMOS_SCAN_VERBOSE
#define GMOS_SCAN_VERBOSE 0
#endif
static uint64_t log_fd, fb_fd, in_fd;
static uint32_t generation, abi_minor;
static uint64_t beat;
static int have_input;
static struct gi_keys list_keys;
static struct gm_menu menu;
/* Menu labels remain DOS-sized; exact UTF-8 locations are separate. Kind 0
 * is a loose folder, 1 a GMP volume set, 2 an in-place ZIP (including SFX). */
static struct {uint8_t packed; struct sp_pack pack; char path[256],gam[256];uint32_t size;} where[GM_MAX_GAMES];
/* An Archive game keeps only what opening it needs: its .GAM name inside the ZIP, the ZIP's size and its place in
   arch_q (whose name gives the path). Games 0..GM_MAX_GAMES-1 are the main tabs'; the Archive's are the list's tail. */
#define ARCH_GAM 40u
static struct {char gam[ARCH_GAM];uint32_t size;uint16_t q;} where_a[GM_MAX_ARCHIVE];
_Static_assert(sizeof where_a + GM_MAX_ARCHIVE * (sizeof(struct gm_game) + sizeof(uint16_t) + 44u /* arch_q */) +
  (GM_CATS * GM_MAX_ALL - 8u * 256u) * sizeof(uint16_t) /* the tab lists grown for the Archive (was 8 x 256) */ < 120u * 1024u,
  "the 512 Archive entries, their tables and the wider tab lists must stay under 120 KiB (about 113 KiB measured)");
static unsigned arch_slot(unsigned i){return i-(menu.count-menu.archive_count);}
static int is_arch(unsigned i){return menu.games[i].cat==GM_CAT_ARCHIVE;}
static struct sp_pack packs[GM_MAX_GAMES];
static uint8_t gamfile[64 * 1024], index_text[256 * 1024];
static uint8_t pic_px[GP_W * GP_H], pic_pal[768], big_px[GP_W * GP_H], big_pal[768];
static int big_id = -1, big_ok;
static unsigned length(const char *s) {unsigned n = 0; while (s[n]) n++; return n;}
static char *put(char *p, const char *s) {while (*s) *p++ = *s++; *p = 0; return p;}
static char *num(char *p, uint32_t n) {char t[11]; int k = 0; do t[k++] = (char)('0' + n % 10); while (n /= 10); while (k) *p++ = t[--k]; *p = 0; return p;}
static char *num64(char *p, uint64_t n) {char t[21]; int k = 0; do t[k++] = (char)('0' + n % 10); while (n /= 10); while (k) *p++ = t[--k]; *p = 0; return p;}
static void say(const char *s) {sbx_log(log_fd, s, length(s));}
/* Serial logs are synchronous. Keep entry-by-entry diagnostics opt-in, not
 * proportional to the card's folder/duplicate count during normal startup. */
static unsigned scan_detail_count;
static void scan_say(const char *s) {scan_detail_count++; if (GMOS_SCAN_VERBOSE) say(s);}
static void scan_summary(void) {
  if (!GMOS_SCAN_VERBOSE && scan_detail_count) {
    char line[80], *p = put(line, "GMOS scan: "); p = num(p, scan_detail_count);
    put(p, " detail messages suppressed (SCAN_VERBOSE=1 to show)"); say(line);
  }
}
static int no_index; /* read_index found no LIBRARY.JSL */
static void (*beat_hook)(int); /* the loading splash's animation, set in main: called between files and games */
static void (*audio_hook)(void); /* feeds /dev/audio (set while a game plays): the heartbeat is called all through its slow steps */
static void bt_health(void *u) {(void)u; sbx_health(generation, ++beat);}
static void bt_audio(void *u) {(void)u; audio_hook();}
static void bt_hook(void *u) {(void)u; beat_hook(0);}
/* Health first (beat.h): a late heartbeat survives the kernel's 2 s rule; the pump's first call is a clock read. */
static void heartbeat(void) {
  struct bt_ops o = {0, bt_health, audio_hook ? bt_audio : 0, beat_hook ? bt_hook : 0};
  bt_beat(&o);
}
static int ends_with_gam(const char *s, unsigned n) {
  return n > 4 && s[n - 4] == '.' && (s[n - 3] == 'G'||s[n - 3]=='g') && (s[n - 2] == 'A'||s[n - 2]=='a') && (s[n - 1] == 'M'||s[n - 1]=='m');
}
static struct {char path[256];uint32_t size;uint8_t key[16];int keyed,directory;} zip_candidates[256];
static unsigned zip_count;
static int collect_children;
static void scan_collect(const char *,const sbx_dirent *,const char *,const void *);
#include "scan_store.h"
/* A single bounded deferred queue shares the first-level directory walk with
 * GAM selection. Inspect depth2 even when its parent is itself a game. */
static void scan_collect(const char *parent,const sbx_dirent *e,const char *name,const void *key) {
  char clean[256],path[256];
  if(gz_noise(name)||gz_path(name,clean,0)||!cache_same(name,clean)||!scan_join(path,parent,name))return;
  int directory=e->type==SBX_DT_DIR;
  if(!directory){
    unsigned n=length(name);char ext[5];if(n<=4)return;
    for(unsigned k=0;k<4;k++){char c=name[n-4+k];ext[k]=c>='a'&&c<='z'?(char)(c-32):c;}ext[4]=0;
    if(e->type!=SBX_DT_FILE||(!cache_same(ext,".ZIP")&&!cache_same(ext,".EXE")))return;
  }
  if(zip_count==256){scan_say("GMOS source candidates full; remaining nested folders/archives skipped");return;}
  put(zip_candidates[zip_count].path,path);zip_candidates[zip_count].size=e->size;
  zip_candidates[zip_count].keyed=key!=0;zip_candidates[zip_count].directory=directory;
  if(key)cache_copy(zip_candidates[zip_count].key,key,16);
  zip_count++;
}
/* The name of the first .GAM file in the folder `path` ("" if none). */
static void find_gam(const char *path, unsigned pn, char name[256]) {
  uint64_t fd, got; static uint8_t list[512];
  name[0] = 0;
  if (sbx_open(path, pn, SBX_O_RDONLY, &fd)) return;
  while ((!name[0]||collect_children) && !scan_list(fd, list, sizeof list, &got) && got) {
    for (uint32_t at = 0; at < got;) {
      const sbx_dirent *e = (const sbx_dirent *)(const void *)(list + at);
      if(!scan_dent(e,(uint32_t)got-at))break;
      const char *n = (const char *)(list + at + scan_head());
      char candidate[256];cache_copy(candidate,n,e->name_bytes);candidate[e->name_bytes]=0;
      if(collect_children)scan_collect(path,e,candidate,abi_minor>=4?(const void *)&e->size:0);
      if (!name[0]&&e->type == SBX_DT_FILE && !gz_noise(candidate) && ends_with_gam(n, e->name_bytes)) {
        for (unsigned i = 0; i < e->name_bytes; i++) name[i] = n[i];
        name[e->name_bytes] = 0;if(!collect_children)break;
      }
      if (e->length < sizeof *e || e->length > got - at) break; /* a broken entry: stop, not loop or read past */
      at += e->length;
    }
  }
  sbx_close(fd);
}
/* The first .GAM file in /sd/GAMES/<dir>, read far enough for its title. */
static void add_game(const char *dir, unsigned dn) {
  char path[256], folder[256], name[256];
  (void)dn;if(!scan_join(folder,"/sd/GAMES",dir))return;
  put(path,folder);
  uint8_t key[16];struct cache_source source={0};

  uint64_t fd, got;
  unsigned before=zip_count;int keyed=cache_find_gam(path,name,key);
  if(!keyed){zip_count=before;find_gam(path, length(path), name);}
  if (!name[0]) return; /* not a game folder */

  if(!scan_join(path,folder,name))return;
  if(!(keyed&&cache_source_get(path,key,sizeof key,&source)&&!source.record_bytes&&cache_same(source.gam,name))){
    source=(struct cache_source){0};
    if (sbx_open(path, length(path), SBX_O_RDONLY, &fd)) return;
    uint32_t have = 0;int status=0;
    while (have < sizeof gamfile && !(status=sbx_read(fd, gamfile + have, sizeof gamfile - have, &got)) && got) have += (uint32_t)got;
    sbx_close(fd);
    if(status)cache_enabled=0; /* don't persist a partial I/O result */
    put(source.gam,name);
    if(gm_gam_title(gamfile,have,source.title)>=0){source.valid=1;source.crc=gm_crc32(0,gamfile,have);}
  }
  if(keyed)cache_source_put(path,key,sizeof key,&source,0);
  if(!source.valid)return;
  char d[9]; unsigned k = 0; while (k < dn && k < 8) {d[k] = dir[k]; k++;} d[k] = 0;
  uint32_t crc = source.crc;
  for (unsigned i = 0; i < menu.count; i++)
    if (menu.games[i].crc == crc) { /* listed already: packed (the pack is played; a card changing over holds both) or another folder */
      char line[80], *q = put(line, "GMOS skipped folder "); q = put(q, d); q = put(q, where[i].packed ? ": packed as " : ": the same game as "); put(q, menu.games[i].dir); scan_say(line); return;
    }
  if(gm_add(&menu, dir, name, source.title, crc)){put(where[menu.count-1].path,folder);put(where[menu.count-1].gam,name);}
}
/* /sd/GAMES/LIBRARY.JSL (or /sd/OS/LIBRARY.JSL, where the kernel's USB PUTF can
   install it), a line at a time: the titles, categories and blurbs it gives. */
static void read_index(void) {
  uint64_t fd, got; uint32_t have = 0, matched = 0;
  const char *path="/sd/GAMES/LIBRARY.JSL";const uint8_t *key=cache_file_key(0,"LIBRARY.JSL");
  if(!key){path="/sd/OS/LIBRARY.JSL";key=cache_file_key(2,"LIBRARY.JSL");}
  const uint8_t *cached=key?gc_get(&scan_cache,path,key,16,&have):0;
  if(cached&&have<=sizeof index_text)cache_copy(index_text,cached,have);
  else {
    have=0;path="/sd/GAMES/LIBRARY.JSL";
    if(sbx_open(path,21,SBX_O_RDONLY,&fd)){
      path="/sd/OS/LIBRARY.JSL";
      if(sbx_open(path,18,SBX_O_RDONLY,&fd)){no_index=1;say("GMOS no library index; titles from the .GAM files");return;}
    }
    int status=0;
    while (have < sizeof index_text && !(status=sbx_read(fd, index_text + have, sizeof index_text - have, &got)) && got) have += (uint32_t)got;
    sbx_close(fd);if(status)cache_enabled=0;
    key=cache_file_key(path[4]=='G'?0:2,"LIBRARY.JSL");
  }
  if(key&&cache_enabled)gc_add(&scan_cache,path,key,16,index_text,have);
  for (uint32_t at = 0; at < have;) {
    uint32_t end = at; while (end < have && index_text[end] != '\n') end++;
    matched += (uint32_t)gm_library_line(&menu, (const char *)index_text + at, end - at);
    at = end + 1;
    heartbeat();
  }
  char line[64], *p = put(line, "GMOS library index: "); p = num(p, matched); put(p, matched == 1 ? " game" : " games"); say(line);
}
/* A packed game: its .GAM from the pack, titled from it. A game already packed (the same .GAM) is not listed again. */
static void add_pack(const struct sp_pack *pk) {
  char path[40], line[96], *p;
  struct cache_source source={0};uint8_t key[256];
  sp_path(pk, 0, path);
  int keyed=cache_pack_key(pk,key),reused=keyed&&cache_source_get(path,key,sizeof key,&source);
  struct gmp *g=0;
  if(!reused){
    uint32_t errors=sp_io_errors;
    source=(struct cache_source){0};g=sp_open(pk);
    int32_t have=g?gmp_read(g,g->gam,gamfile,sizeof gamfile):-1;
    if(have<0||gm_gam_title(gamfile,(uint32_t)have,source.title)<0){
      if(sp_io_errors!=errors)cache_enabled=0;
      if(keyed)cache_source_put(path,key,sizeof key,&source,0);
      p=put(line,"GMOS skipped ");p=put(p,path);put(p,": not a game pack");scan_say(line);return;
    }
    source.valid=1;
    source.crc=gm_crc32(0,gamfile,(uint32_t)have);
    for(unsigned i=0;i<12&&g->gam[i];i++)source.gam[i]=g->gam[i];
    if(keyed){int32_t rec=gmp_entry(g,"library.json",index_text,4096);if(rec>0)source.record_bytes=(uint32_t)rec;}
    if(sp_io_errors!=errors)cache_enabled=0; /* retry transient library read failures next startup */
  }
  if(keyed)cache_source_put(path,key,sizeof key,&source,reused?cache_value+sizeof source:index_text);
  if(!source.valid){p=put(line,"GMOS skipped ");p=put(p,path);put(p,": not a game pack");scan_say(line);return;}
  uint32_t crc=source.crc;
  for (unsigned i = 0; i < menu.count; i++)
    if (menu.games[i].crc == crc) {p = put(line, "GMOS skipped "); p = put(p, path); p = put(p, ": the same game as "); put(p, menu.games[i].dir); scan_say(line); return;}
  if (!gm_add(&menu, pk->base, source.gam, source.title, crc)) return;
  where[menu.count - 1].packed = 1; where[menu.count - 1].pack = *pk;
  put(where[menu.count-1].gam,source.gam);put(where[menu.count-1].path,path);
  if(keyed){if(source.record_bytes)gm_library_line(&menu,(const char *)(reused?cache_value+sizeof source:index_text),source.record_bytes);}
  else {int32_t rec=gmp_entry(g,"library.json",index_text,4096);if(rec>0)gm_library_line(&menu,(const char *)index_text,(uint32_t)rec);}
  p = put(line, "GMOS packed "); p = put(p, pk->base); p = put(p, ": "); p = put(p, path); p = put(p, ", ");
  p = num(p, pk->volumes); put(p, pk->volumes == 1 ? " volume" : " volumes"); scan_say(line);
}
static void find_packs(uint8_t folder) {
  uint32_t errors=sp_io_errors;
  unsigned n = sp_list(folder, packs, GM_MAX_GAMES, scan_say);
  if(errors!=sp_io_errors)cache_enabled=0;
  for (unsigned i = 0; i < n; i++) {add_pack(&packs[i]); heartbeat();}
}
/* Live root enumeration; nested discovery is fused into the GAM scan above. */
static void scan_tree(const char *relative,unsigned depth) {
  char path[256];uint64_t fd,got;_Alignas(8) uint8_t list[2048];
  if(*relative){if(!scan_join(path,"/sd/GAMES",relative))return;}else put(path,"/sd/GAMES");
  if(sbx_open(path,length(path),SBX_O_RDONLY,&fd))return;
  int status;
  while(!(status=scan_list(fd,list,sizeof list,&got))&&got){
    for(uint32_t at=0;at<got;){
      const sbx_dirent *e=(const sbx_dirent *)(const void *)(list+at);
      if(!scan_dent(e,(uint32_t)got-at)){cache_enabled=0;break;}
      char name[256],child[256];cache_copy(name,list+at+scan_head(),e->name_bytes);name[e->name_bytes]=0;
      if(!gz_noise(name)&&!gz_path(name,child,0)&&cache_same(name,child)){
        if(e->type==SBX_DT_DIR&&depth<2){
          int joined=1;if(*relative)joined=scan_join(child,relative,name);else put(child,name);
          if(joined){collect_children=depth==0;add_game(child,length(child));collect_children=0;}
        } else if(e->type==SBX_DT_FILE)scan_collect(path,e,name,abi_minor>=4?(const void *)&e->size:0);
      }
      at+=e->length;heartbeat();
    }
  }
  if(status)cache_enabled=0;
  sbx_close(fd);
}
/* One ZIP/SFX as a game: a main-tab game, or (archive) a card of the Archive tab, whose own room never crowds the others. */
static int add_zip_at(const char *path,uint32_t size,const uint8_t *key,int keyed,unsigned archive) { /* archive: 0 for a main-tab game, else the arch_q number + 1 */
  struct cache_source source={0};
  if(!(keyed&&cache_source_get(path,key,16,&source))){
    uint32_t errors=sz_io_errors;struct gz_zip *z=sz_open(path,size,abi_minor);
    int32_t have=z?gz_read(z,z->gam,gamfile,sizeof gamfile):-1;
    if(have>=0&&gm_gam_title(gamfile,(uint32_t)have,source.title)>=0){source.valid=1;source.crc=gm_crc32(0,gamfile,(uint32_t)have);put(source.gam,z->gam);}
    sz_close();if(errors!=sz_io_errors)cache_enabled=0;
  }
  if(keyed)cache_source_put(path,key,16,&source,0);
  if(!source.valid||(archive&&length(source.gam)>=ARCH_GAM)){scan_say("GMOS skipped archive: unsupported, damaged, or no game");return 0;}
  for(unsigned i=0;i<menu.count;i++)if(menu.games[i].crc==source.crc)return 0;
  const char *base=path;for(const char *p=path;*p;p++)if(*p=='/')base=p+1;
  if(archive?gm_add_archive(&menu,base,source.gam,source.title,source.crc):gm_add(&menu,base,source.gam,source.title,source.crc)){
    unsigned i=menu.count-1;
    if(archive){unsigned a=arch_slot(i);where_a[a].size=size;where_a[a].q=(uint16_t)(archive-1);put(where_a[a].gam,source.gam);}
    else{where[i].packed=2;where[i].size=size;put(where[i].path,path);put(where[i].gam,source.gam);}
    return 1;
  }
  return 0;
}
static void add_zip(unsigned id) {add_zip_at(zip_candidates[id].path,zip_candidates[id].size,zip_candidates[id].key,zip_candidates[id].keyed,0);}
/* /sd/ARCHIVE: the old user-made games (hundreds of ZIPs). The scan only lists the names, quickly; the
   list is up first and the Archive tab fills one ZIP at a time while the keys are idle (arch_step). */
#define ARCH_NAME 40u
static struct {char name[ARCH_NAME];uint32_t size;} arch_q[GM_MAX_ARCHIVE];
static unsigned arch_n,arch_next;
static const char *w_path(unsigned i){
  static char buf[64];if(!is_arch(i))return where[i].path;
  char *p=put(buf,"/sd/ARCHIVE/");put(p,arch_q[where_a[arch_slot(i)].q].name);return buf;}
static const char *w_gam(unsigned i){return is_arch(i)?where_a[arch_slot(i)].gam:where[i].gam;}
static void arch_list(void) {
  uint64_t fd,got;_Alignas(8) uint8_t list[2048];
  arch_n=arch_next=0;
  if(sbx_open("/sd/ARCHIVE",11,SBX_O_RDONLY,&fd))return;
  int status;
  while(!(status=scan_list(fd,list,sizeof list,&got))&&got){
    for(uint32_t at=0;at<got;){
      const sbx_dirent *e=(const sbx_dirent *)(const void *)(list+at);
      if(!scan_dent(e,(uint32_t)got-at))break;
      char name[256],clean[256];cache_copy(name,list+at+scan_head(),e->name_bytes);name[e->name_bytes]=0;
      unsigned n=length(name);char ext[5]={0};
      if(n>4)for(unsigned k=0;k<4;k++){char c=name[n-4+k];ext[k]=c>='a'&&c<='z'?(char)(c-32):c;}
      if(e->type==SBX_DT_FILE&&n>4&&n<ARCH_NAME&&!gz_noise(name)&&!gz_path(name,clean,0)&&cache_same(name,clean)&&(cache_same(ext,".ZIP")||cache_same(ext,".EXE"))){
        if(arch_n<GM_MAX_ARCHIVE){put(arch_q[arch_n].name,name);arch_q[arch_n].size=e->size;arch_n++;}
        else {scan_say("GMOS archive full; remaining files skipped");}
      }
      at+=e->length;heartbeat();
    }
  }
  sbx_close(fd);
  char line[64],*p=put(line,"GMOS archive: ");p=num(p,arch_n);put(p,arch_n==1?" file":" files");scan_say(line);
}
/* One archive file (one ZIP opened, read and closed): 1 when a game joined the tab. Never more than one per call. */
static int arch_step(void) {
  if(arch_next>=arch_n)return 0;
  char path[64],*p=put(path,"/sd/ARCHIVE/");unsigned q=arch_next++;put(p,arch_q[q].name);
  return add_zip_at(path,arch_q[q].size,0,0,q+1);
}
static void find_games(void) {
  scan_detail_count = 0;zip_count=0;
  cache_begin();sp_beat = heartbeat;sz_beat=heartbeat;
  find_packs(SP_FOLDER_POCKET); /* precedence: GMP, loose folder, ZIP/SFX */
  find_packs(SP_FOLDER_GAMES);
  scan_tree("",0);
  for(unsigned i=0;i<zip_count;i++)if(zip_candidates[i].directory){const char *p=zip_candidates[i].path+10;add_game(p,length(p));heartbeat();}
  for(unsigned i=0;i<zip_count;i++)if(!zip_candidates[i].directory){add_zip(i);heartbeat();}
  arch_list();
  scan_summary();
  heartbeat(); read_index(); heartbeat(); /* a checkpoint either side of each slow step */
  gm_finish(&menu);
  /* Use the complete scan path, including pack/ZIP names, for library identity. */
  for (unsigned i=0;i<menu.count;i++) if(!is_arch(i)) gm_game_key(&menu.games[i],where[i].path,where[i].gam);
  gm_refresh(&menu); heartbeat();
  cache_end();
}
static void open_input(void) {
  sbx_input_info info; sbx_input_state st;
  if (sbx_open("/dev/input/event0", 17, SBX_O_RDONLY | SBX_O_NONBLOCK, &in_fd)) return;
  have_input = 1;
  gi_reset(&list_keys); menu.shift = 0;
  char line[96], *p = put(line, "GMOS input event0");
  if (!sbx_input_query(in_fd, &info)) {
    p = put(p, info.flags & SBX_INPUT_SYNTHETIC ? " synthetic" : ""); p = put(p, ", queue "); p = num(p, (uint32_t)info.queue_records);
  }
  say(line);
  if (sbx_input_state_get(in_fd, &st)) {sbx_close(in_fd); have_input = 0;}
  /* Start from an empty queue; keys held across attachment are not presses. */
}
static uint16_t *splash_px; static uint64_t splash_t0, splash_last, splash_call; static uint32_t splash_gap_ms; /* the loading bars (bootart.h) */
/* Called between files and games (the heartbeat): the clock picks the highlight's place, so uneven calls skip shades
   instead of slowing it. At most one redraw every 50 ms; force is the last step before the list. */
static void splash_tick(int force) {
  ge_poll();
  uint64_t now = 0; if (!splash_px) return;
  sbx_clock_ns(&now);
  uint32_t gap = (uint32_t)((now - splash_call) / 1000000u); if (gap > splash_gap_ms) splash_gap_ms = gap; /* checkpoint spacing */
  splash_call = now;
  if (!force && now - splash_last < 50000000ull) return;
  splash_last = now; ba_bar(splash_px, GM_W, (uint32_t)((now - splash_t0) / 1000000u), BA_SEGS);
  sbx_fb_present(fb_fd, BA_BAR_X, BA_BAR_Y, BA_BAR_W, BA_BAR_H);
}
/* game.h's folder reader: preserve spelling and reject escaping/oversized paths. */
static int32_t sd_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {
  char path[256],clean[256];
  if(gz_path(name,clean,0)||gz_noise(clean)||!scan_join(path,(const char *)user,clean))return -1;
  uint64_t fd, got; uint32_t have = 0;
  heartbeat();
  if (sbx_open(path, length(path), SBX_O_RDONLY, &fd)) return -1;
  int status=0;
  while (have < cap) {
    uint32_t take=cap-have<16384u?cap-have:16384u;
    heartbeat();
    status=sbx_read(fd,buf+have,take,&got);
    if(status||!got)break;
    if (got > take) {status = -1; break;}
    have+=(uint32_t)got;
    heartbeat();
  }
  sbx_close(fd);
  heartbeat();
  return status?-1:(int32_t)have;
}
/* game.h's reader for the game g, and its user pointer: the folder's files, or the pack's (opened). */
static gp_read_fn reader(const struct gm_game *g, void **user) {
  unsigned i = (unsigned)(g - menu.games);
  sz_close();
  if (is_arch(i)) {*user=sz_open(w_path(i),where_a[arch_slot(i)].size,abi_minor);return *user?gz_read:0;}
  if (!where[i].packed) {*user = (void *)where[i].path; return sd_read;}
  if(where[i].packed==2){*user=sz_open(where[i].path,where[i].size,abi_minor);return *user?gz_read:0;}
  *user = sp_open(&where[i].pack);
  return *user ? gmp_read : 0;
}
static uint64_t clock_ns(void) {uint64_t t = 0; sbx_clock_ns(&t); return t;}
static uint32_t crc32(const void *data, uint32_t bytes) {
  const uint8_t *b = (const uint8_t *)data;
  static uint32_t table[256];
  if (!table[1]) for (uint32_t i = 0; i < 256; i++) {uint32_t c = i; for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xedb88320u & (0u - (c & 1))); table[i] = c;}
  uint32_t c = ~0u;
  for (uint32_t i = 0; i < bytes; i++) c = table[(c ^ b[i]) & 0xff] ^ c >> 8;
  return ~c;
}
/* ABI 1.8: /dev/audio, kept open and retried on opt-in after failure (audio.h). The library plays background tunes only when enabled by its owner
   (bg_*), a game's or a preview's sound replaces them, and every switch fades and keys all channels off. */
static struct gs_session game;
static int demo_on, demo_none = -1; /* the preview (below) */
static struct au aud; static uint64_t audio_fd; static int aud_on, aud_busy;
static uint8_t bgm_buf[8192]; /* the background tune's file: a game's play and preview use pic_px (64 KB) while they run */
static const char *doing = "start"; /* what the main loop is busy with, for the underrun log */
static uint64_t last_under, audio_t_open, au_prev_ns, au_prev_t;
/* Retain the exact kernel status even after the failed handle is closed. */
static int32_t audio_error;
static const char *audio_error_op = "open";
static uint64_t audio_note_until;
static int audio_note_dirty;
static void audio_error_log(void) {
  char line[112], *p = put(line, "GMOS audio ");
  p = put(p, audio_error_op); p = put(p, ": error ");
  if (audio_error < 0) p = put(p, "-");
  num(p, audio_error < 0 ? 0u - (uint32_t)audio_error : (uint32_t)audio_error); say(line);
}
static void audio_failure(const char *op, int32_t rc) {
  audio_error_op = op; audio_error = rc; audio_error_log();
  audio_note_until = clock_ns() + 2500000000ull; audio_note_dirty = 1;
}
/* A short overlay also works on idle game menus and an empty library. */
static int audio_note_tick(void) {
  if (audio_note_until && clock_ns() >= audio_note_until) {audio_note_until = 0; audio_note_dirty = 1;}
  int dirty = audio_note_dirty; audio_note_dirty = 0; return dirty;
}
static void audio_note_draw(void *pixels, unsigned pitch, int rgb32) {
  if (!audio_note_until) return;
  static const char note[] = "No sound device";
  for (unsigned y = 8; y < 34; y++) for (unsigned x = 8; x < 204; x++) {
    unsigned col = (x - 16) / 12, row = (y - 12) / 2, bit = (x - 16) % 12 / 2;
    int ink = x >= 16 && y >= 12 && col < sizeof note - 1 && row < GM_FONT_H && bit < GM_FONT_W &&
              (gm_font[(unsigned)note[col] - 32][row] & (1u << (4 - bit)));
    if (rgb32) ((uint32_t *)pixels)[y * pitch + x] = ink ? 0xffffffu : 0x182030u;
    else ((uint16_t *)pixels)[y * pitch + x] = ink ? 0xffffu : 0x1906u;
  }
}
static int32_t au_query(void *u, uint32_t *queued, uint32_t *cap) {
  (void)u; sbx_audio_info i; int32_t rc = sbx_audio_query(audio_fd, &i);
  if (!rc) {
    if (i.rate != SBX_AUDIO_RATE || !i.capacity_frames || i.capacity_frames > UINT32_MAX || i.queued_frames > i.capacity_frames) {
      audio_failure("query format", -SBX_EINVAL); return -SBX_EINVAL;
    }
    *queued = (uint32_t)i.queued_frames; *cap = (uint32_t)i.capacity_frames;
    if (i.underruns > last_under && au_quiet(&aud)) last_under = i.underruns; /* a shallow, silent ring running dry is expected (audio.h AU_QUIET_*) */
    if (i.underruns > last_under) { /* a stall: the ring ran dry (silence, never a repeat) */
      char line[120], *p = put(line, "GMOS audio underrun +"); p = num(p, (uint32_t)(i.underruns - last_under)); p = put(p, " at ");
      uint64_t t = 0; sbx_clock_ns(&t); p = num(p, (uint32_t)((t - audio_t_open) / 1000000u)); p = put(p, " ms during: "); put(p, doing); say(line);
      last_under = i.underruns;
    }
  }
  if (rc) audio_failure("query", rc);
  return rc;
}
static int32_t au_write(void *u, const void *pcm, uint32_t bytes) {
  (void)u; uint64_t done = 0; int32_t rc = sbx_write(audio_fd, pcm, bytes, &done);
  if (rc && rc != -SBX_EAGAIN) audio_failure("write", rc);
  if (!rc && (done > bytes || done % 4)) {audio_failure("write count", -SBX_EIO); return -SBX_EIO;}
  return rc ? rc : (int32_t)done;
}
static uint64_t au_now(void *u) {(void)u; uint64_t t = 0; sbx_clock_ns(&t); return t;}
static const struct au_sink au_sink = {0, au_query, au_write, au_now};
static void audio_disable(void) {
  aud_on = 0; audio_hook = 0;
  au_silence(&aud); sbx_close(audio_fd); audio_fd = 0;
}
static void audio_poll(void) {
  if (aud_on && !aud_busy) {
    /* Leave the newly opened device unstarted while the library is silent.
       Once started, keep feeding silence between sounds: the kernel counts every short drain. */
    if (!aud.frames && !aud.cmf.playing && !aud.pc.active && aud.vp.state != VP_PLAYING && !aud.ui.active) return;
    aud_busy = 1;
    if (au_pump(&aud, &au_sink) < 0) audio_disable();
    aud_busy = 0;
  }
}
/* File reads may yield back into audio_poll; only the pump owns aud_busy. */
static void audio_service(gp_read_fn read, void *user) {
  audio_poll();
  uint64_t start = clock_ns();
  au_service(&aud, read, user);
  uint64_t elapsed = clock_ns() - start;
  if (elapsed > aud.svc_max_ns) aud.svc_max_ns = elapsed;
  audio_poll();
}
/* A game's files are inflated in one call (puff, then the CRC): on the board that can run past the kernel's 2 s with no system call, so
 * zip.c's checkpoint reports health and feeds the audio while a game plays (set around play(), never in the scan). */
static void game_yield(void) {sbx_health(generation, ++beat); audio_poll();}
/* A clip of the playing game for vocplay.h: a folder's file is streamed (an open fd, 384-byte reads); a pack's or zip's is read whole
 * into index_text (the library's scan buffer, idle while a game plays; a clip that does not fit is refused). */
static uint64_t voc_fd, voc_pos; static uint32_t voc_blob_n;
static int32_t voc_pread(void *u, uint32_t off, void *buf, uint32_t n) {
  (void)u; uint64_t got = 0;
  if (voc_fd) {
    if (voc_pos != off && (sbx_seek(voc_fd, (int64_t)off, &voc_pos) || voc_pos != off)) return -1;
    if (sbx_read(voc_fd, buf, n, &got)) return -1;
    voc_pos += got; return (int32_t)got;
  }
  if (off >= voc_blob_n) return 0;
  if (n > voc_blob_n - off) n = voc_blob_n - off;
  for (uint32_t k = 0; k < n; k++) ((uint8_t *)buf)[k] = index_text[off + k];
  return (int32_t)n;
}
static void voc_close(void *u) {(void)u; if (voc_fd) sbx_close(voc_fd); voc_fd = 0; voc_pos = 0; voc_blob_n = 0;}
static int voc_open(const char *name, struct vp_io *io) {
  voc_close(0);
  io->user = 0; io->pread = voc_pread; io->close = voc_close; io->size = 0;
  if (game.read == sd_read) {
    char path[256], clean[256];
    if (gz_path(name, clean, 0) || gz_noise(clean) || !scan_join(path, (const char *)game.user, clean)) return -1;
    if (sbx_open(path, length(path), SBX_O_RDONLY, &voc_fd)) {voc_fd = 0; return -1;}
    uint64_t lo = 0, hi = 1u << 26, pos; /* the length: the largest offset a seek accepts */
    while (lo < hi) {uint64_t mid = (lo + hi + 1) / 2; if (!sbx_seek(voc_fd, (int64_t)mid, &pos)) lo = mid; else hi = mid - 1;}
    io->size = (uint32_t)lo; voc_pos = (uint64_t)-1;
    if (!io->size) {voc_close(0); return -1;}
    return 0;
  }
  if (!game.read) return -1;
  int32_t n = game.read(game.user, name, index_text, sizeof index_text);
  if (n <= 0 || (uint32_t)n >= sizeof index_text) return -1;
  voc_blob_n = io->size = (uint32_t)n;
  return 0;
}
static void au_event(void *u, int kind, const char *name, int n) {
  (void)u;
  if (kind == SND_EV_MUSIC) au_music(&aud, name); else if (kind == SND_EV_BEEP) au_beep(&aud, name, n); else if (kind == SND_EV_VOC) au_voc(&aud, name); else au_stop(&aud);
}
static void audio_wait_poll(void *user) {(void)user; sbx_health(generation, ++beat); audio_poll();} /* the kernel's heartbeat, every round of a wait */
static void audio_wait_sleep(void *user, uint64_t wake) {(void)user; sbx_sleep_until(wake);}
static void audio_sleep_until(uint64_t wake) {
  if (aud_on) au_wait_until(&au_sink, wake, audio_wait_poll, audio_wait_sleep);
  else {uint64_t cap = clock_ns() + 100000000ull; sbx_sleep_until(wake < cap ? wake : cap);} /* never past 100 ms: the heartbeat is due */
}
static void audio_log(const char *what) { /* the kernel's counters, and what the synth cost since the last line */
  sbx_audio_info i; char line[512], *p = put(line, "GMOS audio "); p = put(p, what);
  uint64_t t = clock_ns();
  if (!aud_on) return;
  int32_t rc = sbx_audio_query(audio_fd, &i);
  if (rc) {audio_failure("query counters", rc); return;}
  p = put(p, ": rendered "); p = num(p, (uint32_t)aud.frames); p = put(p, " frames, queued "); p = num(p, (uint32_t)i.queued_frames);
  p = put(p, ", underruns "); p = num(p, (uint32_t)i.underruns); p = put(p, ", overruns "); p = num(p, (uint32_t)i.overruns);
  p = put(p, ", eagain "); p = num(p, aud.eagain); p = put(p, ", songs "); p = num(p, aud.music_loads);
  p = put(p, ", beeps "); p = num(p, aud.beeps); p = put(p, ", vocs "); p = num(p, aud.vocs); p = put(p, "/"); p = num(p, aud.voc_refused); p = put(p, ", synth CPU "); p = num(p, t > au_prev_t ? (uint32_t)((aud.total_ns - au_prev_ns) * 1000u / (t - au_prev_t)) : 0);
  p = put(p, " per mille, longest pump gap "); p = num(p, (uint32_t)(aud.gap_max_ns / 1000u)); p = put(p, " us, longest sound read "); p = num(p, (uint32_t)(aud.svc_max_ns / 1000u));
  p = put(p, " us, low water "); p = num(p, aud.low_frames);
  put(p, " frames; since the last line"); say(line);
  if (what[0] == 'g') { /* the card's copy is cut at 160 bytes, so it carries the fields that explain a silent game first: the mute flag, the song's state and the volume */
    char brief[192], *b = put(brief, "GMOS audio game: mute "); b = num(b, aud.music_muted); b = put(b, " song "); b = num(b, aud.cmf.playing);
    b = put(b, " vol "); b = num(b, aud.vol >> 8); b = put(b, "/"); b = num(b, aud.vol_target >> 8); b = put(b, " songs "); b = num(b, aud.music_loads); b = put(b, " vocs "); b = num(b, aud.vocs);
    b = put(b, " peak "); b = num(b, aud.peak); b = put(b, "/"); b = num(b, aud.music_peak); b = put(b, " q "); b = num(b, (uint32_t)i.queued_frames); b = put(b, " under "); b = num(b, (uint32_t)i.underruns); b = put(b, " gap "); b = num(b, (uint32_t)(aud.gap_max_ns / 1000u));
    b = put(b, " read "); b = num(b, (uint32_t)(aud.svc_max_ns / 1000u)); b = put(b, " low "); b = num(b, aud.low_frames); put(b, " us"); gl_add(brief, t);
  }
  aud.peak = aud.music_peak = 0;
  aud.gap_max_ns = aud.svc_max_ns = 0;
  au_prev_ns = aud.total_ns; au_prev_t = t;
}
static void audio_open(void) {
  static int initialized;
  if (aud_on) return;
  if (!initialized) {
    au_init(&aud, bgm_buf, sizeof bgm_buf); aud.voc_open = voc_open;
    game.sound.event = au_event; game.sound.event_user = 0; initialized = 1;
    gp_decode_hook(audio_poll);
  }
  if (abi_minor < 8) {audio_failure("requires ABI 1.8", -SBX_ENOSYS); return;}
  int32_t rc = sbx_open("/dev/audio", 10, SBX_O_WRONLY, &audio_fd);
  if (rc) {audio_failure("open /dev/audio", rc); return;}
  uint32_t queued, cap;
  last_under = 0; audio_t_open = au_prev_t = clock_ns();
  if (au_query(0, &queued, &cap)) {
    sbx_close(audio_fd); audio_fd = 0; return;
  }
  aud_on = 1; audio_hook = audio_poll; aud.vol_target = AU_VOL_ONE;
  audio_error = 0; audio_note_until = 0; audio_note_dirty = 1;
  /* A fatal failure silenced the synth; resume the latest requested song. */
  au_music(&aud, aud.want_music);
  char line[80], *p = put(line, "GMOS audio open, ring ");
  p = num(p, cap); put(p, " frames at 48000 Hz"); say(line);
  say("GMOS audio: HDMI sink status requires kernel AUDIO diagnostics");
}
/* Every actual toggle is observable, even if the optional device is closed. */
static void music_feedback(int enabled) {
  say(enabled ? "GMOS music on" : "GMOS music off");
  if (aud_on) {
    /* Probe even an unstarted/silent device so M exposes a failed handle now. */
    uint32_t queued, cap;
    if (au_query(0, &queued, &cap)) audio_disable();
    return;
  }
  if (enabled) audio_open();
  else audio_failure(audio_error_op, audio_error);
}
/* Reserve M before game menus, card-skip and save handlers. Ignore repeats
 * and releases; typed M in name entry remains owned by that text field. */
static int play_music_event(const sbx_input_event *event) {
  if (event->type != SBX_EV_KEY || event->code != 50) return 0;
  if (event->value == 1) {
    aud.music_muted = !aud.music_muted;
    gl_add(aud.music_muted ? "GMOS music off (M)" : "GMOS music on (M)", clock_ns());
    music_feedback(!aud.music_muted);
  }
  return 1;
}
/* The background tune ends its place to a game or a preview: fade it out (at most a second), then everything stops. */
static void bg_leave(void);
/* The game's or the preview's sound owns the player (music in the 64 KB scratch), until bg_return. */
static void audio_take(void) {
  bg_leave(); aud.asleep = 0; aud.want_music[0] = 0; aud.music_muted = 0; aud.vol = aud.vol_target = AU_VOL_ONE; aud.music = pic_px; aud.music_cap = sizeof pic_px;
}
/* Back to the list: hard stop (every channel keyed off, chip reset), the tune buffer back, the cycle restarts after a pause. */
static int bg_state; static uint64_t bg_t0, bg_next, dark_since; static uint32_t bg_fail, bg_loops0, bg_picks, bg_seed = 12345u;
static void bg_return(const char *why) {
  aud.music_muted = 0; aud.want_music[0] = 0;
  au_set_playing(&aud, 0); au_silence(&aud); aud.music = bgm_buf; aud.music_cap = sizeof bgm_buf; bg_state = 0; bg_next = clock_ns() + 800000000ull; doing = "list";
  audio_log(why);
  if (abi_minor >= 7) gl_flush(&save_sbx_ops, clock_ns(), 1); /* the game is over: the list has no frame to hitch */
}
static void bg_leave(void) {
  if (aud_on && bg_state && aud.cmf.playing) {
    au_volume(&aud, 0);
    for (uint64_t end = clock_ns() + 1100000000ull; aud.vol && clock_ns() < end;) {heartbeat(); audio_sleep_until(clock_ns() + 10000000ull);}
  }
  au_silence(&aud); bg_state = 0;
}
/* The cycle: the OS's own tune every third pick, else a random game's first level song (its menu song if the level has none).
   One try a call, so the list stays responsive; the library index holds no secret game. */
/* Awake again (the screen lit, or a key arrived while dark): menu sounds work at once, and the cycle's clocks stand still for the dark time. */
static void bg_undark(void) {
  au_sleep(&aud, 0);
  if (dark_since) {uint64_t now = clock_ns(); bg_t0 = au_shift_dark(bg_t0, dark_since, now); bg_next = au_shift_dark(bg_next, dark_since, now); dark_since = 0;}
}
static void bg_tick(void) {
  if (!aud_on || demo_on || !menu.background_music) return;
  uint64_t now = clock_ns();
  if (bg_state == 1) {
    if ((aud.cmf.loops > bg_loops0 && now - bg_t0 > 20000000000ull) || now - bg_t0 > 90000000000ull) {au_music(&aud, ""); bg_state = 2;}
  } else if (bg_state == 2) {
    if (!aud.cmf.playing && !aud.music_pending) {bg_state = 0; bg_next = now + 1500000000ull;}
  } else if (now >= bg_next && menu.count + 1) {
    doing = "bgm pick";
    bg_seed = bg_seed * 1103515245u + 12345u;
    const char *name = 0; int32_t n = -1; unsigned idx = menu.count ? (bg_seed >> 8) % menu.count : 0;
    if (bg_picks % 3 && menu.count) {
      const struct gm_game *g = &menu.games[idx]; void *user; gp_read_fn read = reader(g, &user); const char *gam = w_gam(idx);
      if (read && gam[0] && gp_open(read, user, gam) >= 0) {
        name = gp_song(GP_SONG_LEVEL, 0); if (!name[0]) name = gp_song(GP_SONG_TITLE, 0);
        if (name[0]) n = read(user, name, bgm_buf, sizeof bgm_buf);
      }
      if (n <= 0 || (uint32_t)n >= sizeof bgm_buf || au_play_loaded(&aud, (uint32_t)n)) n = -1; /* too big, unreadable or refused: try again soon */
      if (n < 0) {if (++bg_fail >= 6) {bg_fail = 0; bg_picks = (bg_picks + 2) / 3 * 3;} bg_next = now + 200000000ull; doing = "list"; return;}
      bg_fail = 0;
      char line[64], *p = put(line, "GMOS bgm "); p = put(p, g->dir); p = put(p, "/"); put(p, name); say(line);
    } else {
      for (uint32_t k = 0; k < os_tune_len; k++) bgm_buf[k] = os_tune[k];
      if (au_play_loaded(&aud, os_tune_len)) {bg_next = now + 5000000000ull; doing = "list"; return;}
      say("GMOS bgm OS tune");
    }
    bg_picks++; bg_state = 1; bg_t0 = now; bg_loops0 = aud.cmf.loops; doing = "list";
  }
}
#define PLAY_W 640u /* a game's screen: 320 x 200 doubled, as before the page went 16:9 */
#define PLAY_H 480u
/* Another screen mode (sbx_fb_set_mode clears it), checked and logged with its time. 0 when it is up. */
static int screen_mode(uint32_t w, uint32_t h, uint32_t format) {
  uint64_t t0 = clock_ns(); sbx_fb_mode m; char line[64], *p;
  int bad = sbx_fb_set_mode(fb_fd, w, h, format) || sbx_fb_query(fb_fd, &m) || m.width != w || m.height != h || m.format != format ||
            m.pitch != w * (format == SBX_FB_XRGB8888 ? 4u : 2u);
  p = put(line, "GMOS screen "); p = num(p, w); p = put(p, "x"); p = num(p, h); p = put(p, format == SBX_FB_XRGB8888 ? "x32" : "x16");
  if (bad) put(p, " refused"); else {p = put(p, " in "); p = num(p, (uint32_t)((clock_ns() - t0) / 1000u)); put(p, " us");}
  say(line); return bad;
}
static struct hs_screen hsc;
/* The Top Ten shares the list's picture buffers (125 KB less BSS, kernel U_IMAGE_QUOTA): pic_* are only
   scratch while thumbnails load, big_* are re-read after (big_id = -1). */
#define hs_px pic_px
#define hs_pal pic_pal
#define back_px big_px
#define back_pal big_pal
/* PLAYGAME's CheckHighScores and ShowHighScores: the Top Ten screens in the 640x480x32 mode.
   score is the game's (a name entry first if it places), or -1 for the list alone; open is 1 when no
   game is open in the play core yet. The table is read from /OS/SAVES/<GAME>/, else seeded from the
   game's own .HIS (read only); a new name is written back there. */
struct top_ctx {uint32_t *px; int back, open; char *key, *stem; sbx_input_event *ev; uint32_t *count;};
static uint64_t top_now(void *u) {(void)u; return clock_ns();}
static void top_wait(void *u, uint64_t wake) {(void)u; audio_sleep_until(wake);}
static void top_service(void *u) {(void)u; ge_poll(); if (aud_on) audio_service(game.read, game.user);}
/* The draw is the slow part (a full 320x200 scale to the TV): feed the ring between its stages. */
static void top_draw(void *u, const struct hs_screen *h) {
  struct top_ctx *c = u;
  hs_draw_colors(h, hs_px, hs_pal, c->back ? back_px : 0, back_pal, c->open ? 0 : game.colors); audio_wait_poll(0);
  gs_show(hs_px, hs_pal, c->px, PLAY_W); audio_wait_poll(0);
  sbx_fb_present(fb_fd, 0, 0, PLAY_W, PLAY_H); audio_wait_poll(0);
}
static int top_keys(void *u, struct hs_screen *h) {
  struct top_ctx *c = u; int r = 0; int32_t s = -SBX_EAGAIN; char line[96], *p;
  for (unsigned batch = 0; batch < GI_DRAIN && have_input && !(s = gi_read(in_fd, c->ev, c->count)); batch++) {
    for (uint32_t i = 0; i < *c->count && !(r & HS_DONE); i++) {
      int a = hs_event(h, c->ev[i].type, c->ev[i].code, c->ev[i].value, clock_ns());
      if (a & HS_COMMIT) {
        int w = hl_save(&save_sbx_ops, c->key, c->stem, &h->t);
        p = put(line, "GMOS Top Ten "); p = put(p, c->stem); p = put(p, w ? " save failed " : " saved "); num(p, (uint32_t)-w); say(line);
      }
      r |= a;
    }
    if (r & HS_DONE) break;
  }
  if (have_input && s && s != -SBX_EAGAIN) {sbx_close(in_fd); have_input = 0; gi_reset(&list_keys); menu.shift = 0; say("GMOS input removed");}
  if (!have_input) open_input();
  return r;
}
static void top_ten(uint32_t *px, const struct gm_game *g, int32_t score, int open) {
  char line[96], stem[9], key[9], *p; void *user; gp_read_fn read = reader(g, &user);
  const char *gam = w_gam((unsigned)(g - menu.games));
  if (!gam[0] || !read || hl_stem(gam, stem)) {say("GMOS no Top Ten for this game"); return;}
  hl_key(stem, w_path((unsigned)(g - menu.games)), gam, key);
  struct his_table t; int seeded = 0, can_write = abi_minor >= 7;
  int e = hl_load(&save_sbx_ops, key, stem, read, user, &t, &seeded);
  /* a card read that failed for any reason but "no file" must not be saved over by the seed */
  if (e) {p = put(line, "GMOS Top Ten read failed "); p = num(p, (uint32_t)-e); put(p, ": no saving this time"); say(line); can_write = 0;}
  if (seeded) say("GMOS Top Ten seeded from the game's own .HIS");
  if (abi_minor < 7 && score > 0) say("GMOS saving needs the kernel's /OS/SAVES (ABI 1.7)");
  score = hs_entry_score(score, can_write);
  /* PLAYGAME shows File(1,2) (lost) or (1,0) (won) and plays File(1,3)/(1,1), then clears the screen with
     BoxFill before the name entry (PLAYGAME.C 1149-1176): the entry's backdrop is black, as here, and the
     song runs on through it until the menu resumes. The active session now keeps its
     music alive while this screen services audio. */
  big_id = -1;
  int back = !gs_scores_picture(&game, open, read, user, gam, back_px, back_pal);
  if (screen_mode(PLAY_W, PLAY_H, SBX_FB_XRGB8888)) return;
  sbx_input_event ev[GI_BATCH]; uint32_t count;
  sbx_input_state ist; int have_ist = have_input && !sbx_input_state_get(in_fd, &ist); /* also empties the queue */
  for (unsigned batch = 0; batch < GI_DRAIN && have_input && !gi_read(in_fd, ev, &count); batch++) {} /* keys left from the game are not an answer */
  hs_start(&hsc, &t, score, clock_ns());
  if (have_ist) hs_modifiers(&hsc, ist.keys); /* a Shift or Ctrl already down */
  p = put(line, "GMOS Top Ten "); p = put(p, stem); put(p, hsc.stage == HS_NAME ? ": name entry" : ": list"); say(line);
  struct top_ctx c = {px, back, open, key, stem, ev, &count};
  struct el_ops ops = {&c, {0, bt_health, audio_hook ? bt_audio : 0, beat_hook ? bt_hook : 0}, top_now, top_wait, top_service, top_draw, top_keys};
  for (int r = HS_DRAW; !(r & HS_DONE);) r = el_pass(&hsc, r, &ops); /* every pass feeds health and audio, and waits at most 20 ms */
  say("GMOS back from the Top Ten");
}
/* Five-second stage totals include syscall time; no serial traffic per sample. */
enum {PF_TICK, PF_RENDER, PF_SCALE, PF_PRESENT, PF_AUDIO, PF_MAINT, PF_EINK, PF_DIAG, PF_COUNT};
struct play_perf {uint64_t ns; uint32_t calls, max_us;};
static void perf_add(struct play_perf *v, uint64_t ns) {
  v->ns += ns; v->calls++;
  uint32_t us = (uint32_t)(ns / 1000u); if (us > v->max_us) v->max_us = us;
}
static const char *const perf_names[] = {"tick slice", "render", "scale/compare", "present", "audio service/pump", "heartbeat/audio", "eink", "diagnostics"};
static void perf_line(const struct play_perf *v, unsigned i) {
  char line[128], *p = put(line, "GMOS perf "); p = put(p, perf_names[i]);
  p = put(p, " calls "); p = num(p, v[i].calls);
  p = put(p, " total us "); p = num(p, (uint32_t)(v[i].ns / 1000u));
  p = put(p, " avg us "); p = num(p, v[i].calls ? (uint32_t)(v[i].ns / v[i].calls / 1000u) : 0);
  p = put(p, " max us "); num(p, v[i].max_us); say(line);
}
/* A pending snapshot owns its header, histogram and stage totals until drained.
 * No queue: skip new five-second snapshots while busy, and expose a cumulative
 * drop count in the next header. The live window still resets every interval. */
struct play_report {
  struct play_perf shot[PF_COUNT];
  char header[320], steps[128];
  unsigned left;
  uint64_t dropped;
};
static void report_frame(struct play_report *q) {
  if (!q->left) return;
  unsigned row = --q->left;
  if (row == PF_COUNT + 2) {say(q->header); gl_add(q->header, clock_ns());}
  else if (row == PF_COUNT + 1) {say(q->steps); gl_add(q->steps, clock_ns());}
  else if (row == PF_COUNT) {if (aud_on) audio_log("game 5 s");}
  else perf_line(q->shot, row);
  if (!row && abi_minor >= 7) gl_flush(&save_sbx_ops, clock_ns(), 0); /* a card write from the foreground, at most every 30 s */
}
static void report_snapshot(struct play_report *q, const struct play_perf *perf,
                            uint32_t frames, uint32_t ticks, uint32_t step_max,
                            uint32_t draw_max, uint32_t late_max, const uint32_t *per_step) {
  if (q->left) {q->dropped++; return;}
  char *p = put(q->header, "GMOS 5 s: "); p = num(p, frames); p = put(p, " frames, tick "); p = num(p, ticks);
  p = put(p, ", step max "); p = num(p, step_max); p = put(p, " us, draw max "); p = num(p, draw_max);
  p = put(p, " us, late max "); p = num(p, late_max); p = put(p, " us, reports dropped "); num64(p, q->dropped);
  p = put(q->steps, "GMOS 5 s ticks per step: 1 x "); p = num(p, per_step[0]); p = put(p, ", 2 x "); p = num(p, per_step[1]);
  p = put(p, ", 3 x "); p = num(p, per_step[2]); p = put(p, ", 4+ x "); num(p, per_step[3]);
  for (unsigned i = 0; i < PF_COUNT; i++) q->shot[i] = perf[i];
  q->left = PF_COUNT + 3; /* header and audio share the same frame budget as every other row */
}
/* Plays the chosen game until Esc or its end. Each tick logs its number and the
   view's hash (the Pocket Player host test's), each game key the tick it lands
   before, and every 5 s the timing: the host tests replay those lines. */
static void play(uint32_t *px, const struct gm_game *g) { /* px: the surface, in the game's 640x480x32 */
  char *p, line[320];const char *gamname=w_gam((unsigned)(g-menu.games));
  void *user; gp_read_fn read = reader(g, &user);

  if (abi_minor < 1) {say("GMOS playing needs the kernel's clock (ABI 1.1)"); return;}
  game.say = say; game.keep = gl_add; game.clock_ns = clock_ns; game.beat = heartbeat;
  snd_dir(&game.sound, g->dir);
  p = put(line, "GMOS play ");p=put(p,g->dir);p=put(p,"/");put(p,gamname);say(line);
  audio_take(); doing = "game";
  if (!gamname[0] || !read || gs_launch(&game, read, user, gamname, clock_ns())) {bg_return("cannot play"); say("GMOS cannot play it"); return;}
  if (screen_mode(PLAY_W, PLAY_H, SBX_FB_XRGB8888)) {bg_return("no screen"); return;}
  ge_play(g->title);
  sbx_input_event ev[GI_BATCH]; uint32_t count;
  uint32_t frames = 0, step_max = 0, draw_max = 0, late_max = 0, logged = 0, shown = 0;
  uint64_t since = clock_ns();
  struct play_perf perf[PF_COUNT] = {{0, 0, 0}};
  int force = 1;
  /* The 5 s report goes out one line per frame: a burst of a dozen serial lines held the loop for ~100 ms (two ticks). */
  struct play_report report = {0};
  uint32_t per_step[4] = {0, 0, 0, 0}; /* steps that ran 1, 2, 3 or 4+ ticks: the device's own count, which serial log loss cannot touch */
  int r = GS_DRAW, started = 0, sp_on = 0, sp_draw = 0, noted_recent = 0;
  static struct sp_menu sp; char sp_stem[9], sp_key[9]; int sp_ok = !hl_stem(gamname, sp_stem);
  if (sp_ok) hl_key(sp_stem, w_path((unsigned)(g - menu.games)), gamname, sp_key);
  for (;;) {
    awake_session(&game, sp_on);
    uint64_t maintenance = clock_ns();
    heartbeat(); perf_add(&perf[PF_MAINT], clock_ns() - maintenance);
    maintenance = clock_ns(); ge_poll(); perf_add(&perf[PF_EINK], clock_ns() - maintenance);
    if (game.phase == GS_SCORES) {
      top_ten(px, g, game.score_check ? gp_score() : -1, 0); force = 1;
      r = gs_menu_return(&game, clock_ns());
      started = 0;
    }
    if (!noted_recent && game.phase==GS_PLAY) {
      noted_recent=1;
      if (gm_played(&menu,(unsigned)(g-menu.games)) && abi_minor>=7 && gm_list_save(&menu.recent,&save_sbx_ops,1)) say("GMOS Recent: changes not saved");
    }
    if (audio_note_tick()) {force = 1; r |= GS_DRAW;}
    if (r & GS_DRAW) {
      uint64_t d0 = clock_ns();
      uint32_t first, height;
      audio_poll();
      gs_draw_changed(&game, px, PLAY_W, force, &first, &height); force = 0;
      if (audio_note_until) {
        audio_note_draw(px, PLAY_W, 1);
        height += first; first = 0; if (height < 34) height = 34;
      }
      audio_poll();
      uint64_t scaled = clock_ns(); perf_add(&perf[PF_SCALE], scaled - d0);
      if (height) {
        int32_t presented = sbx_fb_present(fb_fd, 0, first, PLAY_W, height);
        perf_add(&perf[PF_PRESENT], clock_ns() - scaled);
        audio_poll();
        /* Only a successful present earns a line; idle draws/catch-up earn none.
         * Drain here, before sleeping or stepping, so the budget cannot go stale. */
        if (!presented) report_frame(&report);
      }
      uint32_t dt = (uint32_t)((clock_ns() - d0) / 1000u); if (dt > draw_max) draw_max = dt;
      frames++;
      uint64_t diag = clock_ns();
      if (game.showing_card && game.shown != shown) { /* a page fully up: faded in, played or written */
        shown = game.shown;
        p = put(line, "GMOS page frame crc "); num(p, crc32(px, PLAY_W * PLAY_H * 4u)); say(line);
      }
      if (!game.showing_card && game.ran && (game.total_ticks == 1 || game.total_ticks == 18)) {
        p = put(line, "GMOS frame crc "); p = num(p, crc32(px, PLAY_W * PLAY_H * 4u)); p = put(p, " tick "); num(p, game.total_ticks); say(line);
      }
      perf_add(&perf[PF_DIAG], clock_ns() - diag);
    }
    if (r & GS_EXIT) break;
    if (started) {game.t0 = clock_ns(); game.ticks = 0; started = 0;} /* a level's clock starts once its files are read and its first frame is up */
    uint64_t wake = sp_on ? sp_wake(&sp, clock_ns()) : gs_wake(&game, clock_ns());
    if (aud_on) {
      uint64_t s0 = clock_ns(); audio_service(game.read, game.user);
      perf_add(&perf[PF_AUDIO], clock_ns() - s0);
      uint64_t soon = clock_ns() + 10000000ull; /* wake every 10 ms to keep the ring fed */
      if (wake > soon) wake = soon;
    }
    audio_sleep_until(wake);
    int32_t s = -SBX_EAGAIN;
    for (unsigned batch = 0; batch < GI_DRAIN && have_input && !(s = gi_read(in_fd, ev, &count)); batch++) {
      for (uint32_t i = 0; i < count; i++) {
        if (ev[i].type == 0 && ev[i].code == 3) {sbx_input_state st; sbx_input_state_get(in_fd, &st); gp_release_all(); say("GMOS input resync after SYN_DROPPED"); break;}
        if (!sp_on && play_music_event(&ev[i])) continue; /* a save slot's name is typed text: its M is a letter, not the mute toggle */
        if (sp_on || (ev[i].type == 1 && ev[i].value == 1 && (ev[i].code == 63 || ev[i].code == 64) && sp_ok && game.phase == GS_PLAY && !game.demo)) {
          /* PLAYGAME's F5 (SaveGame) and F6 (LoadGame): the play waits while the screen is up (PLAYGAME.C 1609-1613) */
          int q;
          if (!sp_on) {
            gp_release_all(); sp_on = 1;
            ge_saving(ev[i].code == 63);
            sp_start(&sp, ev[i].code == 63 ? SP_SAVE : SP_LOAD, &save_sbx_ops, sp_key, &game, clock_ns()); q = SP_DRAW;
          } else q = sp_event(&sp, ev[i].type, ev[i].code, ev[i].value, clock_ns());
          if (q & SP_DRAW) sp_draw = 1;
          if (q & SP_FATAL) {game.phase = GS_DONE; r = GS_EXIT; break;} /* a failed load that could not be undone */
          if (q & SP_DONE) {sp_on = 0; ge_saving(0); game.showing_card = 0; gs_resume(&game, clock_ns()); sp_draw = 1;}
          continue;
        }
        if (ev[i].type == 1 && ev[i].value != 2 && game.phase == GS_PLAY && gs_scan(ev[i].code) && ev[i].code != 1) {
          p = put(line, "GMOS key "); p = num(p, (uint32_t)gs_scan(ev[i].code)); p = put(p, ev[i].value ? " down" : " up");
          p = put(p, " before tick "); num(p, game.total_ticks + 1); say(line);
        }
        int q = gs_event(&game, ev[i].type, ev[i].code, ev[i].value);
        if(q & (GS_SAVE|GS_LOAD)) {
          if(sp_ok) {sp_on=1;ge_saving((q&GS_SAVE)!=0);sp_start(&sp,(q&GS_SAVE)?SP_SAVE:SP_LOAD,&save_sbx_ops,sp_key,&game,clock_ns());}
          else gs_resume(&game,clock_ns());
          sp_draw=1;
        }
        if (q & GS_DRAW) sp_draw = 1;
        if (q & GS_EXIT) {r = GS_EXIT; break;}
        if (game.phase == GS_SCORES) break;
      }
      if (r & GS_EXIT) break;
    }
    if (r & GS_EXIT) break;
    if (have_input && s && s != -SBX_EAGAIN) {sbx_close(in_fd); have_input = 0; gp_release_all(); say("GMOS input removed");}
    if (!have_input) open_input();
    uint64_t t = clock_ns();
    if (game.phase == GS_PLAY) {uint64_t due = gs_wake(&game, t); if (t > due) {uint32_t l = (uint32_t)((t - due) / 1000u); if (l > late_max) late_max = l;}}
    uint32_t before = game.total_ticks; int was = game.phase;
    if (sp_on) { /* the screen's own clock: the cursor blink, the two seconds of a message */
      int q = sp_step(&sp, t); r = 0;
      if (q & SP_DRAW) r = GS_DRAW;
      if (q & SP_DONE) {sp_on = 0; ge_saving(0); game.showing_card = 0; gs_resume(&game, clock_ns()); r = GS_DRAW;}
      if (sp_draw) {r |= GS_DRAW; sp_draw = 0;}
      continue;
    }
    r = gs_step(&game, t);
    if (game.ran) perf_add(&perf[PF_TICK], game.tick_ns);
    if (game.render_ns) perf_add(&perf[PF_RENDER], game.render_ns);
    au_set_playing(&aud, aud_on && game.phase == GS_PLAY); audio_poll();
    if (sp_draw) {r |= GS_DRAW; sp_draw = 0;}
    uint32_t dt = (uint32_t)((clock_ns() - t) / 1000u); if (game.ran && dt > step_max) step_max = dt;
    if (was != GS_PLAY && game.phase == GS_PLAY) started = 1;
    if (was == GS_PLAY && game.phase == GS_CARDS) { /* the level ended: the session lets go of every key */
      p = put(line, "GMOS keys released before tick "); num(p, game.total_ticks + 1); say(line);
    }
    uint64_t diag = clock_ns();
    if (game.total_ticks != before && was == GS_PLAY) {uint32_t n = game.total_ticks - before; per_step[n > 4 ? 3 : n - 1]++;}
    if (game.total_ticks != before && (r & GS_DRAW) && logged++ < 20000) { /* the view after the step's last tick (a late step runs up to 4) */
      p = put(line, "GMOS t "); p = num(p, game.total_ticks); p = put(p, " lv "); p = num(p, (uint32_t)gp_level() + 1); p = put(p, " h "); num(p, gp_pixels_hash(game.px)); say(line);
    }
    perf_add(&perf[PF_DIAG], clock_ns() - diag);
    if (clock_ns() - since >= 5000000000ull) {
      report_snapshot(&report, perf, frames, game.total_ticks, step_max, draw_max, late_max, per_step);
      per_step[0] = per_step[1] = per_step[2] = per_step[3] = 0;
      for (unsigned i = 0; i < PF_COUNT; i++) perf[i] = (struct play_perf){0, 0, 0};
      frames = step_max = draw_max = late_max = 0; since = clock_ns();
    }
  }
  awake_set(0);
  bg_return("game over");
  ge_saving(0);
  ge_library(menu.count);
  say("GMOS back to the games list");
}
/* The demo preview (docs/demo.md): a game left selected on its details view for GS_DEMO_IDLE_NS plays its
   demo .REC in the picture's place; any key stops it (and is then handled as usual); when it ends the
   title screen returns and the wait starts again. A game with no .REC shows nothing extra. */
static uint64_t demo_idle;
static int arch_dirty; static uint64_t arch_shown; /* the Archive tab changed since the last redraw; when it was drawn */
static void demo_present(uint16_t *px) {
  uint32_t x, y, w, h; gm_picture_rect(&x, &y, &w, &h);
  audio_poll(); gm_draw_picture(px, GM_W, game.px, game.pal); audio_poll();
  sbx_fb_present(fb_fd, x, y, w, h); audio_poll();
}
static void demo_stop(const char *why) {
  char line[64], *p = put(line, "GMOS demo stopped: "); put(p, why); say(line);
  awake_set(0);
  gs_demo_stop(&game); demo_on = 0; big_id = -1; bg_return(why);
}
/* Starts the demo of the selected game on its details view; draws its first frame. */
static void demo_start(uint16_t *px, const struct gm_game *g) {
  char line[96], *p; void *user; gp_read_fn read = reader(g, &user);
  demo_none = (int)(g - menu.games);
  if (!read || !w_gam((unsigned)demo_none)[0]) return;
  game.say = say;
  audio_take(); /* the tune fades out first; the preview's own sound replaces it */
  if (gs_demo_start(&game, read, user, w_gam((unsigned)demo_none), clock_ns())) {bg_return("no demo"); return;} /* no demo */
  awake_set(1);
  demo_none = -1; demo_on = 1; doing = "demo";
  if (aud_on) au_music(&aud, gp_song(GP_SONG_TITLE, 0)); /* DOS: the menu song plays under the demo */
  p = put(line, "GMOS demo start "); p = put(p, g->dir); p = put(p, ", "); p = num(p, game.demo_n); put(p, " keys"); say(line);
  demo_present(px);
}
/* One step of the running demo (a tick is due, or the wait ends): draws a changed frame. 1 when it ended. */
static int demo_run(uint16_t *px) {
  uint64_t t = clock_ns();
  if (t < gs_wake(&game, t)) return 0;
  int r = gs_step(&game, t);
  if (r & GS_DRAW) {
    demo_present(px);
    if (game.total_ticks == 1 || game.total_ticks % 91 == 0) {char line[64], *p = put(line, "GMOS demo t "); p = num(p, game.total_ticks); p = put(p, " h "); num(p, gp_frame_hash()); say(line);}
  }
  if (r & GS_EXIT) {demo_stop("the demo ended"); demo_idle = t; return 1;}
  return 0;
}
static struct gm_load_progress picture_progress;
static uint64_t picture_now(void *u) {(void)u; return clock_ns();}
static void picture_present(void *u) {(void)u; heartbeat(); sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H);}
static void picture_tick(int force) {(void)force; gm_load_tick(&picture_progress);}
static void picture_checkpoint(void) {audio_poll(); picture_tick(0); audio_poll();}
/* The title screen of a game (thumb.h), read from its folder; -1 when it has none. */
static int title_picture(const struct gm_game *g, uint8_t *px, uint8_t *pal) {
  /* The page is visible before opening any archive or reading the GAM. */
  gm_load_start(&picture_progress, picture_now, picture_present, 0);
  beat_hook = picture_tick; gp_decode_hook(picture_checkpoint); ga_yield = picture_checkpoint;
  void *user; gp_read_fn read = reader(g, &user);
  int r = read ? gm_title_picture(read, user, w_gam((unsigned)(g-menu.games)), px, pal) : -1;
  gp_decode_hook(audio_poll); ga_yield = 0; beat_hook = 0;
  return r;
}
/* The next card whose title screen is not drawn (thumb.h: the selected one, its page, then the
   neighbor pages): reads it. 1 when a card on the page was read, 2 for a neighbor's, else 0. */
static int load_next_thumb(void) {
  int on_page = 0, id = gm_thumb_next(&menu, &on_page);
  if (id < 0) return 0;
  struct gm_game *g = &menu.games[id];
  uint64_t t0 = clock_ns();
  g->pic = title_picture(g, pic_px, pic_pal) ? -1 : 1;
  if (g->pic > 0) gm_thumb_put((unsigned)id, pic_px, pic_pal);
  char line[80], *p = put(line, "GMOS title screen "); p = put(p, g->dir); p = put(p, g->pic > 0 ? " drawn in " : " none, looked "); p = num(p, (uint32_t)((clock_ns() - t0) / 1000000u)); p = put(p, " ms"); put(p, on_page ? "" : " (ahead)"); say(line);
  return on_page ? 1 : 2;
}
/* Draws the games page and presents it; once every card on it is drawn, logs the frame's CRC (the host tests render the same frame). */
static void show_list(uint16_t *px) {
  if (!menu.count && !menu.options_active && !menu.open_keyboard) { /* an empty or unreadable library: the OS says why (bootart.h) */
    static char l3[40], l5[48]; char *p;
    p = put(l3, "SCAN NOTES: "); num(p, scan_detail_count); p = put(l5, "INDEX: "); put(p, no_index ? "NO LIBRARY.JSL FOUND" : "READ");
    const char *const why[] = {"NO GAMES FOUND.", "LOOKED IN /SD/GAMES AND /SD/POCKET FOR", "FOLDERS WITH A .GAM, AND .GMP, .ZIP, .EXE FILES.", l3, l5,
                               "CHECK THE CARD IS IN, AND COPY GAMES TO /SD/GAMES.", "BACK/ESC: OPTIONS (INCLUDING QUIT)."};
    ba_diag(px, GM_W, why, sizeof why / sizeof why[0]);
    audio_note_draw(px, GM_W, 0);
    sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H); say("GMOS library diagnostics"); return;
  }
  const struct gm_game *g = gm_selected(&menu);
  if (menu.view == GM_DETAILS && g && big_id != (int)(g - menu.games)) {
    big_id = (int)(g - menu.games); big_ok = 0;
    gm_view_picture_loading(1); gm_draw(px, GM_W, &menu, 0, big_pal);
    uint64_t t0 = clock_ns(); big_ok = !title_picture(g, big_px, big_pal); gm_view_picture_loading(0);
    char bl[64], *q = put(bl, "GMOS details picture "); q = put(q, g->dir); q = put(q, " in "); q = num(q, (uint32_t)((clock_ns() - t0) / 1000000u)); put(q, " ms"); say(bl);
  }
  audio_poll();
  gm_draw(px, GM_W, &menu, menu.view == GM_DETAILS && big_ok ? big_px : 0, big_pal);
  audio_note_draw(px, GM_W, 0);
  audio_poll();
  sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H);
  audio_poll();
  int pending = 0;
  if (menu.view == GM_GRID) {
    unsigned t = menu.tab, first = menu.pos[t] / GM_PAGE * GM_PAGE;
    for (unsigned i = 0; i < GM_PAGE; i++) {const struct gm_game *c = gm_tab_game(&menu, t, first + i); if (c && (c->pic == 0 || (c->pic > 0 && !gm_thumb_get((unsigned)(c - menu.games))))) pending = 1;}
  }
  if (!pending) {char line[48], *p = put(line, "GMOS list frame crc "); num(p, crc32(px, GM_W * GM_H * 2u)); say(line);}
}
static int gmos_main(const sbx_startup *start) {
  char line[128], *p;
  if(start->major!=SBX_ABI_MAJOR)return 4; /* never opt into a different major ABI */
  generation = start->generation; abi_minor = start->minor;
  sp_set_abi(start->major, start->minor);
  if (sbx_open("/dev/log", 8, SBX_O_WRONLY, &log_fd)) return 3;
  heartbeat(); /* readiness: the kernel wants it within 10 s */
  p = put(line, "GMOS hello, ABI "); p = num(p, start->major); p = put(p, "."); p = num(p, start->minor);
  p = put(p, ", generation "); num(p, generation); say(line);
  sbx_fb_mode mode; void *surface; uint16_t *px;
  if (abi_minor < 2) {say("GMOS needs screen modes (ABI 1.2)"); return 4;}
  /* Every program starts in 640x480x32, whose surface is the largest: map it once, then switch. */
  if (sbx_open("/dev/fb0", 8, SBX_O_RDWR, &fb_fd) || sbx_fb_query(fb_fd, &mode) || mode.buffer_bytes < GM_W * GM_H * 2u ||
      mode.buffer_bytes < PLAY_W * PLAY_H * 4u || sbx_fb_map(fb_fd, mode.buffer_bytes, &surface) || screen_mode(GM_W, GM_H, SBX_FB_RGB565)) {
    say("GMOS needs a /dev/fb0 that takes 960x540x16"); return 4;
  }
  px = (uint16_t *)surface;
  ba_splash(px, GM_W); sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H); say("GMOS boot splash"); /* until the list is ready */
  ge_init(start->major, start->minor);
  splash_px = px; sbx_clock_ns(&splash_t0); splash_last = splash_call = splash_t0; beat_hook = splash_tick;
  find_games();
  if (abi_minor>=7) {
    int e=gm_list_load(&menu.favorites,&save_sbx_ops,0);
    if(e) say("GMOS Favorites read failed; using an empty list");
    e=gm_list_load(&menu.recent,&save_sbx_ops,1);
    if(e) say("GMOS Recent read failed; using an empty list");
    e=gm_settings_load(&menu.settings,&save_sbx_ops);
    if(e) say("GMOS Settings read failed; using defaults");
  }
  if (awake_configure(&menu.settings)) say("GMOS power timeouts not applied");
  gm_refresh(&menu);
  splash_tick(1); {char l[64], *q = put(l, "GMOS splash longest gap between checkpoints "); q = num(q, splash_gap_ms); put(q, " ms"); say(l);} splash_px = 0; beat_hook = 0; /* a last step, so a quick load still moves once */
  p = put(line, "GMOS "); p = num(p, menu.count); p = put(p, menu.count == 1 ? " game" : " games"); put(p, " in /sd/GAMES"); say(line);
  for (unsigned i = 0; i < menu.count; i++) {
    const struct gm_game *g = &menu.games[i];
    static const char *const cat[GM_CATS] = {"starter", "sandbox", "bizarre", "cd", "other", "archive"};
    p = put(line, "GMOS game "); p = put(p, g->dir); p = put(p, " ["); p = put(p, cat[g->cat]); p = put(p, "]: "); put(p, g->title); say(line);
  }
  ge_library(menu.count);
  show_list(px);
  cache_flush(); /* the scan cache, now that the list is up (the kernel queues key presses meanwhile) */
  open_input();
  if (!have_input) say("GMOS no input yet; trying again");
  sbx_input_event ev[GI_BATCH]; uint32_t count;
  audio_open(); doing = "list"; uint64_t list_log = clock_ns();
  for (;;) {
    awake_set(demo_on);
    heartbeat();
    if (audio_note_tick()) show_list(px);
    /* The screen is blanked (power saving): the music and menu sounds stop with it, no synthesis runs, and they pick up again on any key. */
    int dark = awake_screen_dark();
    if (dark) {if (!dark_since) dark_since = clock_ns(); au_sleep(&aud, 1);} else bg_undark();
    if (aud_on && !dark) {
      audio_service(demo_on ? game.read : 0, game.user); if (!demo_on) bg_tick();
      if (clock_ns() - list_log >= 10000000000ull) {list_log = clock_ns(); audio_log(demo_on ? "demo 10 s" : "list 10 s");}
    }
    ge_poll();
    if (!have_input) {
      open_input();
    }
    int32_t s = have_input ? gi_read(in_fd, ev, &count) : -SBX_EAGAIN;
    if (s == -SBX_EAGAIN) { /* nothing pressed: draw the next card's title screen, or look again in 10 ms */
      if (demo_on) {
        if (demo_run(px)) show_list(px);
        else {uint64_t w = gs_wake(&game, clock_ns()), n = clock_ns() + 10000000ull; audio_sleep_until(w < n ? w : n);}
        continue;
      }
      doing = "thumbnail";
      int loaded = load_next_thumb();
      if (loaded) {doing = "list"; if (loaded == 1) show_list(px);}
      else if (arch_next < arch_n) { /* one ZIP per pass, so keys are read between; the heartbeat in its reads pumps the music, which plays from its own buffer and shares no reader */
        doing = "archive"; arch_dirty |= arch_step();
        uint64_t now = clock_ns();
        if (arch_dirty && (arch_next >= arch_n || now - arch_shown >= 400000000ull)) {arch_dirty = 0; arch_shown = now; doing = "list"; show_list(px);}
        if (arch_next >= arch_n) {char l[64], *q = put(l, "GMOS archive tab: "); q = num(q, menu.archive_count); put(q, menu.archive_count == 1 ? " game" : " games"); say(l);}
      } else {
        doing = "list";
        const struct gm_game *g = gm_selected(&menu);
        if (!menu.open_active && !menu.options_active && menu.view == GM_DETAILS && g && demo_none != (int)(g - menu.games) && gs_demo_idle_due(demo_idle, clock_ns()) && abi_minor >= 1) demo_start(px, g);
        else audio_sleep_until(clock_ns() + 10000000ull);
      }
      continue;
    }
    if (s) {sbx_close(in_fd); have_input = 0; gi_reset(&list_keys); menu.shift = 0; say(s == -SBX_ENODEV ? "GMOS input removed" : "GMOS input error"); continue;}
    demo_idle = clock_ns(); bg_undark(); /* a key wakes the sounds before any modal the key opens */
    {int stopped = 0; for (uint32_t i = 0; i < count; i++) if (demo_on && gs_demo_stops(ev[i].type, ev[i].value)) {demo_stop("a key"); stopped = 1; break;} if (stopped) show_list(px);}
    int redraw = 0, action = GM_NONE, save_favorite = 0, music_changed = 0;
    for (uint32_t i = 0; i < count; i++) {
      if (!gi_accept(&list_keys, &ev[i], clock_ns())) continue;
      int a = gm_event(&menu, ev[i].type, ev[i].code, ev[i].value);
      if (menu.cue && aud_on && !demo_on) au_ui(&aud, menu.cue); /* library, details, Options only: a game has its own loop */
      if (a == GM_OPEN_FOUND || a == GM_OPEN_MISSING || a == GM_OPEN_PLAY) {
        p = put(line, "GMOS open "); p = put(p, menu.open_dir);
        put(p, a == GM_OPEN_MISSING ? ": not found" : ": found"); say(line);
        redraw = 1;
        if (a == GM_OPEN_PLAY) a = GM_CHOSEN;
      }
      if (a == GM_MUSIC) {
        music_feedback(menu.background_music);
        music_changed = 1;
        redraw = 1;
      }
      if (a == GM_FAVORITE) {
        save_favorite = 1;
        redraw=1;
      }
      if (a == GM_SETTINGS) {
        int saved = abi_minor>=7 && !gm_settings_save(&menu.settings,&save_sbx_ops);
        int applied = !awake_configure(&menu.settings);
        put(menu.message, !saved ? "Changes not saved" :
            !applied ? "Power timeouts not applied" : "Settings saved");
        redraw=1;
      }
      if (a == GM_MOVED) redraw = 1;
      if (a == GM_RESYNC) {sbx_input_state st; sbx_input_state_get(in_fd, &st); menu.shift = 0; gi_reset(&list_keys); say("GMOS input resync after SYN_DROPPED"); break;}
      if (a == GM_CHOSEN || a == GM_SCORES || a == GM_QUIT) {action = a; break;}
    }
    /* Finish the immutable input batch before any file IO, decode or nested
     * screen loop. Those operations may outlive the device that supplied it. */
    if (save_favorite && (abi_minor<7 || gm_list_save(&menu.favorites,&save_sbx_ops,0))) put(menu.message,"Changes not saved");
    if (music_changed) {
      if (!menu.background_music) bg_leave();
      else bg_next = clock_ns();
    }
    if (action == GM_QUIT) {
      /* The kernel does not repaint on exit: leave a black frame, not the Options menu. */
      if (!screen_mode(GM_W, GM_H, SBX_FB_RGB565)) {ba_blank(px, GM_W); sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H);}
      say("GMOS quit"); return 0;
    }
    if (action == GM_CHOSEN || action == GM_SCORES) {
      const struct gm_game *g = gm_selected(&menu);
      if (g) {
        if (action == GM_CHOSEN) {
          p = put(line, "GMOS chose "); p = put(p, g->dir); p = put(p, " \""); p = put(p, g->title); put(p, "\""); say(line);
          ga_yield = game_yield; play((uint32_t *)surface, g); ga_yield = 0;
        } else top_ten((uint32_t *)surface, g, -1, 1);
        if (screen_mode(GM_W, GM_H, SBX_FB_RGB565)) return 4;
        menu.message[0] = 0; menu.shift = 0; redraw = 1; big_id = -1;
        gi_reset(&list_keys);
        if (have_input) {sbx_input_state st; sbx_input_state_get(in_fd, &st);}
      }
    }
    if (redraw) {
      const struct gm_game *sel = gm_selected(&menu); /* which card, so a log tells where a crash happened */
      if (sel) {char sl[64], *q = put(sl, menu.view == GM_DETAILS ? "GMOS details " : "GMOS select "); put(q, sel->dir); say(sl);}
      show_list(px);
    }
  }
}

/* All returns, including failed mode changes and startup errors, drop our hold. */
int main(const sbx_startup *start) {
  awake_init(start->major, start->minor);
  int status = gmos_main(start);
  awake_shutdown();
  return status;
}
