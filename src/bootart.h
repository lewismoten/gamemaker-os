/* SPDX-License-Identifier: MIT; Game-Maker OS boot art on a 960x540 RGB565 surface: the splash shown from the
 * moment /dev/fb0 is open until the games list is ready, and the library diagnostics screen for an empty or
 * unreadable library. The badge is run-length data (bootart_data.c, from tools/make_bootart.py); the text is
 * the OS's own 5x7 font. stride is in pixels (pitch / 2). */
#pragma once
#include <stdint.h>
extern const uint16_t ba_pal[16];
extern const uint16_t ba_rle_bytes;
extern const uint8_t ba_rle[];
void ba_splash(uint16_t *px, uint32_t stride);
/* Diagnostics: lines are the OS's reasons (upper case, at most 59 characters; the framed area holds 12). */
#define BA_DIAG_ROWS 12u
void ba_diag(uint16_t *px, uint32_t stride, const char *const *lines, unsigned n);
/* Only the loading bars (a strip of BA_BAR_W x BA_BAR_H at BA_BAR_X, BA_BAR_Y): a highlight travels along them once
   every BA_CYCLE_MS, ms being the time since the splash began. segs: 4 or 8 segments. The strip is cleared first,
   so a redraw leaves nothing behind. */
#define BA_BAR_X 424u
#define BA_BAR_Y 497u
#define BA_BAR_W 112u
#define BA_BAR_H 4u
#define BA_CYCLE_MS 1600u
#define BA_SEGS 8u
#define BA_RAMP 0u /* the highlight's colors: 0 dip through dark, 1 hue path */
extern uint32_t ba_ramp_mode;
void ba_bar(uint16_t *px, uint32_t stride, uint32_t ms, uint32_t segs);
