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
  if (tmp) {l = 0; while (name[l] && name[l] != '.') l++; memcpy(out + n, name, l); n += l; memcpy(out + n, ".TMP", 4); n += 4;}
  else {memcpy(out + n, name, l); n += l;}
  out[n] = 0; return 0;
}
int save_put(const struct save_ops *o, const char *game, const char *name, const void *data, uint32_t n) {
  char dst[40], tmp[40]; int e;
  if ((e = make_path(dst, game, name, 0)) || (e = make_path(tmp, game, name, 1))) return e;
  if (!strcmp(dst, tmp)) return SAVE_EINVAL; /* the name's own stem.TMP; refuse rather than rename onto itself */
  uint64_t fd; if ((e = o->open_write(o->ctx, tmp, &fd))) return e; /* truncates a stale NAME.TMP left by a power cut */
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
  /* A NAME.TMP left by a power cut is never read; if the real file is missing the load
     reports that. The kernel offers no delete to apps, so the next save_put overwrites it. */
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
static int his_name(char out[16], const char *game) {size_t g = strlen(game); if (g > 11) return SAVE_EINVAL; memcpy(out, game, g); memcpy(out + g, ".HIS", 5); return 0;}
int save_his_load(const struct save_ops *o, const char *game, struct his_table *t) {
  char name[16]; uint8_t raw[HIS_BYTES]; uint32_t n = 0; if (his_name(name, game)) return SAVE_EINVAL;
  int e = save_get(o, game, name, raw, sizeof raw, &n);
  if (e == SAVE_EFBIG) e = 0, n = HIS_BYTES; /* DOS reads the first ten records and ignores the rest */
  if (e == SAVE_ENOENT) {e = 0; n = 0;}
  if (e) return e;
  his_parse(t, raw, n); return 0;
}
int save_his_record(const struct save_ops *o, const char *game, int32_t score, const char *name, int *place) {
  struct his_table t; int e = save_his_load(o, game, &t); *place = 0; if (e) return e;
  int at = his_insert(&t, score, name); if (!at) return 0;
  uint8_t out[HIS_BYTES]; char f[16]; if (his_name(f, game)) return SAVE_EINVAL; his_serialize(&t, out);
  if ((e = save_put(o, game, f, out, sizeof out))) return e;
  *place = at; return 0;
}

/* DOS PLAYGAME 3.00 wire records (RSD, MIT); see docs/game-save.md.
 * Hermes OpenAI Codex GPT-6.1 Sol. No native struct/pointer serialization. */
static uint16_t game_u16(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
}
static int16_t game_i16(const uint8_t *p) {
  uint16_t v = game_u16(p);
  return (int16_t)(v <= 32767u ? (int32_t)v : (int32_t)v - 65536);
}
static uint32_t game_u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
         (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void game_put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void game_put32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
/* Subtraction avoids address-end overflow and unrelated-pointer subtraction. */
static int game_overlap(const void *a, size_t an, const void *b, size_t bn) {
  uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
  return an && bn && (x <= y ? y - x < an : x - y < bn);
}
uint32_t save_game_size(const struct save_game *g) {
  if (!g || g->block_count > SAVE_GAME_BLOCKS || g->record_count < 0 ||
      g->record_count > SAVE_GAME_RECORDS) return 0;
  return SAVE_GAME_MIN_BYTES + 5u * g->block_count + 3u * (uint32_t)g->record_count;
}
int save_game_decode(struct save_game *g, const void *wire, uint32_t n) {
  const uint8_t *p = wire;
  if (!g || !p || n < SAVE_GAME_MIN_BYTES || n > SAVE_GAME_MAX_BYTES ||
      game_overlap(g, sizeof *g, p, n)) return SAVE_EINVAL;
  unsigned blocks = p[0];
  if (blocks > SAVE_GAME_BLOCKS) return SAVE_EINVAL;
  /* All variable-array bounds and whole-file framing are checked before any
   * destination mutation. Recording starts after 37685 + 5M bytes. */
  uint32_t rec = 37685u + 5u * blocks;
  if (n < rec + 70u) return SAVE_EINVAL;
  int16_t records = game_i16(p + rec);
  if (records < 0 || records > SAVE_GAME_RECORDS ||
      n != rec + 70u + 3u * (uint32_t)records) return SAVE_EINVAL;
  memset(g, 0, sizeof *g);
  g->block_count = *p++;
  g->prevscene = game_i16(p); p += 2;
  g->doscene = game_i16(p); p += 2;
  g->linkin = game_i16(p); p += 2;
  g->sound_clock = game_u32(p); p += 4;
  for (unsigned i = 0; i < SAVE_GAME_BLOCKS; i++, p += 4) g->next_change[i] = game_u32(p);
#define GAME_READ_BYTES(member) do { memcpy(g->member, p, sizeof g->member); p += sizeof g->member; } while (0)
  GAME_READ_BYTES(block_map);
  GAME_READ_BYTES(character_info);
  GAME_READ_BYTES(character);
  g->map_x = game_i16(p); p += 2;
  g->map_y = game_i16(p); p += 2;
  GAME_READ_BYTES(monster_info);
  GAME_READ_BYTES(monster_map);
  GAME_READ_BYTES(map);
  g->monster_count = *p++;
  for (unsigned i = 0; i < blocks; i++) {
    g->blocks[i].block = *p++; g->blocks[i].next_time = game_u32(p); p += 4;
  }
  g->record_count = game_i16(p); p += 2;
  g->last_recorded_key = game_i16(p); p += 2;
  GAME_READ_BYTES(record_file);
  g->record_pointer = game_u32(p); p += 4;
  g->record_timer = game_u32(p); p += 4;
  g->record_flag = *p++;
  g->last_key_press = game_u32(p); p += 4;
  g->record_paused = game_u16(p); p += 2;
  for (int i = 0; i < records; i++) {
    g->records[i].delta_time = game_u16(p); p += 2; g->records[i].key = *p++;
  }
#undef GAME_READ_BYTES
  return 0;
}
int save_game_encode(const struct save_game *g, void *wire, uint32_t cap, uint32_t *n) {
  uint8_t *p = wire;
  uint32_t size = save_game_size(g);
  if (!size || !p || !n || game_overlap(g, sizeof *g, p, cap) ||
      game_overlap(g, sizeof *g, n, sizeof *n) ||
      game_overlap(p, cap, n, sizeof *n)) return SAVE_EINVAL;
  if (cap < size) return SAVE_EFBIG;
  *p++ = g->block_count;
  game_put16(p, (uint16_t)g->prevscene); p += 2;
  game_put16(p, (uint16_t)g->doscene); p += 2;
  game_put16(p, (uint16_t)g->linkin); p += 2;
  game_put32(p, g->sound_clock); p += 4;
  for (unsigned i = 0; i < SAVE_GAME_BLOCKS; i++, p += 4) game_put32(p, g->next_change[i]);
#define GAME_WRITE_BYTES(member) do { memcpy(p, g->member, sizeof g->member); p += sizeof g->member; } while (0)
  GAME_WRITE_BYTES(block_map);
  GAME_WRITE_BYTES(character_info);
  GAME_WRITE_BYTES(character);
  game_put16(p, (uint16_t)g->map_x); p += 2;
  game_put16(p, (uint16_t)g->map_y); p += 2;
  GAME_WRITE_BYTES(monster_info);
  GAME_WRITE_BYTES(monster_map);
  GAME_WRITE_BYTES(map);
  *p++ = g->monster_count;
  for (unsigned i = 0; i < g->block_count; i++) {
    *p++ = g->blocks[i].block; game_put32(p, g->blocks[i].next_time); p += 4;
  }
  game_put16(p, (uint16_t)g->record_count); p += 2;
  game_put16(p, (uint16_t)g->last_recorded_key); p += 2;
  GAME_WRITE_BYTES(record_file);
  game_put32(p, g->record_pointer); p += 4;
  game_put32(p, g->record_timer); p += 4;
  *p++ = g->record_flag;
  game_put32(p, g->last_key_press); p += 4;
  game_put16(p, g->record_paused); p += 2;
  for (int i = 0; i < g->record_count; i++) {
    game_put16(p, g->records[i].delta_time); p += 2; *p++ = g->records[i].key;
  }
#undef GAME_WRITE_BYTES
  *n = size;
  return 0;
}
