/* SPDX-License-Identifier: MIT; the games list as data, and what input does
 * to it. It follows the Sandbox Edition web site's Games page: category tabs
 * (Starters, Sandbox, Bizarre, CD, and Other for games the library index does
 * not know), a grid of game cards six to a page, and a details view with Play.
 * Pure: no system calls, so the host tests drive it directly. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gam.h"
#include "library.h"
#define GM_MAX_GAMES 256u
#define GM_COLS 3u
#define GM_PAGE 6u /* cards on a page: three across, two down */
#define GM_MESSAGE_MAX 48u
enum {GM_CAT_STARTER, GM_CAT_SANDBOX, GM_CAT_BIZARRE, GM_CAT_CD, GM_CAT_OTHER, GM_CATS};
enum {GM_GRID, GM_DETAILS};       /* what shows */
enum {GM_BTN_PLAY, GM_BTN_BACK, GM_BUTTONS}; /* the details view's buttons */
struct gm_game {
  char dir[9], gam[13];             /* folder and its .GAM file */
  char title[GM_LIB_TITLE + 1];     /* the index's title, else the .GAM's, else the folder */
  char company[GM_LIB_COMPANY + 1], blurb[GM_LIB_BLURB + 1];
  uint32_t crc; int32_t year, course;
  uint8_t cat; int8_t pic;          /* pic: 0 not looked for yet, 1 found, -1 none */
};
struct gm_menu {
  struct gm_game games[GM_MAX_GAMES];
  unsigned count;
  uint16_t order[GM_MAX_GAMES];      /* games by tab, then by course and title */
  unsigned tab_first[GM_CATS], tab_count[GM_CATS];
  unsigned tab, view, button;
  unsigned pos[GM_CATS];            /* the selected card in each tab (remembered) */
  int shift;                        /* a Shift key is held (Shift+Tab) */
  char message[GM_MESSAGE_MAX + 1]; /* footer line; empty shows the key help */
};
enum {GM_NONE, GM_MOVED, GM_CHOSEN, GM_QUIT, GM_RESYNC};
/* Add a game (dir is its folder's name, gam its .GAM file, title from that
   file, crc the .GAM's CRC-32); false when the list is full. */
bool gm_add(struct gm_menu *m, const char *dir, const char *gam, const char *title, uint32_t crc);
/* One line of the library index: fills in the game it describes (matched by
   the .GAM's CRC-32, else its locator or DOS name). 1 when a game matched. */
int gm_library_line(struct gm_menu *m, const char *line, uint32_t n);
/* Sorts the games into their tabs and selects the first card of the Sandbox
   tab (the web page's default), or of the first tab that has games. */
void gm_finish(struct gm_menu *m);
/* One input event (Linux codes). Grid: arrows move, PgUp/PgDn turn the page,
   Home/End, Tab and Shift+Tab (or [ and ]) change the tab, Enter opens the
   details, Esc or Q quits. Details: Left/Right/Tab pick a button, Enter or
   Space presses it (Play chooses the game), Esc goes back. SYN_DROPPED asks
   the caller to resynchronize. */
int gm_event(struct gm_menu *m, uint16_t type, uint16_t code, int32_t value);
/* The selected game (0 when the list is empty), and the cards of a tab. */
const struct gm_game *gm_selected(const struct gm_menu *m);
unsigned gm_tab_size(const struct gm_menu *m, unsigned tab);
const struct gm_game *gm_tab_game(const struct gm_menu *m, unsigned tab, unsigned i);
/* A folder of /sd/GAMES the list can hold: a DOS name, 1 to 8 characters (a
   longer one would not fit the game's dir[9] or the board's path buffers). */
bool gm_dir_ok(const char *name, unsigned n);
unsigned gm_menu_bytes(void); /* for the host tests, which keep the menu as bytes */
