/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * Saves and high scores under /sd/OS/SAVES/<GAME>/, the only place the kernel
 * lets an app write besides its cache (sandbox-kernel docs/saves-storage.md).
 * Files keep DOS's own names and bytes (GAME.HIS, GAME0.SAV, SAVNAMES.DAT), so
 * a card's folder can be copied to a DOS game folder and back. A write goes to
 * NAME.TMP, is synced, then renamed over NAME.EXT: after a power cut the real
 * name holds the old file or the new one. I/O goes through save_ops so the host
 * tests can cut power after any step. Claude Sonnet 5.5 wrote this file. */
#pragma once
#include <stdint.h>
#include "hiscore.h"
#define SAVE_ENOENT (-2)
#define SAVE_EINVAL (-22)
#define SAVE_EFBIG (-27)
struct save_ops {
  void *ctx;
  int (*open_write)(void *ctx, const char *path, uint64_t *fd);   /* create or truncate, staged */
  int (*write)(void *ctx, uint64_t fd, const void *p, uint32_t n, uint32_t *done);
  int (*sync)(void *ctx, uint64_t fd);                            /* commit the staged bytes */
  int (*close)(void *ctx, uint64_t fd);
  int (*rename)(void *ctx, const char *from, const char *to);     /* same folder, replaces */
  int (*open_read)(void *ctx, const char *path, uint64_t *fd);
  int (*read)(void *ctx, uint64_t fd, void *p, uint32_t n, uint32_t *got);
};
/* The kernel-backed ops (src/savefile_sbx.c, board build only). */
extern const struct save_ops save_sbx_ops;
/* game: 1-8 of A-Z 0-9 _ -; name: a plain 8.3 name, not ending in .TMP.
   0 or a negative errno. Nothing is written for a bad name. */
int save_put(const struct save_ops *o, const char *game, const char *name, const void *data, uint32_t n);
/* Reads the whole file into buf (cap bytes); *n is its size. SAVE_ENOENT if absent. */
int save_get(const struct save_ops *o, const char *game, const char *name, void *buf, uint32_t cap, uint32_t *n);
/* GAME.HIS: a missing file is an empty table; any other failure is returned. */
int save_his_load(const struct save_ops *o, const char *game, struct his_table *t);
/* Insert and write if the score places: *place is the 1-based row or 0 (nothing written). */
int save_his_record(const struct save_ops *o, const char *game, int32_t score, const char *name, int *place);

/* GAME0.SAV: pure DOS 3.00 wire codec, NOT a live engine snapshot bridge.
 * Public PLAYGAME.C (RSD, MIT) writes packed 16-bit int/32-bit long/far-pointer
 * records. Host structs are never copied onto the wire. See docs/game-save.md.
 * Hermes OpenAI Codex GPT-6.1 Sol wrote this codec and its tests. */
#define SAVE_GAME_BLOCKS 150
#define SAVE_GAME_MONSTERS 200
#define SAVE_GAME_RECORDS 1000
#define SAVE_GAME_MIN_BYTES 37755u
#define SAVE_GAME_MAX_BYTES 41505u
struct save_game_block { uint8_t block; uint32_t next_time; };
struct save_game_record { uint16_t delta_time; uint8_t key; };
struct save_game {
  uint8_t block_count;
  int16_t prevscene, doscene, linkin;
  uint32_t sound_clock, next_change[SAVE_GAME_BLOCKS];
  uint8_t block_map[SAVE_GAME_BLOCKS];
  /* Complete DOS object images, including otherwise unused bytes and pointer
   * tokens. They are opaque DATA, never cast to host structs or dereferenced.
   * A future state bridge must decode and validate the fields it actually uses. */
  uint8_t character_info[135], character[3784];
  int16_t map_x, map_y;
  uint8_t monster_info[SAVE_GAME_MONSTERS][55];
  uint8_t monster_map[SAVE_GAME_MONSTERS][10];
  uint8_t map[100][100][2]; /* [y][x]: block, monster slot */
  uint8_t monster_count;
  struct save_game_block blocks[SAVE_GAME_BLOCKS];
  int16_t record_count, last_recorded_key;
  uint8_t record_file[51]; /* may lack a NUL; never a host path */
  uint32_t record_pointer, record_timer; /* pointer is a DOS token only */
  uint8_t record_flag;
  uint32_t last_key_press;
  uint16_t record_paused; /* DOS boolean enum occupies two bytes */
  struct save_game_record records[SAVE_GAME_RECORDS];
};
/* Zero if counts are outside the reachable DOS limits, otherwise exact size. */
uint32_t save_game_size(const struct save_game *g);
/* Strict whole-file framing: reject short files, trailers, block_count >150 or
 * record_count outside 0..1000. Other DOS bytes are preserved, not normalized.
 * No allocations, I/O, global state, native pointer restoration or gameplay
 * validation. Caller owns ~43 KiB state and wire storage; do not put on a small
 * board stack. Object/wire/output-size storage must not overlap (EINVAL).
 * On error all output bytes, including *n, remain unchanged. Unused capacity
 * in decoded variable arrays is zeroed on success. Unaligned wire is supported.
 * 0 on success; SAVE_EINVAL for invalid arguments/format; SAVE_EFBIG if cap is
 * too small. n is required and set only after successful encode. */
int save_game_decode(struct save_game *g, const void *wire, uint32_t n);
int save_game_encode(const struct save_game *g, void *wire, uint32_t cap, uint32_t *n);
