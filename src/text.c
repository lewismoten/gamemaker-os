/* SPDX-License-Identifier: MIT; text pages (text.h), a C port of Game-Maker:
 * Sandbox Edition's Screen.write and putChar, formats/graphicScripts.ts
 * textLines and formats/palette.ts matchColor (MIT). */
#include "text.h"
void gp_write(uint8_t *px, int32_t x, int32_t y, uint8_t col, const char *s, uint32_t n) {
  int64_t cx = x, cy = y;
  for (uint32_t i = 0; i < n; i++) {
    uint8_t c = (uint8_t)s[i];
    if (c == 10) {cy += 8; cx = x; continue;}
    if (c < 128)
      for (int32_t row = 0; row < 8; row++) {
        uint8_t bits = gp_font8[c * 8u + (uint32_t)row];
        for (int32_t b = 0; b < 8; b++) {
          int64_t px_x = cx + b, px_y = cy + row;
          if ((bits >> b & 1) && px_x >= 0 && px_y >= 0 && px_x < 320 && px_y < 200) px[px_y * 320 + px_x] = col;
        }
      }
    cx += 8;
  }
}
uint32_t gp_text_mode(uint8_t *d, uint32_t n) {
  uint32_t o = 0;
  for (uint32_t i = 0; i < n; i++) {
    if (d[i] == 0x1a) break;
    if (d[i] == '\r' && i + 1 < n && d[i + 1] == '\n') continue;
    d[o++] = d[i];
  }
  return o;
}
int32_t gp_text_line(const uint8_t *d, uint32_t n, uint32_t *at, uint32_t *start) {
  uint32_t i = *at, end = i;
  if (i >= n) return -1;
  while (end < n && end - i < 31) if (d[end++] == '\n') break;
  *start = i; *at = end;
  return (int32_t)(end - i) - (d[end - 1] == '\n' ? 1 : 0);
}
uint8_t gp_match(const uint8_t *pal, int r, int g, int b) {
  int best = 0, dist = 1 << 30;
  for (int i = 0; i < 256; i++) {
    int dr = (pal[i * 3] & 63) - r, dg = (pal[i * 3 + 1] & 63) - g, db = (pal[i * 3 + 2] & 63) - b, d = dr * dr + dg * dg + db * db;
    if (d < dist) {dist = d; best = i;}
  }
  return (uint8_t)best;
}
