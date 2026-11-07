/* SPDX-License-Identifier: MIT; original games list model (menu.h). */
#include "menu.h"
static void copy(char *d, const char *s, unsigned cap) {unsigned k = 0; while (s[k] && k < cap) {d[k] = s[k]; k++;} d[k] = 0;}
static int lower(int c) {return c >= 'A' && c <= 'Z' ? c + 32 : c;}
static int compare(const char *a, const char *b, bool fold) {
  while (*a && (fold ? lower(*a) == lower(*b) : *a == *b)) {a++; b++;}
  return fold ? lower((unsigned char)*a) - lower((unsigned char)*b) : (unsigned char)*a - (unsigned char)*b;
}
unsigned gm_menu_bytes(void) {return sizeof(struct gm_menu);}
bool gm_dir_ok(const char *name, unsigned n) {
  if (n < 1 || n > 8) return false;
  for (unsigned i = 0; i < n; i++) if (!name[i] || name[i] == '/' || name[i] == '.') return false;
  return true;
}
bool gm_add(struct gm_menu *m, const char *dir, const char *gam, const char *title, uint32_t crc) {
  if (m->count >= GM_MAX_GAMES) return false;
  struct gm_game *g = &m->games[m->count++];
  copy(g->dir, dir, 8); copy(g->gam, gam, 12); copy(g->title, title[0] ? title : dir, GM_LIB_TITLE);
  g->company[0] = g->blurb[0] = 0; g->year = g->course = 0; g->crc = crc; g->cat = GM_CAT_OTHER; g->pic = 0;
  return true;
}
static int same_dos(const char *dos, const char *gam) { /* "DICE" and "DICE.GAM" */
  unsigned k = 0;
  while (dos[k] && gam[k] && lower(dos[k]) == lower(gam[k])) k++;
  return k && !dos[k] && gam[k] == '.';
}
static uint8_t category(const struct gm_record *r) {
  const char *c = r->category[0];
  return compare(c, "starter", false) == 0 ? GM_CAT_STARTER : compare(c, "sandbox", false) == 0 ? GM_CAT_SANDBOX :
         compare(c, "bizarre", false) == 0 ? GM_CAT_BIZARRE : compare(c, "cd", false) == 0 ? GM_CAT_CD : GM_CAT_OTHER;
}
int gm_library_line(struct gm_menu *m, const char *line, uint32_t n) {
  static struct gm_record r;
  if (!gm_record_parse(line, n, &r)) return 0;
  struct gm_game *hit = 0;
  for (unsigned i = 0; i < m->count && !hit; i++) if (r.has_crc && m->games[i].crc == r.crc) hit = &m->games[i];
  for (unsigned i = 0; i < m->count && !hit; i++) if (r.locator[0] && compare(r.locator, m->games[i].dir, true) == 0) hit = &m->games[i];
  for (unsigned i = 0; i < m->count && !hit; i++) if (r.dos[0] && same_dos(r.dos, m->games[i].gam)) hit = &m->games[i];
  if (!hit) return 0;
  copy(hit->title, r.title, GM_LIB_TITLE); copy(hit->company, r.company, GM_LIB_COMPANY); copy(hit->blurb, r.blurb, GM_LIB_BLURB);
  hit->year = r.year; hit->course = r.course; hit->cat = category(&r);
  return 1;
}
/* Within a tab: by course (Starters), then title ignoring case, then folder. */
static int before(const struct gm_game *a, const struct gm_game *b) {
  if (a->cat != b->cat) return a->cat < b->cat;
  int32_t ca = a->course ? a->course : 0x7fffffff, cb = b->course ? b->course : 0x7fffffff;
  if (ca != cb) return ca < cb;
  int c = compare(a->title, b->title, true);
  if (!c) c = compare(a->dir, b->dir, false);
  return c < 0;
}
void gm_finish(struct gm_menu *m) {
  for (unsigned i = 0; i < m->count; i++) {
    uint16_t g = (uint16_t)i; unsigned j = i;
    while (j && before(&m->games[g], &m->games[m->order[j - 1]])) {m->order[j] = m->order[j - 1]; j--;}
    m->order[j] = g;
  }
  for (unsigned t = 0; t < GM_CATS; t++) {m->tab_first[t] = 0; m->tab_count[t] = 0; m->pos[t] = 0;}
  for (unsigned i = m->count; i--;) {unsigned t = m->games[m->order[i]].cat; m->tab_first[t] = i; m->tab_count[t]++;}
  m->tab = GM_CAT_SANDBOX;
  if (!m->tab_count[m->tab]) for (unsigned t = 0; t < GM_CATS; t++) if (m->tab_count[t]) {m->tab = t; break;}
  m->view = GM_GRID; m->button = GM_BTN_PLAY; m->shift = 0; m->message[0] = 0;
}
unsigned gm_tab_size(const struct gm_menu *m, unsigned tab) {return tab < GM_CATS ? m->tab_count[tab] : 0;}
const struct gm_game *gm_tab_game(const struct gm_menu *m, unsigned tab, unsigned i) {
  return i < gm_tab_size(m, tab) ? &m->games[m->order[m->tab_first[tab] + i]] : 0;
}
const struct gm_game *gm_selected(const struct gm_menu *m) {return gm_tab_game(m, m->tab, m->pos[m->tab]);}
static int set_pos(struct gm_menu *m, unsigned p) {
  if (p == m->pos[m->tab]) return GM_NONE;
  m->pos[m->tab] = p; m->message[0] = 0; return GM_MOVED;
}
static int grid_move(struct gm_menu *m, uint16_t code) {
  unsigned n = m->tab_count[m->tab], p = m->pos[m->tab];
  if (!n) return GM_NONE;
  switch (code) {
  case 105: return p ? set_pos(m, p - 1) : GM_NONE;                    /* KEY_LEFT */
  case 106: return p + 1 < n ? set_pos(m, p + 1) : GM_NONE;            /* KEY_RIGHT */
  case 103: return p >= GM_COLS ? set_pos(m, p - GM_COLS) : GM_NONE;   /* KEY_UP */
  case 108:                                                            /* KEY_DOWN: the card below, or the last one when the row below is short */
    if (p + GM_COLS < n) return set_pos(m, p + GM_COLS);
    return p / GM_COLS < (n - 1) / GM_COLS ? set_pos(m, n - 1) : GM_NONE;
  case 104: return set_pos(m, p >= GM_PAGE ? p - GM_PAGE : 0);         /* KEY_PAGEUP */
  case 109: return set_pos(m, p + GM_PAGE < n ? p + GM_PAGE : n - 1);  /* KEY_PAGEDOWN */
  case 102: return set_pos(m, 0);                                      /* KEY_HOME */
  case 107: return set_pos(m, n - 1);                                  /* KEY_END */
  }
  return GM_NONE;
}
static int next_tab(struct gm_menu *m, int by) {
  unsigned t = m->tab;
  for (unsigned k = 0; k < GM_CATS; k++) {
    t = (t + GM_CATS + (unsigned)by) % GM_CATS;
    if (m->tab_count[t]) break;
  }
  if (t == m->tab) return GM_NONE;
  m->tab = t; m->message[0] = 0; return GM_MOVED;
}
int gm_event(struct gm_menu *m, uint16_t type, uint16_t code, int32_t value) {
  if (type == 0 && code == 3) return GM_RESYNC;  /* EV_SYN, SYN_DROPPED */
  if (type != 1) return GM_NONE;
  if (code == 42 || code == 54) {m->shift = value != 0; return GM_NONE;} /* KEY_LEFTSHIFT, KEY_RIGHTSHIFT */
  if (!value) return GM_NONE;                    /* key presses and repeats only */
  if (m->view == GM_DETAILS) {
    unsigned b = m->button;
    if (code == 105 && b) b--;                                         /* Left */
    else if (code == 106 && b + 1 < GM_BUTTONS) b++;                   /* Right */
    else if (code == 15) b = (b + (m->shift ? GM_BUTTONS - 1 : 1)) % GM_BUTTONS; /* Tab, Shift+Tab */
    if (b != m->button) {m->button = b; return GM_MOVED;}
    if (value != 1) return GM_NONE;
    if (code == 28 || code == 57) {             /* Enter, Space */
      if (m->button == GM_BTN_PLAY) return GM_CHOSEN;
      m->view = GM_GRID; return GM_MOVED;
    }
    if (code == 1 || code == 14) {m->view = GM_GRID; return GM_MOVED;} /* Esc, Backspace */
    return GM_NONE;
  }
  if (code == 15) return next_tab(m, m->shift ? -1 : 1); /* KEY_TAB (repeats too) */
  if (code == 27) return next_tab(m, 1);          /* ] */
  if (code == 26) return next_tab(m, -1);         /* [ */
  int r = grid_move(m, code);
  if (r != GM_NONE || value != 1) return r;      /* no repeats past here */
  if (code == 28 && gm_selected(m)) {m->view = GM_DETAILS; m->button = GM_BTN_PLAY; m->message[0] = 0; return GM_MOVED;} /* KEY_ENTER */
  if (code == 1 || code == 16) return GM_QUIT;   /* KEY_ESC, KEY_Q */
  return GM_NONE;
}
