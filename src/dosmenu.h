/* SPDX-License-Identifier: MIT; PLAYGAME menu look, from XFERPLAY 3.00's patch
 * map and the web engine's player/patchTable.ts (MIT, Lewis Moten). */
#pragma once
#include "game.h"
enum {DM_MARKER_WIDTH, DM_PLAY_X, DM_PLAY_Y, DM_INSTRUCTIONS_X, DM_INSTRUCTIONS_Y, DM_STORYLINE_X, DM_STORYLINE_Y, DM_CREDITS_X, DM_CREDITS_Y, DM_HIGHSCORES_X, DM_HIGHSCORES_Y, DM_QUIT_X, DM_QUIT_Y, DM_TITLEX, DM_TITLEY, DM_MARKERX, DM_FIRSTY, DM_SPACING, DM_NOFILE_X, DM_NOFILE_Y, DM_NUMS};
struct dm_look {char words[9][40]; int num[DM_NUMS]; uint8_t rgb[7][3]; int color, message;};
void dm_defaults(struct dm_look *d);
/* PLAYER.JSON: web player {base:"xferplay-3.00",patches:{id:value}} record.
 * Invalid records leave defaults; unknown ids are ignored. */
int dm_json(struct dm_look *d, const uint8_t *data, uint32_t n);
/* Reads the game's named EXE, XFERPLAY.EXE or PLAYGAME.EXE, then PLAYER.JSON.
 * Only the recognized 3.00 transfer build is decoded. scratch is >= 160 KiB. */
void dm_load(struct dm_look *d, gp_read_fn read, void *user, const char *gam, uint8_t *scratch, uint32_t cap);
