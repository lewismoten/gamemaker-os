/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * hisload.h. Claude Sonnet 5.5 wrote this file. */
#include "hisload.h"
#include <string.h>
int hl_stem(const char *gam, char out[9]) {
  unsigned n = 0;
  for (; gam[n] && gam[n] != '.'; n++) {
    char c = gam[n]; if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (n >= 8 || !((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return -1;
    out[n] = c;
  }
  if (!n) return -1;
  out[n] = 0; return 0;
}
/* <GAME> folder key: the first 4 letters of the stem and 4 hex digits of a hash of the game's folder and .GAM
   name, so two START.GAMs in different folders keep separate Top Tens. */
void hl_key(const char *stem, const char *folder, const char *gam, char out[9]) {
  uint32_t h = 2166136261u; const char *parts[3] = {folder, "/", gam};
  for (int i = 0; i < 3; i++) for (const char *c = parts[i]; *c; c++) h = (h ^ (uint8_t)*c) * 16777619u;
  h ^= h >> 16;
  unsigned n = 0; for (; stem[n] && n < 4; n++) out[n] = stem[n];
  for (int i = 3; i >= 0; i--) out[n++] = "0123456789ABCDEF"[(h >> (4 * i)) & 15];
  out[n] = 0;
}
static void his_file(char out[16], const char *game) {
  unsigned n = 0; while (game[n]) {out[n] = game[n]; n++;}
  out[n++] = '.'; out[n++] = 'H'; out[n++] = 'I'; out[n++] = 'S'; out[n] = 0;
}
int hl_load(const struct save_ops *o, const char *key, const char *stem, gp_read_fn read, void *user, struct his_table *t, int *seeded) {
  uint8_t raw[HIS_BYTES]; uint32_t n = 0; char f[16]; his_file(f, stem); *seeded = 0;
  int e = save_get(o, key, f, raw, sizeof raw, &n);
  /* a Top Ten saved before the folder carried a hash: /OS/SAVES/<STEM>/<STEM>.HIS (kept; the next save writes the new place) */
  if (e == SAVE_ENOENT && strcmp(key, stem)) e = save_get(o, stem, f, raw, sizeof raw, &n);
  if (e == SAVE_EFBIG) e = 0, n = HIS_BYTES;
  if (!e) {his_parse(t, raw, n); return 0;}
  n = 0;
  if (read) {int32_t got = read(user, f, raw, sizeof raw); if (got > 0) {n = (uint32_t)got; *seeded = 1;}}
  his_parse(t, raw, n);
  return e == SAVE_ENOENT ? 0 : e;
}
int hl_save(const struct save_ops *o, const char *key, const char *stem, const struct his_table *t) {
  uint8_t out[HIS_BYTES]; char f[16]; his_file(f, stem); his_serialize(t, out);
  return save_put(o, key, f, out, sizeof out);
}
