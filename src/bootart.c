/* SPDX-License-Identifier: MIT; the boot splash and the library diagnostics screen (bootart.h). Layout from
 * art/README.txt: text on a fixed 6 x scale advance, 5x7 font; the badge at 2x (splash) or 80 px (diagnostics). */
#include "bootart.h"
#include "font.h"
#include "rgb565.h"
#define BG 0x101428u
#define WHITE 0xF8FCF8u
#define YELLOW 0xF8D450u
#define MUTED 0xA0ACD0u
#define NAVY 0x182C78u
#define BLUE 0x7898F8u
static uint16_t *P; static uint32_t S;
static void rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
  uint16_t v = gm_565(c);
  for (int32_t j = y; j < y + h && j < 540; j++) for (int32_t i = x; i < x + w && i < 960; i++) P[(uint32_t)j * S + (uint32_t)i] = v;
}
static uint32_t slen(const char *s) {uint32_t n = 0; while (s[n]) n++; return n;}
/* Text at scale k, 6 * k a character (the art's spacing); x < 0 centers it on the screen. */
static void text(int32_t x, int32_t y, const char *s, uint32_t c, int32_t k) {
  if (x < 0) x = (960 - ((int32_t)slen(s) * 6 - 1) * k) / 2;
  for (; *s; s++, x += 6 * k) {
    unsigned g = (unsigned char)*s; if (g < 32 || g > 126) g = '?';
    for (unsigned r = 0; r < 7; r++) for (unsigned col = 0; col < GM_FONT_W; col++)
      if (gm_font[g - 32][r] >> (GM_FONT_W - 1 - col) & 1) rect(x + (int32_t)col * k, y + (int32_t)r * k, k, k, c);
  }
}
/* The badge's 96 x 96 pixels scaled by nearest neighbor (each target pixel's center) to size pixels at (x, y): the
   run data is read once, a source row at a time into a 96-byte buffer. */
static void badge(int32_t x, int32_t y, uint32_t size) {
  uint8_t row[96]; uint32_t at = 0, left = 0, done = 0; int32_t have = -1;
  for (uint32_t ty = 0; ty < size; ty++) {
    int32_t sy = (int32_t)((2u * ty + 1u) * 96u / (2u * size));
    while (have < sy) {
      for (uint32_t i = 0; i < 96; i++, left--) {
        if (!left) {left = (ba_rle[at] & 15u) + 1u; done = ba_rle[at++] >> 4;}
        row[i] = (uint8_t)done;
      }
      have++;
    }
    for (uint32_t tx = 0; tx < size; tx++) P[(uint32_t)(y + (int32_t)ty) * S + (uint32_t)(x + (int32_t)tx)] = ba_pal[row[(2u * tx + 1u) * 96u / (2u * size)]];
  }
}
/* The four bars at the bottom; step n rotates the colors one place a step along the bars. */
static void bar(uint32_t step) {
  static const uint32_t col[4] = {NAVY, BLUE, YELLOW, WHITE};
  for (uint32_t i = 0; i < 4; i++) rect(426 + (int32_t)i * 28, 497, 24, 4, col[(i + step) & 3u]);
}
void ba_bar(uint16_t *px, uint32_t stride, uint32_t step) {P = px; S = stride; bar(step);}
void ba_splash(uint16_t *px, uint32_t stride) {
  P = px; S = stride;
  rect(0, 0, 960, 540, BG);
  badge(384, 46, 192);
  text(-1, 272, "GAME-MAKER OS", WHITE, 5);
  text(-1, 328, "SANDBOX EDITION", YELLOW, 2);
  text(-1, 375, "LOADING GAME LIBRARY...", MUTED, 2);
  bar(0);
}
void ba_diag(uint16_t *px, uint32_t stride, const char *const *lines, unsigned n) {
  P = px; S = stride;
  rect(0, 0, 960, 540, BG);
  badge(91, 35, 80);
  text(190, 53, "GAME-MAKER OS", WHITE, 4);
  text(191, 96, "LIBRARY DIAGNOSTICS", YELLOW, 2);
  rect(112, 135, 736, 2, NAVY); rect(112, 453, 736, 2, NAVY);
  for (unsigned i = 0; i < n && i < BA_DIAG_ROWS; i++) text(128, 150 + (int32_t)i * 24, lines[i], WHITE, 2);
  bar(0);
}
