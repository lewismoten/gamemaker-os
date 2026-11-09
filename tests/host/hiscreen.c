/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten. Claude Sonnet 5.5.
 * The Top Ten flow (hiscreen.c, hisload.c) the way PLAYGAME runs it: a score that places gets
 * the name entry, then the list; the card's copy is read first, the game's own .HIS seeds
 * it, and only /OS/SAVES is written. With a folder as argv[1] it also writes the screens as PPM. */
#include "hiscreen.h"
#include "hisload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MS 1000000ull
static struct {char path[48]; uint8_t d[400]; uint32_t n; int used;} files[4];
static uint64_t wfd; static char wpath[48]; static uint8_t wbuf[400]; static uint32_t wn;
static int rd_i;
static int o_ow(void *c, const char *p, uint64_t *fd) {(void)c; assert(!strncmp(p, "/sd/OS/SAVES/", 13)); strcpy(wpath, p); wn = 0; *fd = ++wfd; return 0;}
static int o_w(void *c, uint64_t fd, const void *p, uint32_t n, uint32_t *d) {(void)c; (void)fd; memcpy(wbuf + wn, p, n); wn += n; *d = n; return 0;}
static int o_sync(void *c, uint64_t fd) {(void)c; (void)fd; int i = 0; while (i < 4 && files[i].used && strcmp(files[i].path, wpath)) i++; assert(i < 4); files[i].used = 1; strcpy(files[i].path, wpath); memcpy(files[i].d, wbuf, wn); files[i].n = wn; return 0;}
static int o_close(void *c, uint64_t fd) {(void)c; (void)fd; return 0;}
static int o_ren(void *c, const char *a, const char *b) { /* the staged name moves over the real one */
  (void)c; int s = -1, d = -1;
  for (int i = 0; i < 4; i++) if (files[i].used) {if (!strcmp(files[i].path, a)) s = i; if (!strcmp(files[i].path, b)) d = i;}
  assert(s >= 0); if (d < 0) {for (d = 0; files[d].used && d < 4; d++) {} assert(d < 4); files[d].used = 1; strcpy(files[d].path, b);}
  memcpy(files[d].d, files[s].d, files[s].n); files[d].n = files[s].n; files[s].used = 0; return 0;
}
static uint32_t rpos;
static int o_or(void *c, const char *p, uint64_t *fd) {(void)c; for (int i = 0; i < 4; i++) if (files[i].used && !strcmp(files[i].path, p)) {rd_i = i; rpos = 0; *fd = 5; return 0;} return -2;}
static int o_rd(void *c, uint64_t fd, void *p, uint32_t n, uint32_t *got) {(void)c; (void)fd; uint32_t left = files[rd_i].n - rpos; if (n > left) n = left; memcpy(p, files[rd_i].d + rpos, n); rpos += n; *got = n; return 0;}
static const struct save_ops OPS = {0, o_ow, o_w, o_sync, o_close, o_ren, o_or, o_rd};
static uint8_t dos_his[HIS_BYTES]; static int dos_reads, dos_has;
static int32_t game_read(void *u, const char *name, uint8_t *buf, uint32_t cap) {
  (void)u; dos_reads++; if (!dos_has || strcmp(name, "TIN.HIS")) return -1;
  uint32_t n = cap < HIS_BYTES ? cap : HIS_BYTES; memcpy(buf, dos_his, n); return (int32_t)n;
}
static uint64_t now = 1000 * MS;
static int key(struct hs_screen *h, uint16_t code) {int a = hs_event(h, 1, code, 1, now); hs_event(h, 1, code, 0, now); now += 50 * MS; return a;}
static void type(struct hs_screen *h, const char *s) {
  static const char row[] = "qwertyuiop", row2[] = "asdfghjkl", row3[] = "zxcvbnm";
  for (; *s; s++) {
    char c = *s, l = c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; uint16_t code = 0;
    for (int i = 0; row[i]; i++) if (row[i] == l) code = (uint16_t)(16 + i);
    for (int i = 0; row2[i]; i++) if (row2[i] == l) code = (uint16_t)(30 + i);
    for (int i = 0; row3[i]; i++) if (row3[i] == l) code = (uint16_t)(44 + i);
    if (l == ' ') code = 57;
    if (l >= '1' && l <= '9') code = (uint16_t)(2 + l - '1');
    assert(code);
    if (c != l) hs_event(h, 1, 42, 1, now);
    key(h, code);
    if (c != l) hs_event(h, 1, 42, 0, now);
  }
}
static void ppm(const char *dir, const char *name, const uint8_t *px, const uint8_t *pal) {
  if (!dir) return;
  char p[300]; snprintf(p, sizeof p, "%s/%s.ppm", dir, name); FILE *f = fopen(p, "wb"); assert(f);
  fprintf(f, "P6\n%u %u\n255\n", HS_W * 2, HS_H * 2);
  for (unsigned y = 0; y < HS_H * 2; y++) for (unsigned x = 0; x < HS_W * 2; x++) {
    const uint8_t *c = pal + 3 * px[y / 2 * HS_W + x / 2]; uint8_t o[3];
    for (int k = 0; k < 3; k++) {unsigned v = c[k] & 63; o[k] = (uint8_t)(v << 2 | v >> 4);}
    fwrite(o, 1, 3, f);
  }
  fclose(f);
}
static void table(struct his_table *t) {
  const char *names[] = {"Gwinn", "Teddy", "Doc Leo", "Kit"}; int32_t sc[] = {9000, 7500, 4200, 300};
  memset(t, 0, sizeof *t);
  for (int i = 0; i < 4; i++) {t->row[i].score = sc[i]; strcpy((char *)t->row[i].name, names[i]);}
}
int main(int argc, char **argv) {
  const char *out = argc > 1 ? argv[1] : 0;
  static uint8_t px[HS_W * HS_H], pal[768], back[HS_W * HS_H], bpal[768];
  struct hs_screen h; struct his_table t;
  /* keys */
  assert(hs_ascii(30, 0) == 'a' && hs_ascii(30, 1) == 'A' && hs_ascii(57, 0) == ' ' && hs_ascii(2, 1) == '!' && hs_ascii(1, 0) == 0);
  /* a score that places: name entry like GGet, then the list */
  table(&t); hs_start(&h, &t, 5000, now);
  assert(h.stage == HS_NAME && h.place == 3);
  hs_draw(&h, px, pal, 0, 0); ppm(out, "name-empty", px, pal);
  type(&h, "Doc Leo"); assert(!strcmp(h.name, "Doc Leo"));
  key(&h, 14); key(&h, 14); assert(!strcmp(h.name, "Doc L")); /* Backspace */
  key(&h, 105); assert(!strcmp(h.name, "Doc ")); /* Left erases too, as GGet */
  type(&h, "Leo"); key(&h, 106); /* Right does nothing */
  assert(!strcmp(h.name, "Doc Leo") && h.stage == HS_NAME);
  hs_draw(&h, px, pal, 0, 0); ppm(out, "name-typing", px, pal);
  int a = key(&h, 28);
  assert((a & HS_COMMIT) && h.stage == HS_TABLE && !(a & HS_DONE));
  assert(h.t.row[2].score == 5000 && !strcmp((char *)h.t.row[2].name, "Doc Leo") && h.t.row[3].score == 4200 && h.t.row[4].score == 300);
  assert(hl_save(&OPS, "TIN", &h.t) == 0);
  hs_draw(&h, px, pal, 0, 0); ppm(out, "top-ten-after-entry", px, pal);
  assert(hs_step(&h, now + 29 * 1000 * MS) == 0 && (hs_step(&h, now + 31 * 1000 * MS) & HS_DONE)); /* 30 seconds */
  assert(key(&h, 1) & HS_DONE); /* any key, Esc too */
  /* Esc accepts the name as typed; an empty name is allowed (GGet returns on either) */
  table(&t); hs_start(&h, &t, 100, now); type(&h, "Kit"); a = key(&h, 1);
  assert((a & HS_COMMIT) && !strcmp((char *)h.t.row[4].name, "Kit") && h.t.row[4].score == 100);
  table(&t); hs_start(&h, &t, 10000, now); a = key(&h, 28);
  assert((a & HS_COMMIT) && h.t.row[0].name[0] == 0 && h.t.row[0].score == 10000 && h.t.row[1].score == 9000);
  /* 22 characters and no more; a tie sits below the equal score */
  table(&t); hs_start(&h, &t, 9000, now); assert(h.place == 2);
  type(&h, "abcdefghijklmnopqrstuvwxyz"); assert(h.len == 22 && !strcmp(h.name, "abcdefghijklmnopqrstuv"));
  hs_draw(&h, px, pal, 0, 0); ppm(out, "name-full", px, pal);
  /* no place: no name entry; scores of 0 or less never place; the menu's list has no latest line */
  for (int i = 0; i < HIS_ROWS; i++) {t.row[i].score = 1000 - i; strcpy((char *)t.row[i].name, "Full");}
  hs_start(&h, &t, 500, now); assert(h.stage == HS_TABLE && h.place == 0);
  hs_start(&h, &t, 0, now); assert(h.stage == HS_TABLE);
  hs_start(&h, &t, -1, now); assert(h.stage == HS_TABLE);
  hs_draw(&h, px, pal, 0, 0); ppm(out, "top-ten-menu-full", px, pal);
  table(&t); hs_start(&h, &t, -1, now); hs_draw(&h, px, pal, 0, 0); ppm(out, "top-ten-menu", px, pal);
  /* the game's own picture behind it */
  for (unsigned i = 0; i < HS_W * HS_H; i++) back[i] = (uint8_t)(16 + (i / HS_W) / 8);
  for (unsigned i = 0; i < 256; i++) {bpal[3 * i] = (uint8_t)(i / 6); bpal[3 * i + 1] = (uint8_t)(i / 8); bpal[3 * i + 2] = (uint8_t)(20 + i / 8);}
  bpal[3 * 1] = 63; bpal[3 * 1 + 1] = 10; bpal[3 * 1 + 2] = 10; bpal[3 * 2] = 10; bpal[3 * 2 + 1] = 10; bpal[3 * 2 + 2] = 63;
  table(&t); hs_start(&h, &t, 5000, now); key(&h, 28); hs_draw(&h, px, pal, back, bpal); ppm(out, "top-ten-picture", px, pal);
  /* GGet's cursor keys: Right never moves (ans[ctr] is always 0), Left erases, extra Rights change nothing */
  table(&t); hs_start(&h, &t, 5000, now); key(&h, 106); assert(h.len == 0); type(&h, "ab");
  key(&h, 106); key(&h, 106); assert(!strcmp(h.name, "ab") && h.len == 2); key(&h, 105); key(&h, 106); type(&h, "c");
  assert(!strcmp(h.name, "ac")); key(&h, 105); key(&h, 105); key(&h, 105); assert(h.len == 0 && h.stage == HS_NAME);
  /* the accepted set: Tab and Ctrl bytes are typed, 0/8/13/27 never are */
  hs_start(&h, &t, 5000, now); key(&h, 15); assert(h.len == 1 && h.name[0] == 9);
  hs_event(&h, 1, 29, 1, now); key(&h, 30); hs_event(&h, 1, 29, 0, now); assert(h.len == 2 && h.name[1] == 1);
  hs_event(&h, 1, 97, 1, now); key(&h, 35); assert(h.len == 1); /* Ctrl+H is 8: Backspace */
  key(&h, 50); assert(h.stage == HS_TABLE); /* Ctrl+M is 13: accepted */
  hs_start(&h, &t, 5000, now); hs_event(&h, 1, 29, 1, now); key(&h, 26); assert(h.stage == HS_TABLE); /* Ctrl+[ is 27 */
  hs_start(&h, &t, 5000, now); hs_event(&h, 1, 29, 1, now); key(&h, 2); assert(h.len == 0); /* Ctrl+1: no byte */
  /* a Shift down when the entry begins (the kernel's key state) */
  { uint64_t keys[12] = {0}; keys[0] = 1ull << 42; hs_start(&h, &t, 5000, now); hs_modifiers(&h, keys); key(&h, 30);
    assert(!strcmp(h.name, "A")); memset(keys, 0, sizeof keys); keys[1] = 1ull << (97 - 64); hs_modifiers(&h, keys); assert(h.shift == 0 && h.ctrl == 1); }
  /* below ABI 1.7 nothing is saved: a placing score opens the list, other scores pass through */
  assert(hs_entry_score(5000, 0) == -1 && hs_entry_score(5000, 1) == 5000 && hs_entry_score(0, 0) == 0 && hs_entry_score(-1, 0) == -1);
  hs_start(&h, &t, hs_entry_score(5000, 0), now); assert(h.stage == HS_TABLE && h.place == 0);
  /* the cursor: on 500 ms after a key, off 400 ms, on again */
  table(&t); hs_start(&h, &t, 5000, now);
  assert(hs_step(&h, now + 499 * MS) == 0 && (hs_step(&h, now + 500 * MS) & HS_DRAW) && h.cur_on == 0);
  assert(hs_step(&h, now + 899 * MS) == 0 && (hs_step(&h, now + 900 * MS) & HS_DRAW) && h.cur_on == 1);
  /* the card: a card copy wins; else the game's own .HIS seeds it (read only); a save lands in /OS/SAVES */
  char stem[9]; assert(!hl_stem("tin.gam", stem) && !strcmp(stem, "TIN") && hl_stem("TOOLONGNAME.GAM", stem) && hl_stem(".GAM", stem) && hl_stem("A B.GAM", stem));
  memset(files, 0, sizeof files);
  table(&t); his_serialize(&t, dos_his); dos_has = 1;
  struct his_table got; int seeded;
  assert(hl_load(&OPS, "TIN", game_read, 0, &got, &seeded) == 0 && seeded == 1 && got.row[0].score == 9000 && !strcmp((char *)got.row[1].name, "Teddy"));
  for (int i = 0; i < 4; i++) assert(!files[i].used); /* loading wrote nothing */
  hs_start(&h, &got, 8000, now); type(&h, "Lewis"); key(&h, 28);
  assert(hl_save(&OPS, "TIN", &h.t) == 0);
  int live = 0, his = 0;
  for (int i = 0; i < 4; i++) if (files[i].used) {live++; his += !strcmp(files[i].path, "/sd/OS/SAVES/TIN/TIN.HIS") && files[i].n == HIS_BYTES;}
  assert(live == 1 && his == 1); /* the .TMP is renamed away; nothing else was written */
  dos_reads = 0; dos_has = 0; memset(dos_his, 0, sizeof dos_his);
  assert(hl_load(&OPS, "TIN", game_read, 0, &got, &seeded) == 0 && seeded == 0 && dos_reads == 0 && got.row[1].score == 8000 && !strcmp((char *)got.row[1].name, "Lewis") && got.row[2].score == 7500);
  memset(files, 0, sizeof files); /* neither: an empty table */
  assert(hl_load(&OPS, "TIN", game_read, 0, &got, &seeded) == 0 && seeded == 0 && got.row[0].score == 0);
  assert(hl_load(&OPS, "TIN", 0, 0, &got, &seeded) == 0 && seeded == 0);
  puts("ALL PASS");
  return 0;
}
