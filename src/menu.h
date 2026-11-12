/* SPDX-License-Identifier: MIT; the games list as data, and what input does
 * to it. It follows the Sandbox Edition web site's Games page: category tabs
 * (Starters, Sandbox, Bizarre, CD, Other for games the library index does
 * not know, and Archive for /sd/ARCHIVE, which no index line moves a game out of), a grid of game cards eight to a page, and a details view with Play.
 * Pure: no system calls, so the host tests drive it directly. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gam.h"
#include "library.h"
#define GM_MAX_GAMES 256u    /* games on the main tabs */
#define GM_MAX_ARCHIVE 512u  /* games of the Archive tab (/sd/ARCHIVE): their own room, so they never crowd the main tabs out */
#define GM_MAX_ALL (GM_MAX_GAMES + GM_MAX_ARCHIVE)
#define GM_COLS 4u
#define GM_PAGE 8u /* cards on a page: four across, two down */
#define GM_MESSAGE_MAX 48u
enum {GM_CAT_STARTER, GM_CAT_SANDBOX, GM_CAT_BIZARRE, GM_CAT_CD, GM_CAT_OTHER, GM_CAT_ARCHIVE, GM_CATS};
enum {GM_GRID, GM_DETAILS};       /* what shows */
enum {GM_BTN_PLAY, GM_BTN_BACK, GM_BUTTONS}; /* the details view's buttons */
struct gm_game {
  char dir[9], gam[13];             /* folder and its .GAM file */
  char title[GM_LIB_TITLE + 1];     /* the index's title, else the .GAM's, else the folder */
  uint32_t crc; int32_t year, course;
  uint8_t cat; int8_t pic;          /* pic: 0 not looked for yet, 1 found, -1 none */
};
/* The index's words, only for the main tabs' games (slot = the game's number): an Archive entry is compact and has none. */
struct gm_text {char company[GM_LIB_COMPANY + 1], blurb[GM_LIB_BLURB + 1];};
struct gm_menu {
  struct gm_game games[GM_MAX_ALL];
  struct gm_text text[GM_MAX_GAMES];
  unsigned count, archive_count;    /* count holds both; archive_count of them are the Archive tab's */
  uint16_t order[GM_MAX_ALL];      /* games by tab, then by course and title */
  unsigned tab_first[GM_CATS], tab_count[GM_CATS];
  unsigned tab, view, button;
  int background_music;             /* owner setting: off on boot, M toggles in the library */
  unsigned pos[GM_CATS];            /* the selected card in each tab (remembered) */
  int finished;                     /* gm_finish has run: archive adds then insert in order */
  int shift;                        /* a Shift key is held (Shift+Tab) */
  char message[GM_MESSAGE_MAX + 1]; /* footer line; empty shows the key help */
};
/* Size budget: the 512 Archive entries may cost under 100 KiB in all (a game here, its place in order[], main.c's where_a). */
_Static_assert(sizeof(struct gm_game) <= 112, "struct gm_game grew: every Archive entry pays for it (512 of them)");
/* The company and blurb of a game ("" for an Archive entry or a game the index does not name). */
static inline const char *gm_company(const struct gm_menu *m, const struct gm_game *g) {return g->cat == GM_CAT_ARCHIVE ? "" : m->text[g - m->games].company;}
static inline const char *gm_blurb(const struct gm_menu *m, const struct gm_game *g) {return g->cat == GM_CAT_ARCHIVE ? "" : m->text[g - m->games].blurb;}
enum {GM_NONE, GM_MOVED, GM_CHOSEN, GM_QUIT, GM_RESYNC, GM_SCORES, GM_MUSIC}; /* GM_SCORES: H, the selected game's Top Ten */
/* Add a game (dir is its folder's name, gam its .GAM file, title from that
   file, crc the .GAM's CRC-32); false when the list is full. */
bool gm_add(struct gm_menu *m, const char *dir, const char *gam, const char *title, uint32_t crc);
/* Add a game of /sd/ARCHIVE to the Archive tab (its own room of GM_MAX_ARCHIVE; the
   main tabs keep GM_MAX_GAMES). Before gm_finish it is only appended; after, it is
   inserted into the tab's sorted place (one pass over the Archive tab, so a frame
   can add one without a pause), whatever order the files were found in.
   False when the Archive room is full. */
bool gm_add_archive(struct gm_menu *m, const char *dir, const char *gam, const char *title, uint32_t crc);
/* One line of the library index: fills in the game it describes (matched by
   the .GAM's CRC-32, else its locator or DOS name). 1 when a game matched. */
int gm_library_line(struct gm_menu *m, const char *line, uint32_t n);
/* Sorts the games into their tabs and selects the first card of the Sandbox
   tab (the web page's default), or of the first tab that has games. */
void gm_finish(struct gm_menu *m);
/* One input event (Linux codes). Grid: arrows move, PgUp/PgDn turn the page,
   Home/End, Tab and Shift+Tab (or [ and ]) change the tab, Enter opens the
   details, Esc or Q quits, H shows the selected game's Top Ten. Details: Left/Right/Tab pick a button, Enter or
   Space presses it (Play chooses the game), H shows the Top Ten, Esc goes back. SYN_DROPPED asks
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
