/* SPDX-License-Identifier: MIT; GMOS.ELF, the first Game-Maker OS program.
 * Built only against the sandbox-kernel SDK (sdk/). It reports readiness,
 * lists the games in /sd/GAMES (each a folder holding a .GAM file, titled
 * from that file), draws the list on /dev/fb0, moves the selection with
 * /dev/input/event0 and logs the game chosen with Enter. Esc or Q quits. */
#include <sandbox/syscall.h>
#include "gam.h"
#include "menu.h"
#include "view.h"
static uint64_t log_fd, fb_fd, in_fd;
static uint32_t generation;
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
/* The first .GAM file in /sd/GAMES/<dir>, read far enough for its title. */
static void add_game(const char *dir, unsigned dn) {
  char path[40], *p = put(path, "/sd/GAMES/"), name[13], title[GM_TITLE_MAX + 1];
  for (unsigned i = 0; i < dn; i++) *p++ = dir[i];
  *p = 0;
  uint64_t fd, got; static uint8_t list[512];
  if (sbx_open(path, (uint32_t)(p - path), SBX_O_RDONLY, &fd)) return;
  name[0] = 0;
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
int main(const sbx_startup *start) {
  char line[96], *p;
  generation = start->generation;
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
  static sbx_input_event ev[16]; uint64_t got; uint32_t idle = 0;
  for (;;) {
    heartbeat();
    if (!have_input) {
      if (++idle % 4096 == 0) open_input();
      continue;
    }
    int32_t s = sbx_read(in_fd, ev, sizeof ev, &got);
    if (s == -SBX_EAGAIN) continue;
    if (s) {sbx_close(in_fd); have_input = 0; say(s == -SBX_ENODEV ? "GMOS input removed" : "GMOS input error"); continue;}
    int redraw = 0;
    for (uint32_t i = 0; i < got / sizeof ev[0]; i++) {
      int a = gm_event(&menu, ev[i].type, ev[i].code, ev[i].value);
      if (a == GM_MOVED) redraw = 1;
      if (a == GM_RESYNC) {sbx_input_state st; sbx_input_state_get(in_fd, &st); say("GMOS input resync after SYN_DROPPED"); break;}
      if (a == GM_CHOSEN) {
        const struct gm_game *g = &menu.games[menu.sel];
        p = put(line, "GMOS chose "); p = put(p, g->dir); p = put(p, " \""); p = put(p, g->title); put(p, "\" (playing comes later)"); say(line);
        p = put(menu.message, "Chose "); put(p, g->title); redraw = 1;
      }
      if (a == GM_QUIT) {say("GMOS quit"); return 0;}
    }
    if (redraw) {gm_draw(px, GM_W, &menu); sbx_fb_present(fb_fd, 0, 0, GM_W, GM_H);}
  }
}
