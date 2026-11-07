/* SPDX-License-Identifier: MIT; text pages between scenes as PLAYGAME's
 * Display_File shows them: 31-character lines (fgets with a 32-byte buffer)
 * in its 8 x 8 font, 16 lines a page from row 5, over the picture showing. */
#pragma once
#include <stdint.h>
extern const uint8_t gp_font8[128 * 8];
/* GWrite: text at (x, y) in palette index col, clipped to 320 x 200; only the
   set bits are drawn; '\n' starts a new line under x; bytes from 128 up draw nothing. */
void gp_write(uint8_t *px, int32_t x, int32_t y, uint8_t col, const char *s, uint32_t n);
/* A text file as Display_File reads it in text mode: Ctrl-Z ends it, CR LF is
   LF. Rewrites d in place; returns the new length. */
uint32_t gp_text_mode(uint8_t *d, uint32_t n);
/* The next line from offset *at (at most 31 characters, ending after a newline,
   which is not part of it); its length, or -1 at the end. */
int32_t gp_text_line(const uint8_t *d, uint32_t n, uint32_t *at, uint32_t *start);
/* The palette entry nearest a 6-bit color (RGBdata::Match). */
uint8_t gp_match(const uint8_t *pal, int r, int g, int b);
