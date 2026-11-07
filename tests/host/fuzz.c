/* SPDX-License-Identifier: MIT; a fuzz pass for make test-asan: the library
 * index reader, the games list (more games than it holds, long and broken
 * text), its drawing, the GIF and FLI decoders and the title-screen picker
 * fed truncated and mutated copies of a real game's files. Every input sits
 * in its own exact-size heap block, so AddressSanitizer sees any read past
 * it. Usage: fuzz <games folder holding DICE> [rounds] [folder of game folders...]:
 * the folders' games are listed, title-pictured and drawn too. Seeded, so a
 * failure repeats. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include "fli.h"
#include "gif.h"
#include "hostio.h"
#include "library.h"
#include "menu.h"
#include "thumb.h"
#include "view.h"
static uint64_t seed = 0x9e3779b97f4a7c15u;
static uint32_t rnd(void) {seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return (uint32_t)(seed >> 11);}
static uint32_t below(uint32_t n) {return n ? rnd() % n : 0;}
static char dice[1024];
static uint8_t *load(const char *name, uint32_t *n) {
  static uint8_t buf[1 << 20];
  int32_t got = host_read(dice, name, buf, sizeof buf);
  if (got < 0) {fprintf(stderr, "fuzz: no %s/%s\n", dice, name); exit(2);}
  uint8_t *d = malloc(got ? (size_t)got : 1); memcpy(d, buf, (size_t)got); *n = (uint32_t)got;
  return d;
}
/* A copy of d cut short, with a few bytes changed (or none): exact size. */
static uint8_t *mutate(const uint8_t *d, uint32_t n, uint32_t *out) {
  uint32_t m = below(4) ? n : below(n + 1);
  if (!below(8)) m = n + below(64);
  uint8_t *c = malloc(m ? m : 1);
  for (uint32_t i = 0; i < m; i++) c[i] = i < n ? d[i] : (uint8_t)rnd();
  for (uint32_t k = below(8); k-- && m;) {
    uint32_t at = below(4) ? below(m < 64 ? m : 64) : below(m); /* headers more often */
    c[at] = below(3) ? (uint8_t)rnd() : below(2) ? 0xff : 0;
  }
  *out = m;
  return c;
}
static const char *pieces[] = {"{", "}", "[", "]", ",", ":", "\"", "\\", "\\u", "\\ud83d\\ude00", "\\u00e9", "\\uD83D",
  "\"schema\"", "\"gmp-library\"", "\"title\"", "\"company\"", "\"blurb\"", "\"category\"", "\"dosName\"", "\"locator\"",
  "\"gamCrc32\"", "\"year\"", "\"course\"", "\"sandbox\"", "\"starter\"", "\"DICE\"", "\"0badf00d\"", "-", "1e999",
  "99999999999999", "true", "null", "\xc3\xa9", "\xe2\x80\x99", "\xf0\x9f\x98\x80", "\xc3", "\xe2\x80", "\xff", "\x80", " ", "\t"};
static uint32_t make_line(char *out, uint32_t cap) {
  uint32_t n = 0;
  if (below(3)) {
    const char *keys[] = {"title", "company", "blurb", "dosName", "locator", "gamCrc32"};
    n = (uint32_t)snprintf(out, cap, "{\"schema\":\"gmp-library\",\"category\":[\"%s\",\"x\",\"y\"],\"year\":%d",
                           below(2) ? "sandbox" : "bizarre", (int)rnd());
    for (int k = 0; k < 6; k++) {
      if (!below(4)) continue;
      n += (uint32_t)snprintf(out + n, cap - n, ",\"%s\":\"", keys[k]);
      for (uint32_t len = below(2) ? below(40) : below(1500); len-- && n + 8 < cap;) {
        uint32_t r = below(10);
        if (r < 6) out[n++] = (char)('A' + below(26));
        else if (r < 8) {const char *p = pieces[32 + below(7)]; while (*p && n + 1 < cap) out[n++] = *p++;}
        else out[n++] = (char)(0x80 + below(128));
      }
      if (n + 2 < cap) out[n++] = '"';
    }
    if (n + 1 < cap && below(8)) out[n++] = '}';
  } else
    for (uint32_t k = below(80); k--;) {const char *p = pieces[below(sizeof pieces / sizeof *pieces)]; while (*p && n + 1 < cap) out[n++] = *p++;}
  return n;
}
static void fuzz_library(uint32_t rounds) {
  static char line[8192];
  struct gm_record r;
  for (uint32_t i = 0; i < rounds * 20; i++) {
    uint32_t n = make_line(line, sizeof line);
    char *exact = malloc(n ? n : 1); memcpy(exact, line, n);
    gm_record_parse(exact, n, &r);
    free(exact);
  }
}
static void random_text(char *s, uint32_t n) {for (uint32_t i = 0; i < n; i++) s[i] = (char)(1 + below(255)); s[n] = 0;}
static void fuzz_menu(uint32_t rounds) {
  static char line[8192], dir[64], gam[64], title[2048];
  static uint32_t frame[640 * 480];
  static uint8_t big[64000], pal[768];
  struct gm_menu *m = malloc(sizeof *m);
  for (uint32_t i = 0; i < rounds; i++) {
    memset(m, 0, sizeof *m);
    uint32_t games = below(4) ? below(20) : 250 + below(20); /* past GM_MAX_GAMES too */
    for (uint32_t g = 0; g < games; g++) {
      random_text(dir, below(20)); random_text(gam, below(30)); random_text(title, below(2) ? below(30) : below(2000));
      if (!below(4)) snprintf(dir, sizeof dir, "DICE");
      gm_add(m, dir, gam, title, below(2) ? 0x0badf00du : rnd());
    }
    for (uint32_t k = below(30); k--;) {uint32_t n = make_line(line, sizeof line); char *e = malloc(n ? n : 1); memcpy(e, line, n); gm_library_line(m, e, n); free(e);}
    gm_finish(m);
    for (uint32_t g = 0; g < m->count; g++) m->games[g].pic = (int8_t)(below(3) - 1);
    for (uint32_t k = below(40); k--;) {
      static const uint16_t codes[] = {1, 14, 15, 16, 26, 27, 28, 42, 57, 102, 103, 104, 105, 106, 107, 108, 109};
      uint16_t c = codes[below(sizeof codes / sizeof *codes)];
      gm_event(m, (uint16_t)(below(10) ? 1 : below(4)), c, (int32_t)below(3));
      if (!below(10)) {random_text(m->message, below(GM_MESSAGE_MAX + 1));}
      if (!below(4)) {
        for (uint32_t p = 0; p < 64000; p++) big[p] = (uint8_t)rnd();
        for (uint32_t p = 0; p < 768; p++) pal[p] = (uint8_t)rnd();
        gm_draw(frame, 640, m, below(2) ? big : 0, pal);
      }
    }
  }
  free(m);
}
static void fuzz_pictures(uint32_t rounds) {
  static const char *gifs[] = {"TITLE.GIF", "MENU.GIF", "ONE.GIF", "CREDITS.GIF", "WON.GIF"};
  uint8_t *px = malloc(64000), *pal = malloc(768), *canvas = malloc(GP_FLI_PIXELS);
  for (uint32_t i = 0; i < rounds; i++) {
    uint32_t n, m; uint8_t *d = load(gifs[below(5)], &n), *c = mutate(d, n, &m);
    gp_gif_draw(c, m, (int32_t)below(800) - 400, (int32_t)below(600) - 300, pal, px);
    gp_gif_palette(c, m, pal);
    free(c); free(d);
    d = load("STARTER.FLI", &n); c = mutate(d, n, &m);
    struct gp_fli f;
    if (!gp_fli_open(&f, c, m)) for (uint32_t k = 0; k < f.frames + 2 && k < 400; k++) gp_fli_frame(&f, canvas, pal);
    free(c); free(d);
  }
  free(px); free(pal); free(canvas);
}
/* The title picker reading the game's folder with one file swapped for a mutated copy (or random BKD / LST text). */
static const char *victim; static uint8_t *swap; static uint32_t swap_n;
static int32_t reader(void *user, const char *name, uint8_t *buf, uint32_t cap) {
  if (victim && !strcasecmp(name, victim)) {uint32_t k = swap_n < cap ? swap_n : cap; memcpy(buf, swap, k); return (int32_t)k;}
  return host_read(user, name, buf, cap);
}
static void fuzz_title(uint32_t rounds) {
  static const char *files[] = {"DICE.GAM", "TITLE.GIF", "MENU.GIF", "TITLE.LST", "STARTER.FLI"};
  uint8_t *px = malloc(64000), *pal = malloc(768);
  for (uint32_t i = 0; i < rounds; i++) {
    uint32_t n; uint8_t *d;
    victim = files[below(5)];
    d = load(victim, &n); swap = mutate(d, n, &swap_n); free(d);
    if (!below(4) && strstr(victim, ".LST")) { /* long lines, many names, nested lists, BKDs */
      static const char *names[] = {"TITLE.LST", "MENU.GIF", "STARTER.FLI", "X.BKD", "AJGAMES.GIF", "GMTITLE2.GIF", "AVERYLONGNAMEINDEED.GIF", ""};
      free(swap); swap = malloc(4096); swap_n = 0;
      for (uint32_t k = below(60); k-- && swap_n < 4000;) swap_n += (uint32_t)snprintf((char *)swap + swap_n, 4096 - swap_n, "%s%s %u\r\n", below(3) ? "" : "   ", names[below(8)], below(2) ? rnd() : below(100));
    }
    gm_title_picture(reader, dice, "DICE.GAM", px, pal);
    if (!below(4)) gm_thumb_put(below(300), px, pal);
    gm_thumb_get(below(300));
    free(swap); swap = 0; victim = 0;
  }
  free(px); free(pal);
}
/* Every game in the folders: its title from the .GAM, its title screen, a card and both views drawn. */
static unsigned real_games(int argc, char **argv) {
  static char path[1024], gam[64], title[32];
  static uint8_t head[512], px[64000], pal[768];
  static uint32_t frame[640 * 480];
  struct gm_menu *m = calloc(1, sizeof *m);
  for (int a = 0; a < argc; a++) {
    DIR *top = opendir(argv[a]); struct dirent *e;
    if (!top) {fprintf(stderr, "fuzz: no folder %s\n", argv[a]); exit(2);}
    while ((e = readdir(top))) {
      if (e->d_name[0] == '.') continue;
      snprintf(path, sizeof path, "%s/%s", argv[a], e->d_name);
      DIR *g = opendir(path); struct dirent *f; gam[0] = 0;
      if (!g) continue;
      while ((f = readdir(g))) {size_t n = strlen(f->d_name); if (n > 4 && n < sizeof gam && !strcasecmp(f->d_name + n - 4, ".gam")) {memcpy(gam, f->d_name, n + 1); break;}}
      closedir(g);
      if (!gam[0]) continue;
      int32_t n = host_read(path, gam, head, sizeof head);
      if (n < 0 || gm_gam_title(head, (uint32_t)n, title) < 0) title[0] = 0;
      unsigned id = m->count;
      if (!gm_add(m, e->d_name, gam, title, 0)) continue;
      int r = gm_title_picture(host_read, path, gam, px, pal);
      m->games[id].pic = r ? -1 : 1;
      if (!r) gm_thumb_put(id, px, pal);
    }
    closedir(top);
  }
  gm_finish(m);
  for (unsigned t = 0; t < GM_CATS; t++) {
    m->tab = t;
    for (unsigned k = 0; k <= m->tab_count[t]; k++) {gm_draw(frame, 640, m, 0, 0); gm_event(m, 1, 106, 1);}
    m->view = GM_DETAILS; gm_draw(frame, 640, m, px, pal); m->view = GM_GRID;
  }
  unsigned count = m->count; free(m);
  return count;
}
int main(int argc, char **argv) {
  if (argc < 2) {fprintf(stderr, "usage: fuzz <games folder> [rounds]\n"); return 2;}
  snprintf(dice, sizeof dice, "%s/DICE", argv[1]);
  uint32_t rounds = argc > 2 ? (uint32_t)atoi(argv[2]) : 300;
  fuzz_library(rounds); puts("fuzz: library index lines ok");
  fuzz_menu(rounds / 4 + 1); puts("fuzz: games list and drawing ok");
  fuzz_pictures(rounds); puts("fuzz: GIF and FLI ok");
  fuzz_title(rounds); puts("fuzz: title pictures ok");
  if (argc > 3) printf("fuzz: %u real games listed, pictured and drawn ok\n", real_games(argc - 3, argv + 3));
  return 0;
}
