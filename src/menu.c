/* SPDX-License-Identifier: MIT; original games list model (menu.h). */
#include "menu.h"
static void copy(char *d, const char *s, unsigned cap) {unsigned k = 0; while (s[k] && k < cap) {d[k] = s[k]; k++;} d[k] = 0;}
bool gm_add(struct gm_menu *m, const char *dir, const char *title) {
  if (m->count >= GM_MAX_GAMES) return false;
  struct gm_game *g = &m->games[m->count++];
  copy(g->dir, dir, 8); copy(g->title, title[0] ? title : dir, GM_TITLE_MAX);
  return true;
}
static int lower(int c) {return c >= 'A' && c <= 'Z' ? c + 32 : c;}
static int compare(const char *a, const char *b, bool fold) {
  while (*a && (fold ? lower(*a) == lower(*b) : *a == *b)) {a++; b++;}
  return fold ? lower((unsigned char)*a) - lower((unsigned char)*b) : (unsigned char)*a - (unsigned char)*b;
}
static int before(const struct gm_game *a, const struct gm_game *b) {
  int c = compare(a->title, b->title, true);
  return c ? c < 0 : compare(a->dir, b->dir, false) < 0;
}
void gm_sort(struct gm_menu *m) { /* insertion sort: at most 128 games */
  for (unsigned i = 1; i < m->count; i++) {
    struct gm_game g = m->games[i]; unsigned j = i;
    while (j && before(&g, &m->games[j - 1])) {m->games[j] = m->games[j - 1]; j--;}
    m->games[j] = g;
  }
  m->sel = m->top = 0;
}
static int move(struct gm_menu *m, int by) {
  if (!m->count) return GM_NONE;
  unsigned old = m->sel;
  if (by < 0 && m->sel) m->sel--;
  if (by > 0 && m->sel + 1 < m->count) m->sel++;
  if (m->sel < m->top) m->top = m->sel;
  if (m->sel >= m->top + GM_ROWS) m->top = m->sel - GM_ROWS + 1;
  if (m->sel == old) return GM_NONE;
  m->message[0] = 0; return GM_MOVED;
}
int gm_event(struct gm_menu *m, uint16_t type, uint16_t code, int32_t value) {
  if (type == 0 && code == 3) return GM_RESYNC; /* EV_SYN, SYN_DROPPED */
  if (type != 1 || !value) return GM_NONE;      /* key presses and repeats only */
  if (code == 103) return move(m, -1);          /* KEY_UP */
  if (code == 108) return move(m, 1);           /* KEY_DOWN */
  if (value != 1) return GM_NONE;               /* no repeats past here */
  if (code == 28) return m->count ? GM_CHOSEN : GM_NONE; /* KEY_ENTER */
  if (code == 1 || code == 16) return GM_QUIT;  /* KEY_ESC, KEY_Q */
  return GM_NONE;
}
