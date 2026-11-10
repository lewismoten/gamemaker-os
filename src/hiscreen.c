/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * hiscreen.h. Texts and positions are HISCOREL.C's. Claude Sonnet 5.5 wrote this file. */
#include "hiscreen.h"
#include "text.h"
#include "osk.h"
#define K_ESC 1
#define K_BACK 14
#define K_ENTER 28
#define K_LSHIFT 42
#define K_RSHIFT 54
#define K_LEFT 105
#define K_TAB 15
#define K_LCTRL 29
#define K_RCTRL 97
#define NS_MS 1000000ull
static const char ROW_NUM[] = "1234567890-=", ROW_NUM_S[] = "!@#$%^&*()_+";
static const char ROW_Q[] = "qwertyuiop[]", ROW_Q_S[] = "QWERTYUIOP{}";
static const char ROW_A[] = "asdfghjkl;'`", ROW_A_S[] = "ASDFGHJKL:\"~";
static const char ROW_Z[] = "zxcvbnm,./", ROW_Z_S[] = "ZXCVBNM<>?";
int hs_ascii(uint16_t c, int sh) {
  if (c >= 2 && c <= 13) return (sh ? ROW_NUM_S : ROW_NUM)[c - 2];
  if (c >= 16 && c <= 27) return (sh ? ROW_Q_S : ROW_Q)[c - 16];
  if (c >= 30 && c <= 41) return (sh ? ROW_A_S : ROW_A)[c - 30];
  if (c >= 44 && c <= 53) return (sh ? ROW_Z_S : ROW_Z)[c - 44];
  if (c == 43) return sh ? '|' : '\\';
  if (c == 57) return ' ';
  if (c == K_TAB) return 9;
  return 0;
}
/* The byte bioskey(0) gives with Ctrl held (low byte of the key word): letters 1..26, Ctrl+[ 27, Ctrl+\ 28, Ctrl+] 29. */
static int ctrl_byte(uint16_t c) {
  int a = hs_ascii(c, 0);
  if (a >= 'a' && a <= 'z') return a - 'a' + 1;
  return a == '[' ? 27 : a == '\\' ? 28 : a == ']' ? 29 : 0;
}
int32_t hs_entry_score(int32_t score, int can_write) {return can_write || score <= 0 ? score : -1;}
void hs_modifiers(struct hs_screen *h, const uint64_t *keys) {
  h->shift = (keys[K_LSHIFT / 64] >> (K_LSHIFT % 64) | keys[K_RSHIFT / 64] >> (K_RSHIFT % 64)) & 1;
  h->ctrl = (keys[K_LCTRL / 64] >> (K_LCTRL % 64) | keys[K_RCTRL / 64] >> (K_RCTRL % 64)) & 1;
}
void hs_start(struct hs_screen *h, const struct his_table *t, int32_t score, uint64_t now) {
  h->t = *t; h->score = score; h->place = score > 0 ? his_place(t, score) + 1 : 0; h->shift = 0; h->ctrl = 0; h->cur_on = 1;
  for (unsigned i = 0; i <= HIS_NAME_MAX; i++) h->name[i] = 0;
  h->keyboard=1; h->keyboard_pos=0;
  h->len = 0; h->t0 = h->blink = now;
  h->stage = h->place ? HS_NAME : HS_TABLE;
}
static int accept(struct hs_screen *h, uint64_t now) { /* Enter or Esc: GGet returns, the row keeps the name as typed */
  his_insert(&h->t, h->score, h->name);
  h->stage = HS_TABLE; h->t0 = now;
  return HS_COMMIT | HS_DRAW;
}
int hs_event(struct hs_screen *h, uint16_t type, uint16_t code, int32_t value, uint64_t now) {
  if (type != 1) return 0;
  if (code == K_LSHIFT || code == K_RSHIFT) {h->shift = value != 0; return 0;}
  if (code == K_LCTRL || code == K_RCTRL) {h->ctrl = value != 0; return 0;}
  if (value == 0) return 0;
  if (h->stage == HS_TABLE) return value == 1 ? HS_DONE : 0; /* "Hit any key" */
  h->blink = now; h->cur_on = 1;
  if(!h->keyboard && (code==103 || code==108)) {h->keyboard=1;return HS_DRAW;}
  if(h->keyboard && (code==103 || code==108 || code==105 || code==106 || code==28)) {
    int ch=osk_press(&h->keyboard_pos,code,value);
    if(ch==13)return accept(h,now);
    if(ch)osk_edit(h->name,HIS_NAME_MAX,ch);
    h->len=0;while(h->name[h->len])h->len++;
    return HS_DRAW;
  }
  int c = h->ctrl ? ctrl_byte(code) : hs_ascii(code, h->shift);
  if(c || code==K_BACK)h->keyboard=0;
  if (code == K_ESC || code == K_ENTER || code == 96 || c == 27 || c == 13) return value == 1 ? accept(h, now) : 0; /* 27, 13 */
  if (code == K_BACK || code == K_LEFT || c == 8) {if (h->len) h->name[--h->len] = 0; return HS_DRAW;} /* 8; Left: DOS erases too */
  /* Right (RIGHTKEY) is "if (ans[ctr] != 0) ctr++", and ans[ctr] is always 0 here because Left and
     Backspace truncate the answer and a new entry's starts empty: it never moves, so it is ignored.
     Every other byte GGet's default case takes: any key byte but 0, 8, 13 and 27, controls included. */
  if (c && h->len < HIS_NAME_MAX) {h->name[h->len++] = (char)c; h->name[h->len] = 0;}
  return HS_DRAW;
}
/* GGet's cursor: shown from a key press for 500 ms, hidden 400 ms, again. */
static int cursor_on(const struct hs_screen *h, uint64_t now) {return (now - h->blink) / NS_MS % 900u < 500u;}
int hs_step(struct hs_screen *h, uint64_t now) {
  if (h->stage == HS_TABLE) return now - h->t0 >= HS_PAUSE_NS ? HS_DONE : 0;
  int on = cursor_on(h, now);
  if (on == h->cur_on) return 0;
  h->cur_on = on; return HS_DRAW;
}
uint64_t hs_wake(const struct hs_screen *h, uint64_t now) {
  return h->stage == HS_TABLE ? h->t0 + HS_PAUSE_NS : now + 20 * NS_MS;
}
static unsigned slen(const char *s) {unsigned n = 0; while (s[n]) n++; return n;}
static void write(uint8_t *px, int x, int y, uint8_t c, const char *s) {gp_write(px, x, y, c, s, slen(s));}
static char *cat(char *p, const char *s) {while (*s) *p++ = *s++; return p;}
static char *dec(char *p, int32_t v, unsigned width) { /* %<width>ld */
  char t[12]; unsigned k = 0; uint32_t u = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
  do t[k++] = (char)('0' + u % 10); while (u /= 10);
  if (v < 0) t[k++] = '-';
  for (unsigned i = k; i < width; i++) *p++ = ' ';
  while (k) *p++ = t[--k];
  return p;
}
static void box(uint8_t *px, int x0, int y0, int x1, int y1, uint8_t c) { /* Box: an outline */
  for (int x = x0; x <= x1; x++) if (x >= 0 && x < (int)HS_W) {px[y0 * HS_W + x] = c; px[y1 * HS_W + x] = c;}
  for (int y = y0; y <= y1; y++) if (x1 < (int)HS_W) {px[y * HS_W + x0] = c; px[y * HS_W + x1] = c;}
}
void hs_draw(const struct hs_screen *h, uint8_t *px, uint8_t *pal, const uint8_t *back, const uint8_t *back_pal) {
  hs_draw_colors(h, px, pal, back, back_pal, 0);
}
void hs_draw_colors(const struct hs_screen *h, uint8_t *px, uint8_t *pal, const uint8_t *back, const uint8_t *back_pal, const uint8_t *colors) {
  int pic = back && h->stage == HS_TABLE;
  for (unsigned i = 0; i < 768; i++) pal[i] = pic ? back_pal[i] : colors ? colors[i] : 0;
  for (unsigned i = 0; i < HS_W * HS_H; i++) px[i] = pic ? back[i] : 0;
  if (!pic && !colors) { /* the DOS palette is the game's colors; here black with the two colors it matches */
    pal[3] = 63; pal[4] = 10; pal[5] = 10; pal[6] = 10; pal[7] = 10; pal[8] = 63;
  }
  uint8_t red = gp_match(pal, 63, 10, 10), blue = gp_match(pal, 10, 10, 63);
  char s[64], *p;
  if (h->stage == HS_NAME) {
    write(px, 96, 40, red, "Congratulations!");
    p = dec(cat(s, "You scored number "), h->place, 2); p = cat(p, " on the Top Ten!"); *p = 0;
    write(px, 0, h->keyboard?64:100, blue, s);
    if(h->keyboard) {osk_draw(px,h->keyboard_pos,h->name,blue,0);return;}
    write(px, 0, 120, red, "Enter Your Name:");
    write(px, 136, 120, blue, h->name);
    if (h->cur_on) {
      if (h->len < HIS_NAME_MAX) box(px, 136 + (int)h->len * 8, 120, 136 + (int)(h->len + 1) * 8 - 2, 127, blue);
      else for (int y = 120; y < 128; y++) px[y * HS_W + 136 + (int)h->len * 8 - 1] = blue;
    }
    return;
  }
  write(px, 104, 0, red, "Top Ten Scores");
  int space = 22, y = 27;
  for (unsigned j = 0; j < HIS_ROWS; j++) {
    if (h->t.row[j].score != 0) {
      p = dec(s, h->t.row[j].score, 10); *p++ = ' ';
      for (unsigned k = 0; k < HIS_NAME && h->t.row[j].name[k] && p < s + 60; k++) *p++ = (char)h->t.row[j].name[k];
      *p = 0; write(px, 20, y, blue, s);
    }
    y += space; if (j % 2 == 0) space--; space--;
  }
  if (h->score > 0) {p = cat(dec(s, h->score, 10), " Your Latest Score"); *p = 0; write(px, 20, y, blue, s);}
  write(px, 116, 192, red, "Hit any key");
}
