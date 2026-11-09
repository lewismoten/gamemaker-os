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
