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
