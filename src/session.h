/* SPDX-License-Identifier: MIT; playing one game on the 640x480 screen: the
 * pages between scenes (GIF pictures, BKD backdrops, FLI animations, text
 * pages and .LST lists of them) with PLAYGAME's palette fades, each level's
 * intro and exit pages, the play at PLAYGAME's 18.2 Hz, the won or lost pages,
 * and back to the list. Time and input come in as arguments, so the host tests
 * drive it exactly as the board does. */
#pragma once
#include <stdint.h>
#include "fli.h"
#include "game.h"
#include "sound.h"
#define GS_MAX_CARDS 32u
#define GS_CARD_Y 40u   /* a 320 x 200 page at x2 fills 640 x 400 from here */
#define GS_VIEW_X 8u    /* the 312 x 196 play view at x2 is 624 x 392, centered */
#define GS_VIEW_Y 44u
/* The PIT's 1193182 / 65536 Hz: one game tick in tenths of a nanosecond. */
#define GS_TICK_NS10 549254157ull
#define GS_FADE_STEPS 65u /* FadeTo: 65 steps, one a vertical retrace (70 Hz) */
#define GS_KEY_WAIT 65535u
enum {GS_DRAW = 1, GS_EXIT = 2};
enum {GS_CARDS, GS_PLAY, GS_DONE};
/* What the pages phase is doing. */
enum {GS_NEXT, GS_FADE, GS_FLI, GS_PAUSE, GS_LEVEL_IN, GS_LEVEL_OUT};
struct gs_card {
  char name[GP_NAME_MAX + 1];
  uint32_t pause;   /* seconds, GS_KEY_WAIT = until a key, above it the file type's default */
  uint8_t list;     /* which .LST it came from (0: none): its pages share one palette */
  uint8_t clear;    /* clear the screen first (the first level's intro, after the title) */
};
struct gs_session {
  gp_read_fn read; void *user;
  void (*say)(const char *line);
  int phase, stage, levels;
  int after;                    /* what follows the pages: GS_PLAY (begin or finish) or GS_DONE */
  int begun;                    /* the first level has started */
  char change;                  /* the 'D', 'L' or 'W' the level ended with, while it fades out */
  struct gs_card cards[GS_MAX_CARDS];
  unsigned ncards, card;        /* the queue and the next page to show */
  const struct gs_card *now_card; /* the page showing (0: none) */
  unsigned keys;                /* key presses waiting to be read (the BIOS buffer), while pages show */
  uint64_t t;                   /* when the current fade, animation or pause began */
  uint32_t pause;               /* the pause's seconds (GS_KEY_WAIT: until a key) */
  uint32_t fade_step;           /* the fade's step shown */
  uint32_t fli_frame;           /* animation frames shown */
  struct gp_fli fli;
  int text, more;               /* a text page showing; more lines after it */
  uint32_t text_at, text_n; int32_t vertpos; uint8_t text_col, text_red;
  uint32_t shown;               /* counts pages fully shown (a picture faded in, an animation played, a text page written) */
  uint64_t t0; uint32_t ticks;  /* play time base and ticks run since it */
  uint32_t total_ticks;         /* ticks run since the game started */
  uint32_t ran;                 /* ticks run by the last gs_step */
  int showing_card;             /* the screen holds a page (else the play view) */
  uint8_t px[GP_W * GP_H], pal[768];
  uint8_t from[768], to[768];   /* a fade's ends */
  uint8_t colors[768];          /* PLAYGAME's `colors`: the menu picture's palette, then each level's */
  uint8_t listpal[768]; uint8_t listid; /* a list's NewCols: the palette of its last picture */
  struct gs_sound sound;        /* SND log lines (sound.h); its dir is set before gs_start */
};
/* Opens the game (its .GAM in the folder the reader reads) and queues the
   title pages; 0, or -1 if it cannot be played. */
int gs_start(struct gs_session *s, gp_read_fn read, void *user, const char *gam_name, uint64_t now_ns);
/* One input event (Linux codes). Esc leaves the game (returns GS_EXIT). */
int gs_event(struct gs_session *s, uint16_t type, uint16_t code, int32_t value);
/* Moves time on to now: fades, animation frames, pauses and pages, or the game
   ticks due (at most 4 at once). GS_DRAW when the picture changed, GS_EXIT
   when the game is over. */
int gs_step(struct gs_session *s, uint64_t now_ns);
/* When gs_step next has something to do (a tick, a fade step, a frame, a
   pause's end, or soon, to read input). */
uint64_t gs_wake(const struct gs_session *s, uint64_t now_ns);
/* A 320 x 200 picture and its 6-bit palette as a page (x2, as gs_draw shows a card): the Top Ten screens. */
void gs_show(const uint8_t *px, const uint8_t *pal, uint32_t *fb, uint32_t stride);
/* The screen: a page or the play view at x2, black around it. */
void gs_draw(const struct gs_session *s, uint32_t *fb, uint32_t stride);
/* The game's high score picture (File(0,8): a GIF or BKD) into px (320 x 200 indexes) and pal,
   as the Top Ten screen's backdrop; 0, or -1 if it has none. open is 1 when no game is open
   yet (the list's own Top Ten): it opens gam first. */
int gs_scores_picture(struct gs_session *s, int open, gp_read_fn read, void *user, const char *gam, uint8_t *px, uint8_t *pal);
/* The DOS scan code for a Linux key code (0: none). */
int gs_scan(uint16_t code);
/* FadeTo's palette at a step (0..steps) between two palettes. */
void gs_fade_palette(const uint8_t *from, const uint8_t *to, uint32_t step, uint32_t steps, uint8_t *out);
