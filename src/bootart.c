/* SPDX-License-Identifier: MIT; the boot splash and the library diagnostics screen (bootart.h). Layout from
 * art/README.txt: text on a fixed 6 x scale advance, 5x7 font; the badge at 2x (splash) or 80 px (diagnostics). */
#include "bootart.h"
#include "font.h"
#include "rgb565.h"
#define BG 0x101428u
#define WHITE 0xF8FCF8u
#define MUTED 0xA0ACD0u
#define NAVY 0x182C78u
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
/* The loading bars: a highlight travels along them once every BA_CYCLE_MS. A segment is light blue at rest and eases
   to yellow as the highlight passes (16 shades, exact in RGB565); the background-like navy is not used, so every
   segment stays visible. segs is 4 (24 px, 4 px gaps) or 8 (12 px, 2 px gaps). */
uint32_t ba_ramp_mode = BA_RAMP; /* 0 blue base, gold comet; 1 gold base (the host test renders both) */
static uint32_t mix(uint32_t a, uint32_t b, uint32_t t, uint32_t n) { /* a toward b, t of n, per channel */
  uint32_t o = 0;
  for (int sh = 16; sh >= 0; sh -= 8) {uint32_t x = a >> sh & 255u, y = b >> sh & 255u; o = o << 8 | (x * (n - t) + y * t + n / 2u) / n;}
  return o;
}
/* The shade of a segment k/16 of the way into the comet (0 outside it, 16 at the peak). No segment is ever darker
   than the resting one. GOLD is yellow #F8D450 dimmed to the relative luminance of the resting light blue #7898F8
   (a hair above it once both are rounded to RGB565), so a segment switches blue to gold with no brightness change.
   Mode 0: blue at rest, gold from the edge of the comet, brightening to yellow at the peak. Mode 1: gold at rest. */
#define BLUE 0x7898F8u
#define GOLD 0xB79C39u
#define YELLOW 0xF8D450u
static uint32_t ramp(uint32_t k) {
  if (k == 0) return ba_ramp_mode == 0 ? BLUE : GOLD;
  return mix(GOLD, YELLOW, k - 1u, 15u);
}
static void bar(uint32_t ms, uint32_t segs) {
  uint32_t w = segs == 8u ? 12u : 24u, pitch = segs == 8u ? 14u : 28u; int32_t x0 = segs == 8u ? 425 : 426;
  rect((int32_t)BA_BAR_X, (int32_t)BA_BAR_Y, (int32_t)BA_BAR_W, (int32_t)BA_BAR_H, BG);
  uint32_t pos = (ms % BA_CYCLE_MS) * segs * 16u / BA_CYCLE_MS; /* the highlight's place in 1/16 segments */
  for (uint32_t i = 0; i < segs; i++) {
    uint32_t at = i * 16u, d = pos > at ? pos - at : at - pos;
    if (d > segs * 8u) d = segs * 16u - d; /* around the loop */
    uint32_t reach = 28u, k = d >= reach ? 0 : (reach - d) * 16u / reach; /* two segments either side fade out */
    rect(x0 + (int32_t)(i * pitch), (int32_t)BA_BAR_Y, (int32_t)w, (int32_t)BA_BAR_H, ramp(k));
  }
}
void ba_bar(uint16_t *px, uint32_t stride, uint32_t ms, uint32_t segs) {P = px; S = stride; bar(ms, segs);}
void ba_blank(uint16_t *px, uint32_t stride) {
  for (uint32_t y = 0; y < 540; y++) for (uint32_t x = 0; x < 960; x++) px[y * stride + x] = 0;
}

void ba_splash(uint16_t *px, uint32_t stride) {
  P = px; S = stride;
  rect(0, 0, 960, 540, BG);
  badge(384, 46, 192);
  text(-1, 272, "GAME-MAKER OS", WHITE, 5);
  text(-1, 328, "SANDBOX EDITION", YELLOW, 2);
  text(-1, 375, "LOADING GAME LIBRARY...", MUTED, 2);
  bar(0, BA_SEGS);
}
void ba_diag(uint16_t *px, uint32_t stride, const char *const *lines, unsigned n) {
  P = px; S = stride;
  rect(0, 0, 960, 540, BG);
  badge(91, 35, 80);
  text(190, 53, "GAME-MAKER OS", WHITE, 4);
  text(191, 96, "LIBRARY DIAGNOSTICS", YELLOW, 2);
  rect(112, 135, 736, 2, NAVY); rect(112, 453, 736, 2, NAVY);
  for (unsigned i = 0; i < n && i < BA_DIAG_ROWS; i++) text(128, 150 + (int32_t)i * 24, lines[i], WHITE, 2);
  bar(0, BA_SEGS);
}
