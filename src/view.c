/* SPDX-License-Identifier: MIT; original games list layout (view.h). */
#include "view.h"
#include "font.h"
#define BG 0x0b1a3cu
#define BAR 0x1f3a93u
#define ACCENT 0xffcc33u
#define TEXT 0xf2f4ffu
#define DIM 0x9fb0d8u
#define SEL_TEXT 0x0b1a3cu
#define FOOT 0x142a66u
#define LIST_Y 84u
#define ROW_H 38u
static void fill(uint32_t *px, uint32_t stride, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t c) {
  for (uint32_t j = y; j < y + h && j < GM_H; j++) for (uint32_t i = x; i < x + w && i < GM_W; i++) px[j * stride + i] = c;
}
uint32_t gm_text_width(const char *s, uint32_t scale) {
  uint32_t n = 0; while (s[n]) n++;
  return n ? n * 6 * scale - scale : 0;
}
void gm_text(uint32_t *px, uint32_t stride, uint32_t x, uint32_t y, const char *s, uint32_t scale, uint32_t color) {
  for (; *s; s++, x += 6 * scale) {
    unsigned c = (unsigned char)*s; if (c < 32 || c > 126) c = '?';
    const uint8_t *g = gm_font[c - 32];
    for (uint32_t r = 0; r < GM_FONT_H; r++) for (uint32_t b = 0; b < GM_FONT_W; b++)
      if (g[r] >> (4 - b) & 1) fill(px, stride, x + b * scale, y + r * scale, scale, scale, color);
  }
}
static void center(uint32_t *px, uint32_t stride, uint32_t y, const char *s, uint32_t scale, uint32_t color) {
  gm_text(px, stride, (GM_W - gm_text_width(s, scale)) / 2, y, s, scale, color);
}
/* A small solid triangle pointing up (dir < 0) or down, 13 px wide. */
static void arrow(uint32_t *px, uint32_t stride, uint32_t x, uint32_t y, int dir, uint32_t c) {
  for (uint32_t r = 0; r < 7; r++) {uint32_t half = dir < 0 ? r : 6 - r; fill(px, stride, x + 6 - half, y + r, 2 * half + 1, 1, c);}
}
static char *put(char *p, const char *s) {while (*s) *p++ = *s++; *p = 0; return p;}
static char *num(char *p, uint32_t n) {char t[11]; int k = 0; do t[k++] = (char)('0' + n % 10); while (n /= 10); while (k) *p++ = t[--k]; *p = 0; return p;}
void gm_draw(uint32_t *px, uint32_t stride, const struct gm_menu *m) {
  fill(px, stride, 0, 0, GM_W, GM_H, BG);
  fill(px, stride, 0, 0, GM_W, 64, BAR);
  fill(px, stride, 0, 64, GM_W, 4, ACCENT);
  center(px, stride, 18, "GAME-MAKER OS", 4, TEXT);
  if (!m->count) {
    center(px, stride, 200, "No games in /sd/GAMES", 3, TEXT);
    center(px, stride, 248, "Copy a game's folder there to see it here.", 2, DIM);
  }
  for (uint32_t i = 0; i < GM_ROWS && m->top + i < m->count; i++) {
    const struct gm_game *g = &m->games[m->top + i];
    uint32_t y = LIST_Y + i * ROW_H; int sel = m->top + i == m->sel;
    if (sel) fill(px, stride, 24, y, GM_W - 48, ROW_H - 4, ACCENT);
    gm_text(px, stride, 40, y + 6, g->title, 3, sel ? SEL_TEXT : TEXT);
    gm_text(px, stride, GM_W - 40 - gm_text_width(g->dir, 2), y + 10, g->dir, 2, sel ? BAR : DIM);
  }
  if (m->top) arrow(px, stride, GM_W - 20, LIST_Y + 2, -1, DIM);
  if (m->top + GM_ROWS < m->count) arrow(px, stride, GM_W - 20, LIST_Y + GM_ROWS * ROW_H - 12, 1, DIM);
  fill(px, stride, 0, 436, GM_W, 44, FOOT);
  gm_text(px, stride, 24, 451, m->message[0] ? m->message : "UP/DOWN choose   ENTER play   ESC quit", 2, m->message[0] ? ACCENT : TEXT);
  if (m->count) {
    char pos[24], *p = num(pos, m->sel + 1); p = put(p, "/"); num(p, m->count);
    gm_text(px, stride, GM_W - 24 - gm_text_width(pos, 2), 451, pos, 2, DIM);
  }
}
