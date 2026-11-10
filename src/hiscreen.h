/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * PLAYGAME's high score screens (code/GM/HISCOREL.C, GRAPHC.C GGet): the name
 * entry when a score makes the Top Ten ("Congratulations!", "Enter Your Name:",
 * 22 characters, Enter or Esc accepts, Backspace or Left erases) and the Top Ten
 * list (30 seconds or any key). A 320 x 200 picture in palette indexes, as the
 * rest of the play core draws. Pure: time and input come in as arguments, so the
 * host tests drive it as the board does. Claude Sonnet 5.5 wrote this file. */
#pragma once
#include <stdint.h>
#include "hiscore.h"
#define HS_W 320u
#define HS_H 200u
#define HS_PAUSE_NS 30000000000ull /* PauseTimeKey(30) */
enum {HS_NAME, HS_TABLE};
enum {HS_DRAW = 1, HS_COMMIT = 2, HS_DONE = 4};
struct hs_screen {
  int stage, place, shift, ctrl, cur_on;
  struct his_table t;
  int32_t score;                  /* the game's score, or -1 when opened from the menu */
  char name[HIS_NAME_MAX + 1];
  uint32_t len;
  uint64_t t0, blink;
};
/* CheckHighScores then ShowHighScores for a finished game (score from the game),
   or just ShowHighScores for the menu's entry (score -1). A score that places
   starts the name entry; any other starts the list. */
void hs_start(struct hs_screen *h, const struct his_table *t, int32_t score, uint64_t now);
/* One key event (Linux codes). HS_COMMIT: the name was accepted and h->t holds the
   new table, for the caller to write. HS_DONE: leave. */
int hs_event(struct hs_screen *h, uint16_t type, uint16_t code, int32_t value, uint64_t now);
/* Time moves on: the cursor blinks, the list's 30 seconds end. */
int hs_step(struct hs_screen *h, uint64_t now);
uint64_t hs_wake(const struct hs_screen *h, uint64_t now);
/* The screen into px (320 x 200 indexes) and pal (6-bit). back and back_pal are the
   game's high score picture (File(0,8)), or 0 for black. */
void hs_draw(const struct hs_screen *h, uint8_t *px, uint8_t *pal, const uint8_t *back, const uint8_t *back_pal);
/* As hs_draw, retaining PLAYGAME's current colors for the cleared name entry
 * and for a table without a picture. colors may be 0 for the standalone default. */
void hs_draw_colors(const struct hs_screen *h, uint8_t *px, uint8_t *pal, const uint8_t *back, const uint8_t *back_pal, const uint8_t *colors);
/* The ASCII a Linux key code types (0: none). */
int hs_ascii(uint16_t code, int shift);
/* Shift and Ctrl as the kernel's input state has them (sbx_input_state.keys), read when the entry begins. */
void hs_modifiers(struct hs_screen *h, const uint64_t *keys);
/* The score to open with: below ABI 1.7 nothing can be saved, so a score that would place shows the list (-1). */
int32_t hs_entry_score(int32_t score, int can_write);
