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
#include "menu.h"
#include "sdpack.h"
#include "sdzip.h"
#include "session.h"
#include "thumb.h"
#include "view.h"
#ifndef GMOS_SCAN_VERBOSE
#define GMOS_SCAN_VERBOSE 0
#endif
static uint64_t log_fd, fb_fd, in_fd;
static uint32_t generation, abi_minor;
static uint64_t beat;
static int have_input;
static struct gm_menu menu;
/* Menu labels remain DOS-sized; exact UTF-8 locations are separate. Kind 0
 * is a loose folder, 1 a GMP volume set, 2 an in-place ZIP (including SFX). */
static struct {uint8_t packed; struct sp_pack pack; char path[256],gam[256];uint32_t size;} where[GM_MAX_GAMES];
static struct sp_pack packs[GM_MAX_GAMES];
static uint8_t gamfile[64 * 1024], index_text[256 * 1024];
static uint8_t pic_px[GP_W * GP_H], pic_pal[768], big_px[GP_W * GP_H], big_pal[768];
static int big_id = -1, big_ok;
static unsigned length(const char *s) {unsigned n = 0; while (s[n]) n++; return n;}
static char *put(char *p, const char *s) {while (*s) *p++ = *s++; *p = 0; return p;}
static char *num(char *p, uint32_t n) {char t[11]; int k = 0; do t[k++] = (char)('0' + n % 10); while (n /= 10); while (k) *p++ = t[--k]; *p = 0; return p;}
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
static void heartbeat(void) {sbx_health(generation, ++beat);}
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
      if(sbx_open(path,18,SBX_O_RDONLY,&fd)){say("GMOS no library index; titles from the .GAM files");return;}
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
static void add_zip(unsigned id) {
  const char *path=zip_candidates[id].path;const uint8_t *key=zip_candidates[id].key;
  struct cache_source source={0};int keyed=zip_candidates[id].keyed;
  if(!(keyed&&cache_source_get(path,key,16,&source))){
    uint32_t errors=sz_io_errors;struct gz_zip *z=sz_open(path,zip_candidates[id].size,abi_minor);
    int32_t have=z?gz_read(z,z->gam,gamfile,sizeof gamfile):-1;
    if(have>=0&&gm_gam_title(gamfile,(uint32_t)have,source.title)>=0){source.valid=1;source.crc=gm_crc32(0,gamfile,(uint32_t)have);put(source.gam,z->gam);}
    sz_close();if(errors!=sz_io_errors)cache_enabled=0;
  }
  if(keyed)cache_source_put(path,key,16,&source,0);
  if(!source.valid){scan_say("GMOS skipped archive: unsupported, damaged, or no game");return;}
  for(unsigned i=0;i<menu.count;i++)if(menu.games[i].crc==source.crc)return;
  const char *base=path;for(const char *p=path;*p;p++)if(*p=='/')base=p+1;
  if(gm_add(&menu,base,source.gam,source.title,source.crc)){
    unsigned i=menu.count-1;where[i].packed=2;where[i].size=zip_candidates[id].size;put(where[i].path,path);put(where[i].gam,source.gam);
  }
}
static void find_games(void) {
  scan_detail_count = 0;zip_count=0;
  cache_begin();sp_beat = heartbeat;sz_beat=heartbeat;
  find_packs(SP_FOLDER_POCKET); /* precedence: GMP, loose folder, ZIP/SFX */
  find_packs(SP_FOLDER_GAMES);
  scan_tree("",0);
  for(unsigned i=0;i<zip_count;i++)if(zip_candidates[i].directory){const char *p=zip_candidates[i].path+10;add_game(p,length(p));heartbeat();}
  for(unsigned i=0;i<zip_count;i++)if(!zip_candidates[i].directory){add_zip(i);heartbeat();}
  scan_summary();
  read_index();
  gm_finish(&menu);
  cache_end();
}
static void open_input(void) {
  sbx_input_info info; sbx_input_state st;
  if (sbx_open("/dev/input/event0", 17, SBX_O_RDONLY | SBX_O_NONBLOCK, &in_fd)) return;
  have_input = 1;
  char line[96], *p = put(line, "GMOS input event0");
  if (!sbx_input_query(in_fd, &info)) {
    p = put(p, info.flags & SBX_INPUT_SYNTHETIC ? " synthetic" : ""); p = put(p, ", queue "); p = num(p, (uint32_t)info.queue_records);
  }
  say(line);
  sbx_input_state_get(in_fd, &st); /* start from the current state and an empty queue */
}
/* game.h's folder reader: preserve spelling and reject escaping/oversized paths. */
static int32_t sd_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {
  char path[256],clean[256];
  if(gz_path(name,clean,0)||gz_noise(clean)||!scan_join(path,(const char *)user,clean))return -1;
  uint64_t fd, got; uint32_t have = 0;
  if (sbx_open(path, length(path), SBX_O_RDONLY, &fd)) return -1;
  int status=0;
  while (have < cap) {
    uint32_t take=cap-have<(1u<<20)?cap-have:1u<<20;
    status=sbx_read(fd,buf+have,take,&got);
    if(status||!got)break;
    have+=(uint32_t)got;
  }
  sbx_close(fd);
  heartbeat();
  return status?-1:(int32_t)have;
}
/* game.h's reader for the game g, and its user pointer: the folder's files, or the pack's (opened). */
static gp_read_fn reader(const struct gm_game *g, void **user) {
  unsigned i = (unsigned)(g - menu.games);
  sz_close();
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
static struct gs_session game;
/* Plays the chosen game until Esc or its end. Each tick logs its number and the
   view's hash (the Pocket Player host test's), each game key the tick it lands
   before, and every 5 s the timing: the host tests replay those lines. */
static void play(uint32_t *px, const struct gm_game *g) { /* px: the surface, in the game's 640x480x32 */
  char *p, line[320];const char *gamname=where[g-menu.games].gam;
  void *user; gp_read_fn read = reader(g, &user);

  if (abi_minor < 1) {say("GMOS playing needs the kernel's clock (ABI 1.1)"); return;}
  game.say = say;
  snd_dir(&game.sound, g->dir);
  p = put(line, "GMOS play ");p=put(p,g->dir);p=put(p,"/");put(p,gamname);say(line);
  if (!gamname[0] || !read || gs_start(&game, read, user, gamname, clock_ns())) {say("GMOS cannot play it"); return;}
  if (screen_mode(PLAY_W, PLAY_H, SBX_FB_XRGB8888)) return;
  static sbx_input_event ev[16]; uint64_t got;
  uint32_t frames = 0, step_max = 0, draw_max = 0, late_max = 0, logged = 0, shown = 0;
  uint64_t since = clock_ns();
  int r = GS_DRAW, started = 0;
  for (;;) {
    heartbeat();
    if (r & GS_DRAW) {
      uint64_t d0 = clock_ns();
      gs_draw(&game, px, PLAY_W); sbx_fb_present(fb_fd, 0, 0, PLAY_W, PLAY_H);
      uint32_t dt = (uint32_t)((clock_ns() - d0) / 1000u); if (dt > draw_max) draw_max = dt;
      frames++;
      if (game.showing_card && game.shown != shown) { /* a page fully up: faded in, played or written */
        shown = game.shown;
        p = put(line, "GMOS page frame crc "); num(p, crc32(px, PLAY_W * PLAY_H * 4u)); say(line);
      }
      if (!game.showing_card && game.ran && (game.total_ticks == 1 || game.total_ticks == 18)) {
        p = put(line, "GMOS frame crc "); p = num(p, crc32(px, PLAY_W * PLAY_H * 4u)); p = put(p, " tick "); num(p, game.total_ticks); say(line);
      }
    }
    if (r & GS_EXIT) break;
    if (started) {game.t0 = clock_ns(); game.ticks = 0; started = 0;} /* a level's clock starts once its files are read and its first frame is up */
    uint64_t wake = gs_wake(&game, clock_ns());
    sbx_sleep_until(wake);
    int32_t s = -SBX_EAGAIN;
    while (have_input && !(s = sbx_read(in_fd, ev, sizeof ev, &got))) {
      for (uint32_t i = 0; i < got / sizeof ev[0]; i++) {
        if (ev[i].type == 0 && ev[i].code == 3) {sbx_input_state st; sbx_input_state_get(in_fd, &st); gp_release_all(); say("GMOS input resync after SYN_DROPPED"); break;}
        if (ev[i].type == 1 && ev[i].value != 2 && game.phase == GS_PLAY && gs_scan(ev[i].code) && ev[i].code != 1) {
          p = put(line, "GMOS key "); p = num(p, (uint32_t)gs_scan(ev[i].code)); p = put(p, ev[i].value ? " down" : " up");
          p = put(p, " before tick "); num(p, game.total_ticks + 1); say(line);
        }
        if (gs_event(&game, ev[i].type, ev[i].code, ev[i].value) & GS_EXIT) {r = GS_EXIT; break;}
      }
      if (r & GS_EXIT) break;
    }
    if (r & GS_EXIT) break;
    if (have_input && s != -SBX_EAGAIN) {sbx_close(in_fd); have_input = 0; gp_release_all(); say("GMOS input removed");}
    if (!have_input) open_input();
    uint64_t t = clock_ns();
    if (game.phase == GS_PLAY) {uint64_t due = gs_wake(&game, t); if (t > due) {uint32_t l = (uint32_t)((t - due) / 1000u); if (l > late_max) late_max = l;}}
    uint32_t before = game.total_ticks; int was = game.phase;
    r = gs_step(&game, t);
    uint32_t dt = (uint32_t)((clock_ns() - t) / 1000u); if (game.ran && dt > step_max) step_max = dt;
    if (was != GS_PLAY && game.phase == GS_PLAY) started = 1;
    if (was == GS_PLAY && game.phase == GS_CARDS) { /* the level ended: the session lets go of every key */
      p = put(line, "GMOS keys released before tick "); num(p, game.total_ticks + 1); say(line);
    }
    if (game.total_ticks != before && logged++ < 20000) { /* the view after the step's last tick (a late step runs up to 4) */
      p = put(line, "GMOS t "); p = num(p, game.total_ticks); p = put(p, " lv "); p = num(p, (uint32_t)gp_level() + 1); p = put(p, " h "); num(p, gp_frame_hash()); say(line);
    }
    if (clock_ns() - since >= 5000000000ull) {
      p = put(line, "GMOS 5 s: "); p = num(p, frames); p = put(p, " frames, tick "); p = num(p, game.total_ticks);
      p = put(p, ", step max "); p = num(p, step_max); p = put(p, " us, draw max "); p = num(p, draw_max);
      p = put(p, " us, late max "); p = num(p, late_max); put(p, " us"); say(line);
      frames = step_max = draw_max = late_max = 0; since = clock_ns();
    }
  }
  say("GMOS back to the games list");
}
/* The title screen of a game (thumb.h), read from its folder; -1 when it has none. */
static int title_picture(const struct gm_game *g, uint8_t *px, uint8_t *pal) {
  void *user; gp_read_fn read = reader(g, &user);
  return read ? gm_title_picture(read, user, where[g-menu.games].gam, px, pal) : -1;
}
/* The next card on the page whose title screen is not drawn yet: reads it. 1 when one was read. */
static int load_next_thumb(void) {
  if (menu.view != GM_GRID) return 0;
  unsigned t = menu.tab, first = menu.pos[t] / GM_PAGE * GM_PAGE;
  for (unsigned i = 0; i < GM_PAGE; i++) {
    struct gm_game *g = (struct gm_game *)gm_tab_game(&menu, t, first + i);
    if (!g || g->pic < 0) continue;
    unsigned id = (unsigned)(g - menu.games);
    if (g->pic > 0 && gm_thumb_get(id)) continue;
    uint64_t t0 = clock_ns();
    g->pic = title_picture(g, pic_px, pic_pal) ? -1 : 1;
    if (g->pic > 0) gm_thumb_put(id, pic_px, pic_pal);
    char line[80], *p = put(line, "GMOS title screen "); p = put(p, g->dir); p = put(p, g->pic > 0 ? " drawn in " : " none, looked "); p = num(p, (uint32_t)((clock_ns() - t0) / 1000000u)); put(p, " ms"); say(line);
    return 1;
  }
  return 0;
}
/* Draws the games page and presents it; once every card on it is drawn, logs the frame's CRC (the host tests render the same frame). */
static void show_list(uint16_t *px) {
  const struct gm_game *g = gm_selected(&menu);
  if (menu.view == GM_DETAILS && g && big_id != (int)(g - menu.games)) {big_id = (int)(g - menu.games); big_ok = !title_picture(g, big_px, big_pal);}
  gm_draw(px, GM_W, &menu, menu.view == GM_DETAILS && big_ok ? big_px : 0, big_pal);
  sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H);
  int pending = 0;
  if (menu.view == GM_GRID) {
    unsigned t = menu.tab, first = menu.pos[t] / GM_PAGE * GM_PAGE;
    for (unsigned i = 0; i < GM_PAGE; i++) {const struct gm_game *c = gm_tab_game(&menu, t, first + i); if (c && (c->pic == 0 || (c->pic > 0 && !gm_thumb_get((unsigned)(c - menu.games))))) pending = 1;}
  }
  if (!pending) {char line[48], *p = put(line, "GMOS list frame crc "); num(p, crc32(px, GM_W * GM_H * 2u)); say(line);}
}
int main(const sbx_startup *start) {
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
  find_games();
  p = put(line, "GMOS "); p = num(p, menu.count); p = put(p, menu.count == 1 ? " game" : " games"); put(p, " in /sd/GAMES"); say(line);
  for (unsigned i = 0; i < menu.count; i++) {
    const struct gm_game *g = &menu.games[i];
    static const char *const cat[GM_CATS] = {"starter", "sandbox", "bizarre", "cd", "other"};
    p = put(line, "GMOS game "); p = put(p, g->dir); p = put(p, " ["); p = put(p, cat[g->cat]); p = put(p, "]: "); put(p, g->title); say(line);
  }
  show_list(px);
  open_input();
  if (!have_input) say("GMOS no input yet; trying again");
  static sbx_input_event ev[16]; uint64_t got;
  for (;;) {
    heartbeat();
    if (!have_input) {
      sbx_sleep_until(clock_ns() + 100000000ull);
      open_input();
      continue;
    }
    int32_t s = sbx_read(in_fd, ev, sizeof ev, &got);
    if (s == -SBX_EAGAIN) { /* nothing pressed: draw the next card's title screen, or look again in 10 ms */
      if (load_next_thumb()) show_list(px); else sbx_sleep_until(clock_ns() + 10000000ull);
      continue;
    }
    if (s) {sbx_close(in_fd); have_input = 0; say(s == -SBX_ENODEV ? "GMOS input removed" : "GMOS input error"); continue;}
    int redraw = 0;
    for (uint32_t i = 0; i < got / sizeof ev[0]; i++) {
      int a = gm_event(&menu, ev[i].type, ev[i].code, ev[i].value);
      if (a == GM_MOVED) redraw = 1;
      if (a == GM_RESYNC) {sbx_input_state st; sbx_input_state_get(in_fd, &st); menu.shift = 0; say("GMOS input resync after SYN_DROPPED"); break;}
      if (a == GM_CHOSEN) {
        const struct gm_game *g = gm_selected(&menu);
        p = put(line, "GMOS chose "); p = put(p, g->dir); p = put(p, " \""); p = put(p, g->title); put(p, "\""); say(line);
        play((uint32_t *)surface, g);
        if (screen_mode(GM_W, GM_H, SBX_FB_RGB565)) return 4;
        menu.message[0] = 0; menu.shift = 0; redraw = 1;
        big_id = -1; /* the game used the play core: read the title screen again */
        if (have_input) {sbx_input_state st; sbx_input_state_get(in_fd, &st);} /* keys held in the game are not the list's */
        break;
      }
      if (a == GM_QUIT) {say("GMOS quit"); return 0;}
    }
    if (redraw) show_list(px);
  }
}
