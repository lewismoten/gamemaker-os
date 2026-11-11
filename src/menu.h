/* SPDX-License-Identifier: MIT; the games list as data, and what input does
 * to it. It follows the Sandbox Edition web site's Games page: category tabs
 * (Starters, Sandbox, Bizarre, CD, and Other for games the library index does
 * not know), a grid of game cards eight to a page, and a details view with Play.
 * Pure: no system calls, so the host tests drive it directly. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gam.h"
#include "library.h"
#include "collections.h"
#include "settings.h"
#define GM_MAX_GAMES 256u
#define GM_COLS 4u
#define GM_PAGE 8u /* cards on a page: four across, two down */
#define GM_MESSAGE_MAX 48u
#include "osk.h"
#define GM_SEARCH_KEYS OSK_KEYS
enum {GM_CAT_STARTER, GM_CAT_SANDBOX, GM_CAT_BIZARRE, GM_CAT_CD, GM_CAT_OTHER, GM_CAT_SEARCH, GM_CAT_FAVORITES, GM_CAT_RECENT, GM_CATS};
enum {GM_GRID, GM_DETAILS};       /* what shows */
enum {GM_BTN_PLAY, GM_BTN_BACK, GM_BTN_FAVORITE, GM_BTN_SCORES, GM_BTN_MUSIC, GM_BUTTONS}; /* the details view's buttons */
struct gm_game {
  char dir[9], gam[13], key[9];     /* folder, .GAM file and saved-list identity */
  char title[GM_LIB_TITLE + 1];     /* the index's title, else the .GAM's, else the folder */
  char company[GM_LIB_COMPANY + 1], blurb[GM_LIB_BLURB + 1];
  uint32_t crc; int32_t year, course;
  uint8_t cat; int8_t pic;          /* pic: 0 not looked for yet, 1 found, -1 none */
};
enum {GM_OPT_BACK, GM_OPT_FAVORITE, GM_OPT_SCORES, GM_OPT_MUSIC, GM_OPT_OPEN, GM_OPT_QUIT, GM_OPT_SCREEN, GM_OPT_POWER, GM_OPTIONS};
struct gm_menu {
  struct gm_game games[GM_MAX_GAMES];
  unsigned count;
  uint16_t order[GM_MAX_GAMES];      /* by category, then course and title */
  uint16_t visible[GM_CATS][GM_MAX_GAMES];
  struct gm_list favorites, recent;
  char query[25];
  int search_active, tabs_focus, search_keyboard;
  unsigned keyboard_pos;
  unsigned detail_id;              /* stable while a virtual tab changes */
  unsigned tab_count[GM_CATS];
  unsigned tab, view, button;
  int background_music;             /* owner setting: on by default, M toggles in the library */
  struct gm_settings settings;
  unsigned pos[GM_CATS];            /* the selected card in each tab (remembered) */
  int shift;                        /* a Shift key is held (Shift+Tab) */
  int options_active, open_keyboard; unsigned option;
  int open_active;                  /* slash starts a folder-name request */
  char open_dir[10];                /* ninth character makes an overlong name fail */
  char message[GM_MESSAGE_MAX + 1]; /* footer line; empty shows the key help */
};
enum {GM_NONE, GM_MOVED, GM_CHOSEN, GM_QUIT, GM_RESYNC, GM_SCORES, GM_MUSIC, GM_OPEN_FOUND, GM_OPEN_MISSING, GM_OPEN_PLAY, GM_FAVORITE, GM_SETTINGS}; /* GM_SCORES: H, the selected game's Top Ten */
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
   Home/End, Tab and Shift+Tab (or [ and ]) change the tab. Up from the
   top row focuses tabs (Left/Right, Enter/Down). Search accepts up to 24
   characters, filters title/company live; Esc clears then leaves. F toggles
   Favorite in grid/details. Enter opens the
   details, Q quits (Esc opens Options in the library), H shows the selected game's Top Ten. Details: Left/Right/Tab pick a button, Enter or
   Space presses it (Play chooses the game), H shows the Top Ten, Esc goes back. SYN_DROPPED asks
   the caller to resynchronize. Slash starts/resets a DOS folder request in
   either view: letters/digits/-/_ type, Backspace edits, Esc cancels, Enter
   opens details (Shift+Enter also presses Play). GM_OPEN_* lets the caller log. */
int gm_event(struct gm_menu *m, uint16_t type, uint16_t code, int32_t value);
/* Exact folder alias, ignoring case; selects its tab/page and opens details.
   Returns GM_OPEN_FOUND or GM_OPEN_MISSING; a miss preserves selection. */
int gm_open_dir(struct gm_menu *m, const char *dir);
/* The selected game (0 when the list is empty), and the cards of a tab. */
const struct gm_game *gm_selected(const struct gm_menu *m);
unsigned gm_tab_size(const struct gm_menu *m, unsigned tab);
const struct gm_game *gm_tab_game(const struct gm_menu *m, unsigned tab, unsigned i);
/* A folder of /sd/GAMES the list can hold: a DOS name, 1 to 8 characters (a
   longer one would not fit the game's dir[9] or the board's path buffers). */
bool gm_dir_ok(const char *name, unsigned n);
unsigned gm_menu_bytes(void); /* for the host tests, which keep the menu as bytes */

/* Refresh virtual tabs after lists or query change; never opens game assets. */
void gm_refresh(struct gm_menu *m);
void gm_search(struct gm_menu *m, const char *query);
int gm_favorite(struct gm_menu *m);
int gm_played(struct gm_menu *m, unsigned id);
int gm_is_favorite(const struct gm_menu *m, const struct gm_game *g);

/* Stable full-path identity (eight hexadecimal FNV-1a digits). */
void gm_game_key(struct gm_game *g, const char *path, const char *gam);
