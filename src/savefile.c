/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten. Claude Sonnet 5.5. */
#include "savefile.h"
#include <string.h>
static int ok_part(const char *s, unsigned max_base, int allow_dot) {
  unsigned base = 0, ext = 0; int dot = 0;
  for (; *s; s++) {
    char c = *s;
    if (c == '.') {if (!allow_dot || dot || !base) return 0; dot = 1; continue;}
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    if (dot ? ++ext > 3 : ++base > max_base) return 0;
  }
  return base && (!dot || ext);
}
/* "/sd/OS/SAVES/GAME/NAME" and, if tmp, the same stem with .TMP. */
static int make_path(char out[40], const char *game, const char *name, int tmp) {
  if (!ok_part(game, 8, 0) || !ok_part(name, 8, 1)) return SAVE_EINVAL;
  size_t n = 0, g = strlen(game), l = strlen(name);
  if (l >= 4 && !strcmp(name + l - 4, ".TMP")) return SAVE_EINVAL;
  memcpy(out, "/sd/OS/SAVES/", 13); n = 13;
  memcpy(out + n, game, g); n += g; out[n++] = '/';
  if (tmp) {const char *d = strchr(name, '.'); l = d ? (size_t)(d - name) : l; memcpy(out + n, name, l); n += l; memcpy(out + n, ".TMP", 4); n += 4;}
  else {memcpy(out + n, name, l); n += l;}
  out[n] = 0; return 0;
}
int save_put(const struct save_ops *o, const char *game, const char *name, const void *data, uint32_t n) {
  char dst[40], tmp[40]; int e;
  if ((e = make_path(dst, game, name, 0)) || (e = make_path(tmp, game, name, 1))) return e;
  if (!strcmp(dst, tmp)) return SAVE_EINVAL; /* the name's own stem.TMP; refuse rather than rename onto itself */
  uint64_t fd; if ((e = o->open_write(o->ctx, tmp, &fd))) return e;
  const uint8_t *p = data; uint32_t at = 0;
  while (at < n) {
    uint32_t done = 0; e = o->write(o->ctx, fd, p + at, n - at, &done);
    if (e || !done) {o->close(o->ctx, fd); return e ? e : -5;}
    at += done;
  }
  if ((e = o->sync(o->ctx, fd))) {o->close(o->ctx, fd); return e;}
  if ((e = o->close(o->ctx, fd))) return e;
  return o->rename(o->ctx, tmp, dst);
}
int save_get(const struct save_ops *o, const char *game, const char *name, void *buf, uint32_t cap, uint32_t *n) {
  char path[40]; int e; if ((e = make_path(path, game, name, 0))) return e;
  uint64_t fd; if ((e = o->open_read(o->ctx, path, &fd))) return e;
  uint8_t *p = buf; uint32_t at = 0; *n = 0;
  for (;;) {
    uint32_t got = 0;
    if (at < cap) e = o->read(o->ctx, fd, p + at, cap - at, &got);
    else {uint8_t extra; e = o->read(o->ctx, fd, &extra, 1, &got); if (!e && got) e = SAVE_EFBIG;}
    if (e || !got) {o->close(o->ctx, fd); if (!e) *n = at; return e;}
    at += got;
  }
}
static void his_name(char out[16], const char *game) {size_t g = strlen(game); memcpy(out, game, g); memcpy(out + g, ".HIS", 5);}
int save_his_load(const struct save_ops *o, const char *game, struct his_table *t) {
  char name[16]; uint8_t raw[HIS_BYTES]; uint32_t n = 0; his_name(name, game);
  int e = save_get(o, game, name, raw, sizeof raw, &n);
  if (e == SAVE_EFBIG) e = 0, n = HIS_BYTES; /* DOS reads the first ten records and ignores the rest */
  if (e == SAVE_ENOENT) {e = 0; n = 0;}
  if (e) return e;
  his_parse(t, raw, n); return 0;
}
int save_his_record(const struct save_ops *o, const char *game, int32_t score, const char *name, int *place) {
  struct his_table t; int e = save_his_load(o, game, &t); *place = 0; if (e) return e;
  int at = his_insert(&t, score, name); if (!at) return 0;
  uint8_t out[HIS_BYTES]; char f[16]; his_name(f, game); his_serialize(&t, out);
  if ((e = save_put(o, game, f, out, sizeof out))) return e;
  *place = at; return 0;
}
