/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * Where a game's Top Ten comes from and goes: the card's own copy in
 * /OS/SAVES/<GAME>/<GAME>.HIS first; when there is none yet, the game's own
 * <GAME>.HIS in its folder (read only), so DOS scores carry over. Writes go only
 * to /OS/SAVES. Claude Sonnet 5.5 wrote this file. */
#pragma once
#include "game.h"
#include "savefile.h"
/* The 8.3 stem of a game's .GAM name, upper case, as <GAME>; 0, or -1 if it is no valid <GAME>. */
int hl_stem(const char *gam, char out[9]);
/* Fills t. *seeded is 1 when it came from the game's own .HIS. A card error other than
   "no file" is returned (t then holds the seed or zeros). */
int hl_load(const struct save_ops *o, const char *game, gp_read_fn read, void *user, struct his_table *t, int *seeded);
/* Writes t to /OS/SAVES/<GAME>/<GAME>.HIS. */
int hl_save(const struct save_ops *o, const char *game, const struct his_table *t);
