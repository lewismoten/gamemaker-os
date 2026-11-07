/* SPDX-License-Identifier: MIT; GMOS.ELF, the first Game-Maker OS program.
 * Built only against the sandbox-kernel SDK (sdk/). It reports readiness,
 * lists the games in /sd/GAMES (each a folder holding a .GAM file, titled
 * from that file), draws the list on /dev/fb0 and moves the selection with
 * /dev/input/event0. Enter plays the game chosen (session.h) at 18.2 Hz on
 * the kernel's clock, sleeping between ticks; Esc or the game's end returns
 * to the list, and Esc or Q there quits. */
#include <sandbox/syscall.h>
#include "gam.h"
#include "menu.h"
#include "session.h"
#include "view.h"
static uint64_t log_fd, fb_fd, in_fd;
static uint32_t generation, abi_minor;
static uint64_t beat;
static int have_input;
static struct gm_menu menu;
static uint8_t dents[2048], head[GM_GAM_TITLE_BYTES];
static unsigned length(const char *s) {unsigned n = 0; while (s[n]) n++; return n;}
static char *put(char *p, const char *s) {while (*s) *p++ = *s++; *p = 0; return p;}
static char *num(char *p, uint32_t n) {char t[11]; int k = 0; do t[k++] = (char)('0' + n % 10); while (n /= 10); while (k) *p++ = t[--k]; *p = 0; return p;}
static void say(const char *s) {sbx_log(log_fd, s, length(s));}
static void heartbeat(void) {sbx_health(generation, ++beat);}
static int ends_with_gam(const char *s, unsigned n) {
  return n > 4 && s[n - 4] == '.' && s[n - 3] == 'G' && s[n - 2] == 'A' && s[n - 1] == 'M';
}
/* The name of the first .GAM file in the folder `path` ("" if none). */
static void find_gam(const char *path, unsigned pn, char name[13]) {
  uint64_t fd, got; static uint8_t list[512];
  name[0] = 0;
  if (sbx_open(path, pn, SBX_O_RDONLY, &fd)) return;
  while (!name[0] && !sbx_getdents(fd, list, sizeof list, &got) && got) {
    for (uint32_t at = 0; at < got;) {
      const sbx_dirent *e = (const sbx_dirent *)(const void *)(list + at);
      const char *n = (const char *)(list + at + sizeof *e);
      if (e->type == SBX_DT_FILE && e->name_bytes <= 12 && ends_with_gam(n, e->name_bytes)) {
        for (unsigned i = 0; i < e->name_bytes; i++) name[i] = n[i];
        name[e->name_bytes] = 0; break;
      }
      at += e->length;
    }
  }
  sbx_close(fd);
}
/* The first .GAM file in /sd/GAMES/<dir>, read far enough for its title. */
static void add_game(const char *dir, unsigned dn) {
  char path[40], *p = put(path, "/sd/GAMES/"), name[13], title[GM_TITLE_MAX + 1];
  for (unsigned i = 0; i < dn; i++) *p++ = dir[i];
  *p = 0;
  uint64_t fd, got;
  find_gam(path, (uint32_t)(p - path), name);
  if (!name[0]) return; /* not a game folder */
  *p++ = '/'; p = put(p, name);
  if (sbx_open(path, (uint32_t)(p - path), SBX_O_RDONLY, &fd)) return;
  uint32_t have = 0;
  while (have < sizeof head && !sbx_read(fd, head + have, sizeof head - have, &got) && got) have += (uint32_t)got;
  sbx_close(fd);
  if (gm_gam_title(head, have, title) < 0) return;
  char d[9]; unsigned k = 0; while (k < dn && k < 8) {d[k] = dir[k]; k++;} d[k] = 0;
  gm_add(&menu, d, title);
}
static void find_games(void) {
  uint64_t fd, got;
  if (sbx_open("/sd/GAMES", 9, SBX_O_RDONLY, &fd)) {say("GMOS no /sd/GAMES folder"); return;}
  while (!sbx_getdents(fd, dents, sizeof dents, &got) && got) {
    for (uint32_t at = 0; at < got;) {
      const sbx_dirent *e = (const sbx_dirent *)(const void *)(dents + at);
      if (e->type == SBX_DT_DIR) add_game((const char *)(dents + at + sizeof *e), e->name_bytes);
      at += e->length;
      heartbeat();
    }
  }
  sbx_close(fd);
  gm_sort(&menu);
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
/* game.h's reader: a file of /sd/GAMES/<dir>, its name in upper case (the card's 8.3 names). */
static int32_t sd_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {
  char path[48], *p = put(path, "/sd/GAMES/"); p = put(p, (const char *)user); *p++ = '/';
  for (; *name && p < path + sizeof path - 1; name++) *p++ = *name >= 'a' && *name <= 'z' ? (char)(*name - 32) : *name;
  *p = 0;
  uint64_t fd, got; uint32_t have = 0;
  if (sbx_open(path, (uint32_t)(p - path), SBX_O_RDONLY, &fd)) return -1;
  while (have < cap && !sbx_read(fd, buf + have, cap - have, &got) && got) have += (uint32_t)got;
  sbx_close(fd);
  heartbeat();
  return (int32_t)have;
}
static uint64_t clock_ns(void) {uint64_t t = 0; sbx_clock_ns(&t); return t;}
static uint32_t crc32(const uint32_t *px, uint32_t words) {
  static uint32_t table[256];
  if (!table[1]) for (uint32_t i = 0; i < 256; i++) {uint32_t c = i; for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xedb88320u & (0u - (c & 1))); table[i] = c;}
  uint32_t c = ~0u;
  for (uint32_t i = 0; i < words; i++) for (int k = 0; k < 4; k++) c = table[(c ^ (px[i] >> (8 * k))) & 0xff] ^ c >> 8;
  return ~c;
}
static struct gs_session game;
/* Plays the chosen game until Esc or its end. Each tick logs its number and the
   view's hash (the Pocket Player host test's), each game key the tick it lands
   before, and every 5 s the timing: the host tests replay those lines. */
static void play(uint32_t *px, const struct gm_game *g) {
  char path[24], *p = put(path, "/sd/GAMES/"), gamname[13], line[120];
  p = put(p, g->dir);
  find_gam(path, (uint32_t)(p - path), gamname);
  if (abi_minor < 1) {say("GMOS playing needs the kernel's clock (ABI 1.1)"); return;}
  game.say = say;
  p = put(line, "GMOS play "); p = put(p, g->dir); p = put(p, "/"); put(p, gamname); say(line);
  if (!gamname[0] || gs_start(&game, sd_read, (void *)g->dir, gamname, clock_ns())) {say("GMOS cannot play it"); return;}
  static sbx_input_event ev[16]; uint64_t got;
  uint32_t frames = 0, step_max = 0, draw_max = 0, late_max = 0, logged = 0, cards = 0;
  uint64_t since = clock_ns();
  int r = GS_DRAW;
  for (;;) {
    heartbeat();
    if (r & GS_DRAW) {
      uint64_t d0 = clock_ns();
      gs_draw(&game, px, GM_W); sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H);
      uint32_t dt = (uint32_t)((clock_ns() - d0) / 1000u); if (dt > draw_max) draw_max = dt;
      frames++;
      if (game.showing_card && game.card != cards) {
        cards = game.card;
        p = put(line, "GMOS card frame crc "); num(p, crc32(px, GM_W * GM_H)); say(line);
      }
      if (!game.showing_card && game.ran && (game.total_ticks == 1 || game.total_ticks == 18)) {
        p = put(line, "GMOS frame crc "); p = num(p, crc32(px, GM_W * GM_H)); p = put(p, " tick "); num(p, game.total_ticks); say(line);
      }
    }
    if (r & GS_EXIT) break;
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
    uint32_t before = game.total_ticks;
    r = gs_step(&game, t);
    uint32_t dt = (uint32_t)((clock_ns() - t) / 1000u); if (game.ran && dt > step_max) step_max = dt;
    for (uint32_t k = before + 1; k <= game.total_ticks && logged < 20000; k++, logged++) {
      p = put(line, "GMOS t "); p = num(p, k); p = put(p, " lv "); p = num(p, (uint32_t)gp_level() + 1); p = put(p, " h "); num(p, gp_frame_hash()); say(line);
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
int main(const sbx_startup *start) {
  char line[96], *p;
  generation = start->generation; abi_minor = start->minor;
  if (sbx_open("/dev/log", 8, SBX_O_WRONLY, &log_fd)) return 3;
  heartbeat(); /* readiness: the kernel wants it within 10 s */
  p = put(line, "GMOS hello, ABI "); p = num(p, start->major); p = put(p, "."); p = num(p, start->minor);
  p = put(p, ", generation "); num(p, generation); say(line);
  sbx_fb_mode mode; uint32_t *px;
  if (sbx_open("/dev/fb0", 8, SBX_O_RDWR, &fb_fd) || sbx_fb_query(fb_fd, &mode) || mode.width != GM_W || mode.height != GM_H ||
      sbx_fb_map(fb_fd, mode.buffer_bytes, (void **)&px)) {say("GMOS needs a 640x480 /dev/fb0"); return 4;}
  find_games();
  p = put(line, "GMOS "); p = num(p, menu.count); p = put(p, menu.count == 1 ? " game" : " games"); put(p, " in /sd/GAMES"); say(line);
  for (unsigned i = 0; i < menu.count; i++) {p = put(line, "GMOS game "); p = put(p, menu.games[i].dir); p = put(p, ": "); put(p, menu.games[i].title); say(line);}
  gm_draw(px, GM_W, &menu); sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H);
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
    if (s == -SBX_EAGAIN) {sbx_sleep_until(clock_ns() + 10000000ull); continue;} /* look again in 10 ms */
    if (s) {sbx_close(in_fd); have_input = 0; say(s == -SBX_ENODEV ? "GMOS input removed" : "GMOS input error"); continue;}
    int redraw = 0;
    for (uint32_t i = 0; i < got / sizeof ev[0]; i++) {
      int a = gm_event(&menu, ev[i].type, ev[i].code, ev[i].value);
      if (a == GM_MOVED) redraw = 1;
      if (a == GM_RESYNC) {sbx_input_state st; sbx_input_state_get(in_fd, &st); say("GMOS input resync after SYN_DROPPED"); break;}
      if (a == GM_CHOSEN) {
        const struct gm_game *g = &menu.games[menu.sel];
        p = put(line, "GMOS chose "); p = put(p, g->dir); p = put(p, " \""); p = put(p, g->title); put(p, "\""); say(line);
        play(px, g);
        menu.message[0] = 0; redraw = 1;
        if (have_input) {sbx_input_state st; sbx_input_state_get(in_fd, &st);} /* keys held in the game are not the list's */
        break;
      }
      if (a == GM_QUIT) {say("GMOS quit"); return 0;}
    }
    if (redraw) {gm_draw(px, GM_W, &menu); sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H);}
  }
}
