/* SPDX-License-Identifier: MIT; original games list layout (view.h). Two
 * bitmap fonts drawn proportionally (each glyph as wide as its ink, a column
 * apart): PLAYGAME's 8 x 8 font (text.h) for titles and labels, the 5 x 9
 * Game-Maker OS font (font.h) for smaller lines. At 960 x 540: four cards
 * across, two down, and the details view's title screen at twice its size. */
#include "view.h"
#include "font.h"
#include "text.h"
#include "thumb.h"
#include "rgb565.h"
#define C_HEADER 0x000080u
#define C_GOLD 0xffd700u
#define C_PAGE 0x11131au
#define C_PANEL 0x1b1e28u
#define C_LINE 0x343a4fu
#define C_LINE2 0x262a38u
#define C_BLUE 0x6d8bffu
#define C_TEXT 0xeef0f6u
#define C_MUTED 0x9aa2b8u
#define C_DIM 0x787e91u
#define C_PILL 0x2a2e3cu
#define C_FOOT 0x0b0d13u
#define C_BADGE 0x383365u
#define C_FRAME 0xe6dfcbu
#define C_TITLE 0xffe14du
#define C_OUTLINE 0x2a1060u
#define C_EMPTY 0x232736u
#define HEADER_H 40u
#define TABS_Y 48u
#define TABS_H 32u
#define HEAD_Y 92u
#define GRID_Y 112u
#define CARD_W 220u /* four across with 16 between and at the sides */
#define CARD_H 180u
#define GAP_X 16u
#define GAP_Y 12u
#define FOOT_Y 494u
#define PIC_X 36   /* the details view's title screen, doubled: 640 x 400 */
#define PIC_Y 68
#define SIDE_X 700 /* the details view's column beside it */
#define SIDE_W 232
static uint16_t *P; static uint32_t S; /* the surface being drawn */
static int picture_loading; /* the details picture is still being read: say so, not "No title screen" */
static int measuring; /* count_lines: nothing is drawn */
static uint32_t work_stores, work_divs; /* gm_view_work: pixels stored and software divisions, for the host frame-cost tests */
static int on_page(int32_t x, int32_t y) {return !measuring && x >= 0 && y >= 0 && x < (int32_t)GM_W && y < (int32_t)GM_H;}
static void dot16(int32_t x, int32_t y, uint16_t c) {if (on_page(x, y)) {work_stores++; P[(uint32_t)y * S + (uint32_t)x] = c;}}
static void dot_dither(int32_t x, int32_t y, uint32_t c) {if (on_page(x, y)) {work_stores++; P[(uint32_t)y * S + (uint32_t)x] = gm_565_dither(c, (uint32_t)x, (uint32_t)y);}}
static void fill(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
  if (measuring) return;
  uint16_t v = gm_565(c);
  for (int32_t j = y < 0 ? 0 : y; j < y + h && j < (int32_t)GM_H; j++)
    for (int32_t i = x < 0 ? 0 : x; i < x + w && i < (int32_t)GM_W; i++) {P[(uint32_t)j * S + (uint32_t)i] = v; work_stores++;}
}
/* How far row j (0 at the top edge) of a corner of radius r is cut in. */
static int32_t inset(int32_t r, int32_t j) {
  if (j >= r) return 0;
  int32_t dy = 2 * r - 2 * j - 1, i = 0;
  while (i < r && (2 * r - 2 * i - 1) * (2 * r - 2 * i - 1) + dy * dy > 4 * r * r) i++;
  return i;
}
static void round_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) {
  for (int32_t j = 0; j < h; j++) {int32_t k = inset(r, j < h - 1 - j ? j : h - 1 - j); fill(x + k, y + j, w - 2 * k, 1, c);}
}
/* An outlined rounded box: t pixels of line, then the inside (inside 0: none). */
static void box(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, int32_t t, uint32_t line, uint32_t inside, int filled) {
  for (int32_t j = 0; j < h; j++) {
    int32_t k = inset(r, j < h - 1 - j ? j : h - 1 - j);
    int32_t in_j = j - t, in_h = h - 2 * t;
    if (in_j < 0 || in_j >= in_h) {fill(x + k, y + j, w - 2 * k, 1, line); continue;}
    int32_t ri = r - t > 0 ? r - t : 0, ki = t + inset(ri, in_j < in_h - 1 - in_j ? in_j : in_h - 1 - in_j);
    fill(x + k, y + j, ki - k, 1, line); fill(x + w - ki, y + j, ki - k, 1, line);
    if (filled) fill(x + ki, y + j, w - 2 * ki, 1, inside);
  }
}
/* Fonts: glyph bits, and each glyph's ink columns (left, width) found once. */
enum {F8, F5};
static uint8_t left8[128], wide8[128], left5[95], wide5[95], measured;
static int bit(int f, unsigned c, unsigned row, unsigned col) {
  if (f == F8) return c < 128 && row < 8 && col < 8 && (gp_font8[c * 8 + row] >> col & 1);
  return c >= 32 && c < 127 && row < GM_FONT_H && col < GM_FONT_W && (gm_font[c - 32][row] >> (GM_FONT_W - 1 - col) & 1);
}
static void measure(void) {
  for (int f = F8; f <= F5; f++)
    for (unsigned c = f == F8 ? 0 : 32; c < (f == F8 ? 128u : 127u); c++) {
      unsigned lo = 99, hi = 0, cols = f == F8 ? 8 : GM_FONT_W, rows = f == F8 ? 8 : GM_FONT_H;
      for (unsigned col = 0; col < cols; col++) for (unsigned row = 0; row < rows; row++) if (bit(f, c, row, col)) {if (col < lo) lo = col; if (col > hi) hi = col;}
      uint8_t l = lo == 99 ? 0 : (uint8_t)lo, w = lo == 99 ? 0 : (uint8_t)(hi - lo + 1);
      if (f == F8) {left8[c] = l; wide8[c] = w;} else {left5[c - 32] = l; wide5[c - 32] = w;}
    }
  measured = 1;
}
static unsigned glyph(int f, char ch) {(void)f; unsigned c = (unsigned char)ch; return c < 32 || c > 126 ? '?' : c;}
static int32_t advance(int f, char ch, int32_t scale) {
  if (!measured) measure();
  unsigned c = glyph(f, ch);
  if (c == ' ') return (f == F8 ? 4 : 3) * scale;
  return ((f == F8 ? wide8[c] : wide5[c - 32]) + 1) * scale;
}
static int32_t width(int f, const char *s, uint32_t n, int32_t scale) {
  int32_t w = 0; for (uint32_t i = 0; i < n && s[i]; i++) w += advance(f, s[i], scale);
  return w ? w - scale : 0;
}
static uint32_t len(const char *s) {uint32_t n = 0; while (s[n]) n++; return n;}
static int32_t text(int f, int32_t x, int32_t y, const char *s, uint32_t n, uint32_t color, int32_t scale) {
  if (!measured) measure();
  for (uint32_t i = 0; i < n && s[i]; i++) {
    unsigned c = glyph(f, s[i]);
    if (c != ' ') {
      unsigned l = f == F8 ? left8[c] : left5[c - 32], w = f == F8 ? wide8[c] : wide5[c - 32], rows = f == F8 ? 8 : GM_FONT_H;
      for (unsigned row = 0; row < rows; row++) for (unsigned col = 0; col < w; col++)
        if (bit(f, c, row, l + col)) fill(x + (int32_t)col * scale, y + (int32_t)row * scale, scale, scale, color);
    }
    x += advance(f, s[i], scale);
  }
  return x;
}
static int32_t text0(int f, int32_t x, int32_t y, const char *s, uint32_t color) {return text(f, x, y, s, len(s), color, 1);}
/* Greedy word wrap into at most max lines of w pixels; the last line ends in
   "..." when text is left over. Draws each line, lh pixels apart; returns the lines drawn. */
static unsigned wrap(int f, int32_t x, int32_t y, int32_t w, const char *s, unsigned max, int32_t lh, uint32_t color, int32_t scale) {
  uint32_t at = 0, n = len(s); unsigned lines = 0;
  while (at < n && lines < max) {
    while (at < n && s[at] == ' ') at++;
    if (at >= n) break;
    uint32_t end = at, fit = at;
    while (end < n) {
      uint32_t e = end; while (e < n && s[e] == ' ') e++;
      while (e < n && s[e] != ' ') e++;
      if (width(f, s + at, e - at, scale) > w) break;
      fit = end = e;
    }
    if (fit == at) {fit = at + 1; while (fit < n && width(f, s + at, fit + 1 - at, scale) <= w) fit++;} /* one long word */
    uint32_t rest = fit; while (rest < n && s[rest] == ' ') rest++;
    if (lines + 1 == max && rest < n) { /* the last line: cut it to end in "..." */
      int32_t dots = width(f, "...", 3, scale) + advance(f, '.', scale);
      while (fit > at && width(f, s + at, fit - at, scale) + dots > w) fit--;
      while (fit > at && s[fit - 1] == ' ') fit--;
      int32_t ex = text(f, x, y + (int32_t)lines * lh, s + at, fit - at, color, scale);
      text(f, ex, y + (int32_t)lines * lh, "...", 3, color, scale);
      return lines + 1;
    }
    text(f, x, y + (int32_t)lines * lh, s + at, fit - at, color, scale);
    lines++; at = fit;
  }
  return lines;
}
static unsigned count_lines(int f, int32_t w, const char *s, unsigned max, int32_t scale) {
  measuring = 1;
  unsigned k = wrap(f, 0, 0, w, s, max, 0, 0, scale);
  measuring = 0; return k;
}
static char *put(char *p, const char *s) {while (*s) *p++ = *s++; *p = 0; return p;}
static char *num(char *p, uint32_t n) {char t[11]; int k = 0; do t[k++] = (char)('0' + n % 10); while (n /= 10); while (k) *p++ = t[--k]; *p = 0; return p;}
static const char *const TAB_NAME[GM_CATS] = {"Starters", "Sandbox", "Bizarre", "CD", "Other", "Archive", "Search", "Favorites", "Recent"};
static const char *const BADGE[GM_CATS] = {"STARTER", "SANDBOX", "BIZARRE", "CD", "ON THIS CARD", "ARCHIVE"};
static const char *const TAB_LINE[GM_CATS] = {
  "Small games that each show one Game-Maker idea.",
  "New games in the original formats, so they play in DOS too.",
  "Heartfelt songs about all the wrong things, made into games.",
  "The Game-Maker CD: free gameware, then other people's shareware.",
  "Games in /sd/GAMES that the library index does not list.",
  "Old user-made Game-Maker games from /sd/ARCHIVE.",
  "", "Starred games, newest first.", "Recently played, newest first.",
};
extern const uint16_t ha_pal[4];
extern const uint8_t ha_bits[256];
static void header(void) {
  fill(0, 0, GM_W, HEADER_H, C_HEADER);
  for (int32_t j = 0; j < 32; j++) for (int32_t i = 0; i < 32; i++) {
    unsigned v = (ha_bits[(j * 32 + i) / 4] >> (6 - 2 * ((j * 32 + i) & 3))) & 3u;
    if (v) dot16(8 + i, 4 + j, ha_pal[v]);
  }
  int32_t x = 48;
  for (const char *s = "GAME-MAKER OS"; *s; s++) x = text(F8, x, 6, s, 1, C_GOLD, 1) + 2;
  text0(F5, 48, 26, "SANDBOX EDITION", 0xf8d450u);
  const char *nav = "Games";
  int32_t nw = width(F8, nav, len(nav), 1);
  text0(F8, (int32_t)GM_W - 16 - nw, 16, nav, C_GOLD);
  fill((int32_t)GM_W - 16 - nw, 27, nw, 2, C_GOLD);
}
static void tabs(const struct gm_menu *m) {
  box(16, TABS_Y, GM_W - 32, TABS_H, 8, 1, C_LINE2, C_PANEL, 1);
  unsigned n = 0;
  for (unsigned t = 0; t < GM_CATS; t++) if (m->tab_count[t] || (m->count && t>=GM_CAT_SEARCH)) n++;
  if (!n) return;
  int32_t slot = (int32_t)(GM_W - 40) / (int32_t)n, k = 0;
  for (unsigned t = 0; t < GM_CATS; t++) {
    if (!m->tab_count[t] && !(m->count && t>=GM_CAT_SEARCH)) continue;
    char count[8]; num(count, m->tab_count[t]);
    int32_t lw = width(F8, TAB_NAME[t], len(TAB_NAME[t]), 1), cw = width(F5, count, len(count), 1) + 10;
    int32_t cx = 20 + slot * k++ + slot / 2, x = cx - (lw + 6 + cw) / 2;
    int on = t == m->tab;
    if (on) round_rect(x - 5, TABS_Y + 4, lw + 6 + cw + 10, TABS_H - 8, 6, C_BLUE);
    if (on && m->tabs_focus) box(21+slot*(k-1),TABS_Y+2,slot-2,TABS_H-4,6,1,C_GOLD,0,0);
    text0(F8, x, TABS_Y + 12, TAB_NAME[t], on ? 0xffffffu : C_TEXT);
    round_rect(x + lw + 6, TABS_Y + 9, cw, 14, 7, on ? C_PANEL : C_PILL);
    text0(F5, x + lw + 11, TABS_Y + 12, count, on ? 0xffffffu : C_MUTED);
  }
}
void gm_card_rect(unsigned i, uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h) {
  *x = 16 + i % GM_COLS * (CARD_W + GAP_X); *y = GRID_Y + i / GM_COLS * (CARD_H + GAP_Y); *w = CARD_W; *h = CARD_H;
}
static void picture(int32_t x, int32_t y, const uint16_t *pic, int32_t w, int32_t h, int32_t r) {
  for (int32_t j = 0; j < h; j++) {
    int32_t k = inset(r, j);
    for (int32_t i = k; i < w - k; i++) dot16(x + i, y + j, pic[j * w + i]);
  }
}
/* Yellow five-pointed mark, above the title art as in Pocket Player. */
static void star(int32_t x, int32_t y) {
  static const uint16_t bits[]={0x40,0x40,0xe0,0xe0,0x1f0,0x1fff,0xffe,0x7fc,0x3f8,0x3f8,0x7fc,0x71c,0x606};
  round_rect(x-3,y-3,19,19,5,C_FOOT);
  for(unsigned j=0;j<13;j++)for(unsigned i=0;i<13;i++)if(bits[j]>>i&1)fill(x+(int32_t)i,y+(int32_t)j,1,1,C_GOLD);
}
static void card(const struct gm_menu *m, const struct gm_game *g, unsigned i, int on) {
  uint32_t ux, uy, uw, uh; gm_card_rect(i, &ux, &uy, &uw, &uh);
  int32_t x = (int32_t)ux, y = (int32_t)uy, w = (int32_t)uw, h = (int32_t)uh;
  if (on) box(x - 2, y - 2, w + 4, h + 4, 8, 3, C_BLUE, C_PANEL, 1);
  else box(x, y, w, h, 6, 1, C_LINE, C_PANEL, 1);
  const uint16_t *pic = g->pic > 0 ? gm_thumb_get((unsigned)(g - m->games)) : 0;
  if (pic) picture(x + 1, y + 1, pic, GM_THUMB_W, GM_THUMB_H, 5);
  else {
    for (int32_t j = 0; j < (int32_t)GM_THUMB_H; j++) {int32_t k = inset(5, j); fill(x + 1 + k, y + 1 + j, (int32_t)GM_THUMB_W - 2 * k, 1, C_EMPTY);}
    const char *say = g->pic < 0 ? "No title screen" : "Loading...";
    text0(F5, x + 1 + ((int32_t)GM_THUMB_W - width(F5, say, len(say), 1)) / 2, y + 1 + (int32_t)GM_THUMB_H / 2 - 4, say, C_DIM);
  }
  if (gm_is_favorite(m,g)) star(x+w-23,y+10);
  fill(x + 1, y + 1 + (int32_t)GM_THUMB_H, (int32_t)GM_THUMB_W, 1, C_LINE2);
  int32_t ty = y + 1 + (int32_t)GM_THUMB_H + 9;
  unsigned lines = wrap(F8, x + 9, ty, w - 18, g->title, 2, 11, 0xffffffu, 1);
  text0(F5, x + 9, ty + (int32_t)lines * 11 + 2, gm_company(m, g)[0] ? gm_company(m, g) : g->dir, C_DIM);
}
static void keycap(int32_t *x, int32_t y, const char *key, const char *label) {
  int32_t kw = width(F5, key, len(key), 1) + 10;
  box(*x, y - 3, kw, 15, 3, 1, C_LINE, C_PILL, 1);
  text0(F5, *x + 5, y, key, C_TEXT);
  *x = text0(F5, *x + kw + 5, y, label, C_MUTED) + 12;
}
static void footer(const struct gm_menu *m) {
  fill(0, FOOT_Y, GM_W, GM_H - FOOT_Y, C_FOOT);
  fill(0, FOOT_Y, GM_W, 1, C_LINE2);
  int32_t x = 16, y = FOOT_Y + 10;
  if (m->message[0]) text0(F8,x,y,m->message,C_GOLD);
  else if(m->options_active || m->open_keyboard) {
    keycap(&x,y,"Arrows","choose");keycap(&x,y,"OK/Enter","select");keycap(&x,y,"Back/Esc","cancel");
  } else if (m->search_active) {
    if(m->search_keyboard) {keycap(&x,y,"Arrows","choose a letter");keycap(&x,y,"OK/Enter","type / Done for results");}
    else {keycap(&x,y,"Type","title or company");keycap(&x,y,"Arrows","choose");keycap(&x,y,"OK/Enter","details");}
    keycap(&x,y,"Back/Esc","clear / leave");
  } else if (m->tabs_focus) {
    keycap(&x,y,"Left/Right","category");keycap(&x,y,"OK/Down","open");keycap(&x,y,"Up","Options");keycap(&x,y,"Back/Esc","back");
  } else if (m->view==GM_DETAILS) {
    keycap(&x,y,"Arrows","choose button");keycap(&x,y,"OK/Enter","press");keycap(&x,y,"F","Favorite");keycap(&x,y,"Back/Esc","back");
  } else {
    keycap(&x,y,"Arrows","choose / Up to tabs");keycap(&x,y,"OK/Enter","open");keycap(&x,y,"F","Favorite");keycap(&x,y,"Back/Esc","Options");
  }
  x=16;y=FOOT_Y+30;
  keycap(&x,y,"Tab / [ ]","category");keycap(&x,y,"/","folder name");
  if(m->search_active) {keycap(&x,y,"Keyboard","type title / company");keycap(&x,y,"Backspace","edit");}
  else {
    keycap(&x,y,"H","high scores");keycap(&x,y,"M",m->background_music?"music on":"music off");
    keycap(&x,y,"PgUp/PgDn","page");if(m->view==GM_GRID)keycap(&x,y,"Q","quit");
  }
}
static void search_keyboard(const struct gm_menu *m) {
  box(16,304,928,180,8,1,C_LINE,C_PANEL,1);
  text0(F5,28,314,m->open_keyboard?"Choose the folder alias, then Done to open. Back cancels.":"Search: choose letters, then Done to browse results.",C_MUTED);
  for(unsigned k=0;k<42;k++) {
    int32_t x,y,w;
    const char *label; char letter[2]={0,0};
    if(k<36) {x=28+(int32_t)(k%9)*101;y=330+(int32_t)(k/9)*29;w=94;letter[0]=GM_SEARCH_KEYS[k];label=letter;}
    else {x=28+(int32_t)(k-36)*151;y=448;w=144;label=osk_label(k,letter);}
    int on=k==m->keyboard_pos;
    box(x,y,w,24,4,1,on?C_GOLD:C_LINE,on?C_BLUE:C_PILL,1);
    text0(F8,x+(w-width(F8,label,len(label),1))/2,y+8,label,C_TEXT);
  }
}
static void grid(const struct gm_menu *m) {
  unsigned t = m->tab, n = m->tab_count[t], p = m->pos[t], first = p / GM_PAGE * GM_PAGE;
  if (!m->count) {
    const char *a = "No games in /sd/GAMES", *b = "Copy a game's folder there to see it here.";
    text(F8, ((int32_t)GM_W - width(F8, a, len(a), 2)) / 2, 236, a, len(a), C_TEXT, 2);
    text0(F5, ((int32_t)GM_W - width(F5, b, len(b), 1)) / 2, 272, b, C_MUTED);
    return;
  }
  if (t==GM_CAT_SEARCH) {
    box(16,85,650,23,5,1,m->search_active?C_GOLD:C_LINE,C_PANEL,1);
    int32_t sx=text0(F5,24,92,"Search: All games",C_MUTED);
    text0(F8,sx+14,92,m->query[0]?m->query:"Type a title or company",m->query[0]?C_TEXT:C_DIM);
  } else {
    int32_t x = text0(F8, 16, HEAD_Y, t==GM_CAT_RECENT?"Recently played":TAB_NAME[t], 0xffffffu);
    text0(F5, x + 10, HEAD_Y, TAB_LINE[t], C_MUTED);
  }
  if (!n) {
    const char *empty=t==GM_CAT_SEARCH?"No matching titles":t==GM_CAT_FAVORITES?"No favorites yet.":"No recently played games yet.";
    text0(F8,32,220,empty,C_TEXT);
    if(t==GM_CAT_FAVORITES)text0(F5,32,244,"Choose a game and press F, or open Details and choose Favorite.",C_MUTED);
  }
  if (n > GM_PAGE) {
    char line[24], *q = put(line, "Page "); q = num(q, p / GM_PAGE + 1); q = put(q, " of "); num(q, (n + GM_PAGE - 1) / GM_PAGE);
    text0(F5, (int32_t)GM_W - 16 - width(F5, line, len(line), 1), HEAD_Y, line, C_MUTED);
  }
  for (unsigned i = 0; i < (m->search_active && m->search_keyboard?GM_COLS:GM_PAGE) && first + i < n; i++) card(m, gm_tab_game(m, t, first + i), i, first + i == p);
  if(m->search_active && m->search_keyboard) search_keyboard(m);
}
uint32_t gm_view_work(uint32_t *divisions) {if (divisions) *divisions = work_divs; return work_stores;}
void gm_view_picture_loading(int on) {picture_loading = on;}
void gm_view_work_reset(void) {work_stores = work_divs = 0;}
static uint32_t mix(uint32_t a, uint32_t b, uint32_t k, uint32_t of) {
  uint32_t out = 0;
  for (int s = 16; s >= 0; s -= 8) {uint32_t x = a >> s & 255u, y = b >> s & 255u; work_divs++; out = out << 8 | (x * (of - k) + y * k) / of;}
  return out;
}
static void button(int32_t x, int32_t y, int32_t w, const char *label, int primary, int focus) {
  if (focus) box(x - 3, y - 3, w + 6, 36, 9, 2, 0xffffffu, 0, 0);
  if (primary) round_rect(x, y, w, 30, 6, C_BLUE);
  else box(x, y, w, 30, 6, 1, 0x8a91b4u, 0, 0);
  int32_t lw = width(F8, label, len(label), 1), tx = x + (w - lw) / 2;
  if (primary) { /* a play triangle before the label */
    tx += 7;
    for (int32_t j = 0; j < 11; j++) {int32_t half = j < 6 ? j : 10 - j; fill(tx - 15, y + 9 + j, half + 1, 1, 0xffffffu);}
  }
  text0(F8, tx, y + 11, label, 0xffffffu);
}
static void picture_area(const uint8_t *big, const uint8_t *big_pal) {
  if (big) { /* each pixel 2 x 2: crisp, and 1080p is exactly 2x again */
    uint16_t c16[256];
    for (unsigned i = 0; i < 256; i++) {const uint8_t *c = big_pal + i * 3; uint32_t v = 0; for (int k = 0; k < 3; k++) v = v << 8 | ((c[k] & 63u) << 2 | (c[k] & 63u) >> 4); c16[i] = gm_565(v);}
    for (int32_t j = 0; j < 400; j++) for (int32_t i = 0; i < 640; i++) dot16(PIC_X + i, PIC_Y + j, c16[big[(j >> 1) * 320 + (i >> 1)]]);
  } else {
    fill(PIC_X, PIC_Y, 640, 400, C_EMPTY);
    const char *say = picture_loading ? "Loading..." : "No title screen";
    text(F8, PIC_X + (640 - width(F8, say, len(say), 2)) / 2, PIC_Y + 192, say, len(say), C_DIM, 2);
  }
}
static void details(const struct gm_menu *m, const uint8_t *big, const uint8_t *big_pal) {
  const struct gm_game *g = gm_selected(m);
  int32_t px = 16, py = TABS_Y, pw = GM_W - 32, ph = FOOT_Y - 8 - TABS_Y;
  /* The panel's gradient: left to right once per column, then top to bottom per row. The blend is
     (x * (of - k) + y * k) / of, exact, with one reciprocal for the whole panel: a division per pixel
     was software (ARM1176 has none) and ran for over a second a frame (the 2 s heartbeat fault). */
  static uint32_t col[GM_W];
  const uint32_t of = (uint32_t)ph * 2u, bottom = 0x370d4eu;
  const uint64_t inv = (1ull << 40) / of + 1u;
  for (int32_t i = 0; i < pw && i < (int32_t)GM_W; i++) col[i] = mix(0x150e4cu, 0x3b2272u, (uint32_t)i, (uint32_t)pw);
  for (int32_t j = 0; j < ph; j++) {
    int32_t k = inset(12, j < ph - 1 - j ? j : ph - 1 - j);
    for (int32_t i = k; i < pw - k && i < (int32_t)GM_W; i++) {
      uint32_t c = 0, a = col[i];
      for (int s = 16; s >= 0; s -= 8) {uint32_t n = (a >> s & 255u) * (of - (uint32_t)j) + (bottom >> s & 255u) * (uint32_t)j; c = c << 8 | (uint32_t)((n * inv) >> 40);}
      dot_dither(px + i, py + j, c);
    }
  }
  if (!g) return;
  round_rect(PIC_X - 8, PIC_Y - 8, 656, 416, 10, C_FRAME);
  picture_area(big, big_pal);
  int32_t x = SIDE_X, w = SIDE_W, y = 62;
  const char *badge = BADGE[g->cat];
  int32_t bw = width(F8, badge, len(badge), 1) + 12;
  round_rect(x, y, bw, 17, 4, C_BADGE);
  text0(F8, x + 6, y + 5, badge, 0xffffffu);
  y += 28;
  unsigned lines = count_lines(F8, w, g->title, 3, 2);
  for (int32_t dy = -2; dy <= 2; dy += 2) for (int32_t dx = -2; dx <= 2; dx += 2) if (dx || dy) wrap(F8, x + dx, y + dy, w, g->title, 3, 20, C_OUTLINE, 2);
  wrap(F8, x, y, w, g->title, 3, 20, C_TITLE, 2);
  y += (int32_t)lines * 20 + 6;
  int32_t bx = text0(F5, x, y, gm_company(m, g)[0] ? gm_company(m, g) : g->dir, C_TEXT);
  if (g->year > 0) {
    char yr[12]; num(yr, (uint32_t)g->year);
    if (gm_company(m, g)[0]) {fill(bx + 3, y + 3, 2, 2, C_TEXT); bx += 9;}
    text0(F5, bx, y, yr, C_TEXT);
  }
  y += 22;
  button(x, y, 112, "Play", 1, m->button == GM_BTN_PLAY);
  button(x + 124, y, 88, "Back", 0, m->button == GM_BTN_BACK);
  const char *blurb = gm_blurb(m, g)[0] ? gm_blurb(m, g) : "This game has no entry in the library index, so there is no description to show.";
  button(x,y+40,212,gm_is_favorite(m,g)?"In Favorites":"Favorite",0,m->button==GM_BTN_FAVORITE);
  if(gm_is_favorite(m,g))star(x+12,y+49);
  button(x,y+80,212,"High scores",0,m->button==GM_BTN_SCORES);
  button(x,y+120,212,m->background_music?"Music: On":"Music: Off",0,m->button==GM_BTN_MUSIC);
  y += 162;
  wrap(F5, x, y, w, blurb, (unsigned)((440 - y) / 13), 13, C_TEXT, 1);
  char where[40], *q = put(where, "/sd/GAMES/"); q = put(q, g->dir); q = put(q, "/"); put(q, g->gam);
  text0(F5, x, 464, where, 0xb0a8d0u);
}
void gm_draw(uint16_t *px, uint32_t stride, const struct gm_menu *m, const uint8_t *big, const uint8_t *big_pal) {
  P = px; S = stride;
  fill(0, 0, GM_W, GM_H, C_PAGE);
  header();
  if (m->view == GM_DETAILS) details(m, big, big_pal);
  else {tabs(m); grid(m);}
  if(m->open_keyboard) {
    box(16,85,928,28,5,1,C_GOLD,C_PANEL,1);
    text0(F8,28,95,"Open folder (DOS alias):",C_TEXT);
    text0(F8,270,95,m->open_dir,C_GOLD);
    search_keyboard(m);
  }
  if(m->options_active) {
    static const char *const labels[GM_OPTIONS]={"Back to library","Favorite / Unfavorite","High scores","Music","Open by folder name","Quit Game-Maker OS","UI sounds","Screen off after","Power saving"};
    box(240,64,480,404,12,2,C_GOLD,C_PANEL,1);
    text0(F8,268,82,"Options",C_GOLD);
    for(unsigned i=0;i<GM_OPTIONS;i++) {
      const char *label=labels[i];
      if(i==GM_OPT_MUSIC) label=m->background_music?"Music: On":"Music: Off";
      if(i==GM_OPT_SOUNDS) label=m->settings.ui_sounds?"UI sounds: On":"UI sounds: Off";
      if(i==GM_OPT_SCREEN) label=gm_settings_screen_label(&m->settings);
      if(i==GM_OPT_POWER) label=m->settings.power_saving?"Power saving: On":"Power saving: Off";
      button(268,120+(int32_t)i*38,424,label,0,m->option==i);
    }
  }
  footer(m);
}
void gm_picture_rect(uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h) {*x = PIC_X; *y = PIC_Y; *w = 640; *h = 400;}
void gm_draw_picture(uint16_t *px, uint32_t stride, const uint8_t *big, const uint8_t *big_pal) {
  P = px; S = stride;
  picture_area(big, big_pal);
}
