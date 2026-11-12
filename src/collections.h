/* SPDX-License-Identifier: MIT; bounded library lists, independent of board I/O. */
#pragma once
#include "savefile.h"
#define GM_FAVORITES_MAX 32u
#define GM_RECENT_MAX 20u
struct gm_list {unsigned count; char key[GM_FAVORITES_MAX][9];};
int gm_list_has(const struct gm_list *l, const char *key);
/* Newest first; toggles never evict a favorite. */
int gm_list_toggle(struct gm_list *l, const char *key);
int gm_list_recent(struct gm_list *l, const char *key);
int gm_list_load(struct gm_list *l, const struct save_ops *o, int recent);
int gm_list_save(const struct gm_list *l, const struct save_ops *o, int recent);
