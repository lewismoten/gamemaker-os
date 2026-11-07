/* SPDX-License-Identifier: MIT; playing one game on the 640x480 screen: the
 * title cards, each level's intro and exit cards, the play at PLAYGAME's
 * 18.2 Hz, the won or lost cards, and back to the list. Time and input come
 * in as arguments, so the host tests drive it exactly as the board does. */
#pragma once
#include <stdint.h>
#include "game.h"
#define GS_MAX_CARDS 24u
#define GS_CARD_Y 40u   /* a 320 x 200 card at x2 fills 640 x 400 from here */
#define GS_VIEW_X 8u    /* the 312 x 196 play view at x2 is 624 x 392, centered */
#define GS_VIEW_Y 44u
/* The PIT's 1193182 / 65536 Hz: one game tick in tenths of a nanosecond. */
#define GS_TICK_NS10 549254157ull
enum {GS_DRAW = 1, GS_EXIT = 2};
enum {GS_CARDS, GS_PLAY, GS_DONE};
struct gs_card {char name[GP_NAME_MAX + 1]; uint16_t pause;}; /* pause: seconds, 65535 = until a key */
struct gs_session {
  gp_read_fn read; void *user;
  void (*say)(const char *line);
  int phase, levels;
  int after;                    /* what follows the cards: GS_PLAY (begin or finish) or GS_DONE */
  int begun;                    /* the first level has started */
  struct gs_card cards[GS_MAX_CARDS];
  unsigned ncards, card;        /* the queue and the card showing */
  uint64_t card_until;          /* when the card showing ends (0: waits for a key) */
  int key_pressed;              /* a key went down since the card was shown */
  uint64_t t0; uint32_t ticks;  /* play time base and ticks run since it */
  uint32_t total_ticks;         /* ticks run since the game started */
  uint32_t ran;                 /* ticks run by the last gs_step */
  int showing_card;             /* the screen holds a card (else the play view) */
  uint8_t px[GP_W * GP_H], pal[768];
};
/* Opens the game (its .GAM in the folder the reader reads) and queues the
   title cards; 0, or -1 if it cannot be played. */
int gs_start(struct gs_session *s, gp_read_fn read, void *user, const char *gam_name, uint64_t now_ns);
/* One input event (Linux codes). Esc leaves the game (returns GS_EXIT). */
int gs_event(struct gs_session *s, uint16_t type, uint16_t code, int32_t value);
/* Moves time on to now: the next card, or the game ticks due (at most 4 at
   once). GS_DRAW when the picture changed, GS_EXIT when the game is over. */
int gs_step(struct gs_session *s, uint64_t now_ns);
/* When gs_step next has something to do (a tick, a card's end, or soon, to read input). */
uint64_t gs_wake(const struct gs_session *s, uint64_t now_ns);
/* The screen: a card or the play view at x2, black around it. */
void gs_draw(const struct gs_session *s, uint32_t *fb, uint32_t stride);
/* The DOS scan code for a Linux key code (0: none). */
int gs_scan(uint16_t code);
