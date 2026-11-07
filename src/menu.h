/* SPDX-License-Identifier: MIT; the games list as data, and what input does
 * to it. Pure: no system calls, so the host tests drive it directly. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gam.h"
#define GM_MAX_GAMES 128u
#define GM_ROWS 9u /* rows visible at once */
#define GM_MESSAGE_MAX 48u
struct gm_game {char dir[9]; char title[GM_TITLE_MAX + 1];};
struct gm_menu {
  struct gm_game games[GM_MAX_GAMES];
  unsigned count, sel, top; /* top: first visible row */
  char message[GM_MESSAGE_MAX + 1]; /* footer line; empty shows the key help */
};
enum {GM_NONE, GM_MOVED, GM_CHOSEN, GM_QUIT, GM_RESYNC};
/* Add a game (dir is its folder's name); false when the list is full. */
bool gm_add(struct gm_menu *m, const char *dir, const char *title);
void gm_sort(struct gm_menu *m); /* by title, ignoring case; then by folder */
/* One input event (Linux codes): up/down move on press and repeat, Enter
   chooses, Esc or Q quits, SYN_DROPPED asks the caller to resynchronize. */
int gm_event(struct gm_menu *m, uint16_t type, uint16_t code, int32_t value);
