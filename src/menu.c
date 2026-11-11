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
void gm_game_key(struct gm_game *g, const char *path, const char *gam) {
  uint32_t h=2166136261u;
  const char *parts[3]={path,"/",gam};
  for (unsigned i=0;i<3;i++) for(const char *p=parts[i];*p;p++) h=(h^(uint8_t)*p)*16777619u;
  for(unsigned i=0;i<8;i++) g->key[i]="0123456789ABCDEF"[(h>>(28-4*i))&15u];
  g->key[8]=0;
}
bool gm_add(struct gm_menu *m, const char *dir, const char *gam, const char *title, uint32_t crc) {
  if (m->count >= GM_MAX_GAMES) return false;
  struct gm_game *g = &m->games[m->count++];
  copy(g->dir, dir, 8); copy(g->gam, gam, 12);
  gm_game_key(g,dir,gam);
  if (title[0]) copy(g->title, title, GM_LIB_TITLE);
  else {
    /* Display only: keep launch paths and legacy identity fields untouched.
     * Strip the last extension BEFORE clipping a long basename. */
    const char *base=dir, *end=0;
    for (const char *p=dir; *p; p++) if (*p=='/') base=p+1;
    for (const char *p=base; *p; p++) if (*p=='.' && p!=base) end=p;
    unsigned n=0;
    while (base[n] && base+n!=end && n<GM_LIB_TITLE) {g->title[n]=base[n];n++;}
    g->title[n]=0;
  }
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
  for (unsigned t = 0; t < GM_CATS; t++) {m->tab_count[t] = 0; m->pos[t] = 0;}
  for (unsigned i = m->count; i--;) {unsigned t = m->games[m->order[i]].cat; m->tab_count[t]++;}
  m->tab = GM_CAT_SANDBOX;
  if (!m->tab_count[m->tab]) for (unsigned t = 0; t < GM_CATS; t++) if (m->tab_count[t]) {m->tab = t; break;}
  m->view = GM_GRID; m->button = GM_BTN_PLAY; m->shift = 0; m->message[0] = 0; m->background_music = 1;
  m->options_active=m->open_keyboard=0; m->option=0;
  gm_settings_defaults(&m->settings);
  m->open_active = 0; m->open_dir[0] = 0;
  m->query[0] = 0; m->search_active = m->tabs_focus = m->search_keyboard = 0; m->keyboard_pos=0;
  gm_refresh(m);
}
static int contains(const char *text, const char *query) {
  if (!*query) return 1;
  for (;*text;text++) {
    const char *a=text,*b=query;
    while (*a && *b && lower(*a)==lower(*b)) {a++;b++;}
    if (!*b) return 1;
  }
  return 0;
}
void gm_refresh(struct gm_menu *m) {
  for (unsigned t=0;t<GM_CATS;t++) m->tab_count[t]=0;
  for (unsigned i=0;i<m->count;i++) {
    unsigned id=m->order[i], t=m->games[id].cat;
    m->visible[t][m->tab_count[t]++]=(uint16_t)id;
    const struct gm_game *g=&m->games[id];
    if (contains(g->title,m->query)||contains(g->company,m->query))
      m->visible[GM_CAT_SEARCH][m->tab_count[GM_CAT_SEARCH]++]=(uint16_t)id;
  }
  for (unsigned t=GM_CAT_FAVORITES;t<=GM_CAT_RECENT;t++) {
    const struct gm_list *l=t==GM_CAT_FAVORITES?&m->favorites:&m->recent;
    for (unsigned k=0;k<l->count;k++) for (unsigned i=0;i<m->count;i++)
      if (!compare(l->key[k],m->games[i].key,false)) {m->visible[t][m->tab_count[t]++]=(uint16_t)i;break;}
  }
  for (unsigned t=0;t<GM_CATS;t++) if (m->pos[t]>=m->tab_count[t]) m->pos[t]=m->tab_count[t]?m->tab_count[t]-1:0;
}
void gm_search(struct gm_menu *m, const char *query) {
  copy(m->query,query,24); m->pos[GM_CAT_SEARCH]=0; gm_refresh(m);
}
int gm_is_favorite(const struct gm_menu *m, const struct gm_game *g) {return g && gm_list_has(&m->favorites,g->key)>=0;}
int gm_favorite(struct gm_menu *m) {
  const struct gm_game *g=gm_selected(m); if (!g) return GM_NONE;
  int r=gm_list_toggle(&m->favorites,g->key);
  copy(m->message,r<0?"Favorites are full (32): unstar one first":r?"Added to Favorites":"Removed from Favorites",GM_MESSAGE_MAX);
  gm_refresh(m); return r<0?GM_MOVED:GM_FAVORITE;
}
int gm_played(struct gm_menu *m, unsigned id) {
  if (id>=m->count) return 0;
  int selected=m->tab==GM_CAT_RECENT && gm_selected(m)==&m->games[id];
  int changed=gm_list_recent(&m->recent,m->games[id].key); gm_refresh(m);
  if(selected)m->pos[GM_CAT_RECENT]=0;
  return changed;
}
unsigned gm_tab_size(const struct gm_menu *m, unsigned tab) {return tab < GM_CATS ? m->tab_count[tab] : 0;}
const struct gm_game *gm_tab_game(const struct gm_menu *m, unsigned tab, unsigned i) {
  return i < gm_tab_size(m, tab) ? &m->games[m->visible[tab][i]] : 0;
}
const struct gm_game *gm_selected(const struct gm_menu *m) {
  if (m->view==GM_DETAILS) return m->detail_id<m->count?&m->games[m->detail_id]:0;
  return gm_tab_game(m, m->tab, m->pos[m->tab]);
}
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
    if (m->tab_count[t] || (m->count && t>=GM_CAT_SEARCH)) break;
  }
  if (t == m->tab) return GM_NONE;
  m->tab = t; m->search_keyboard=0; m->search_active=t==GM_CAT_SEARCH && !m->tabs_focus; m->message[0] = 0; return GM_MOVED;
}
int gm_open_dir(struct gm_menu *m, const char *dir) {
  for (unsigned t = 0; t < GM_CAT_SEARCH; t++) for (unsigned i = 0; i < m->tab_count[t]; i++) {
    const struct gm_game *g = gm_tab_game(m, t, i);
    if (compare(g->dir, dir, true)) continue;
    m->detail_id=(unsigned)(g-m->games);
    m->tab = t; m->pos[t] = i; m->view = GM_DETAILS;
    m->button = GM_BTN_PLAY; m->message[0] = 0; m->shift = 0;
    return GM_OPEN_FOUND;
  }
  copy(m->message, "Open game: not found", GM_MESSAGE_MAX);
  return GM_OPEN_MISSING;
}
/* Linux key codes, independent of keyboard layout; requests use DOS aliases.
 * Consume command letters before shortcuts such as Q, H and M. */
static int open_event(struct gm_menu *m, uint16_t code) {
  if(m->open_keyboard && (code==103 || code==108 || code==105 || code==106 || code==28)) {
    int c=osk_press(&m->keyboard_pos,code,1);
    if(c==13) {m->open_active=m->open_keyboard=0;return gm_open_dir(m,m->open_dir);}
    if(c)osk_edit(m->open_dir,8,c);
    return GM_MOVED;
  }
  static const char letters[] = "QWERTYUIOPASDFGHJKLZXCVBNM";
  static const uint8_t codes[] = {16,17,18,19,20,21,22,23,24,25,30,31,32,33,34,35,36,37,38,44,45,46,47,48,49,50};
  unsigned n = 0; while (m->open_dir[n]) n++;
  if (code == 1) {m->open_active=m->open_keyboard=0; m->message[0] = 0; return GM_MOVED;}
  if (code == 28) {
    int play = m->shift; m->open_active = 0;
    int result = gm_open_dir(m, m->open_dir);
    return play && result == GM_OPEN_FOUND ? GM_OPEN_PLAY : result;
  }
  if (code == 14) {if (n) m->open_dir[--n] = 0;}
  else {
    char c = 0;
    for (unsigned i = 0; i < sizeof codes; i++) if (code == codes[i]) c = letters[i];
    if (code >= 2 && code <= 11) c = "1234567890"[code - 2];
    if (code == 12) c = m->shift ? '_' : '-';
    if (!c || n == sizeof m->open_dir - 1) return GM_NONE;
    m->open_dir[n++] = c; m->open_dir[n] = 0;
  }
  copy(m->message, "Open game: ", GM_MESSAGE_MAX);
  copy(m->message + 11, m->open_dir, 9);
  return GM_MOVED;
}
static int search_event(struct gm_menu *m, uint16_t code, int32_t value) {
  if (code==1) {
    if(value!=1)return GM_NONE;
    if (m->query[0]) gm_search(m, "");
    else {m->search_active=m->search_keyboard=0;m->tabs_focus=1;}
    return GM_MOVED;
  }
  if (m->search_keyboard && (code==103 || code==108 || code==105 || code==106 || code==28)) {
    if(code==103 && m->keyboard_pos<9) {m->search_active=m->search_keyboard=0;m->tabs_focus=1;return GM_MOVED;}
    int c=osk_press(&m->keyboard_pos,code,value);
    if(c==13) {m->search_keyboard=0;return GM_MOVED;}
    if(c) {osk_edit(m->query,24,c);m->pos[GM_CAT_SEARCH]=0;gm_refresh(m);}
    return GM_MOVED;
  }
  if (code==103 || code==108 || code==105 || code==106) {
    if (code==103 && m->pos[m->tab]%GM_PAGE<GM_COLS) {m->search_active=0;m->tabs_focus=1;return GM_MOVED;}
    return grid_move(m,code);
  }
  if (code==28 && value==1 && gm_selected(m)) {
    m->detail_id=(unsigned)(gm_selected(m)-m->games);m->search_active=0;m->view=GM_DETAILS;m->button=GM_BTN_PLAY;return GM_MOVED;
  }
  if (value!=1) return GM_NONE;
  unsigned n=0;while(m->query[n]) n++;
  if(code==14) {if(n)m->query[--n]=0;}
  else {
    static const char letters[]="qwertyuiopasdfghjklzxcvbnm";
    static const uint8_t codes[]={16,17,18,19,20,21,22,23,24,25,30,31,32,33,34,35,36,37,38,44,45,46,47,48,49,50};
    char c=0;
    for(unsigned i=0;i<sizeof codes;i++) if(code==codes[i]) c=letters[i];
    if(code>=2&&code<=11)c="1234567890"[code-2];
    if(code==57)c=' ';
    if(code==12)c=m->shift?'_':'-';
    if(code==40)c='\'';
    if(code==52)c='.';
    if(!c||n==24)return GM_NONE;
    m->search_keyboard=0;
    m->query[n++]=c;m->query[n]=0;
  }
  m->pos[GM_CAT_SEARCH]=0;gm_refresh(m);return GM_MOVED;
}
int gm_event(struct gm_menu *m, uint16_t type, uint16_t code, int32_t value) {
  if (type == 0 && code == 3) {m->options_active = m->open_keyboard = m->open_active = m->search_active = m->tabs_focus = m->search_keyboard = 0; m->message[0] = 0; return GM_RESYNC;}  /* EV_SYN, SYN_DROPPED */
  if (type != 1) return GM_NONE;
  if (code == 42 || code == 54) {m->shift = value != 0; return GM_NONE;} /* KEY_LEFTSHIFT, KEY_RIGHTSHIFT */
  if (!value) return GM_NONE;                    /* key presses and repeats only */
  /* Text entry owns letters; Options otherwise keeps the M shortcut live. */
  if (!m->open_active && !m->search_active && code == 50 && value == 1) {
    m->background_music = !m->background_music; return GM_MUSIC;
  }
  if(m->options_active) {
    if(code==1 && value==1) {m->options_active=0;return GM_MOVED;}
    if(code==103 || code==105) {m->option=(m->option+GM_OPTIONS-1)%GM_OPTIONS;return GM_MOVED;}
    if(code==108 || code==106) {m->option=(m->option+1)%GM_OPTIONS;return GM_MOVED;}
    if(code!=28 || value!=1)return GM_NONE;
    unsigned option=m->option;m->options_active=0;
    if(option==GM_OPT_SCREEN || option==GM_OPT_POWER) {
      m->options_active=1;
      if(option==GM_OPT_SCREEN) m->settings.screen_off=(m->settings.screen_off+1)%4;
      else m->settings.power_saving=!m->settings.power_saving;
      return GM_SETTINGS;
    }
    if(option==GM_OPT_FAVORITE) {int r=gm_favorite(m);return r==GM_NONE?GM_MOVED:r;}
    if(option==GM_OPT_SCORES)return gm_selected(m)?GM_SCORES:GM_MOVED;
    if(option==GM_OPT_MUSIC) {m->background_music=!m->background_music;return GM_MUSIC;}
    if(option==GM_OPT_QUIT)return GM_QUIT;
    if(option==GM_OPT_OPEN) {
      m->search_active=m->tabs_focus=m->search_keyboard=0;
      m->open_active=m->open_keyboard=1;m->keyboard_pos=0;m->open_dir[0]=m->message[0]=0;
    }
    return GM_MOVED;
  }
  if (code == 53 && value == 1) {
    m->open_keyboard=0;
    m->search_active=m->tabs_focus=m->search_keyboard=0; m->open_active = 1; m->open_dir[0] = 0;
    copy(m->message, "Open game: ", GM_MESSAGE_MAX); return GM_MOVED;
  }
  if (m->open_active) return value == 1 ? open_event(m, code) : GM_NONE;
  if (m->view==GM_GRID && (code==15 || code==26 || code==27)) return next_tab(m,code==26 || (code==15 && m->shift)?-1:1);
  if (m->search_active) return search_event(m,code,value);
  if (m->view==GM_GRID && m->tabs_focus) {
    if(code==103) {m->options_active=1;m->option=0;return GM_MOVED;}
    if (code==105 || code==106) return next_tab(m,code==105?-1:1);
    if (code==108 || code==28 || code==1) {
      m->tabs_focus=0; m->search_active=m->tab==GM_CAT_SEARCH;
      m->search_keyboard=m->search_active && code!=1; m->keyboard_pos=0; return GM_MOVED;
    }
  }
  if (code==33 && value==1) return gm_favorite(m); /* F */
  if (m->view == GM_DETAILS) {
    unsigned b = m->button;
    if ((code == 105 || code == 103) && b) b--;                                         /* Left */
    else if ((code == 106 || code == 108) && b + 1 < GM_BUTTONS) b++;                   /* Right */
    else if (code == 15) b = (b + (m->shift ? GM_BUTTONS - 1 : 1)) % GM_BUTTONS; /* Tab, Shift+Tab */
    if (b != m->button) {m->button = b; return GM_MOVED;}
    if (value != 1) return GM_NONE;
    if (code == 28 || code == 57) {             /* Enter, Space */
      if (m->button == GM_BTN_PLAY) return GM_CHOSEN;
      if (m->button == GM_BTN_FAVORITE) return gm_favorite(m);
      if (m->button == GM_BTN_SCORES) return GM_SCORES;
      if (m->button == GM_BTN_MUSIC) {m->background_music=!m->background_music;return GM_MUSIC;}
      m->view = GM_GRID; m->search_active=m->tab==GM_CAT_SEARCH; return GM_MOVED;
    }
    if (code == 35) return GM_SCORES;           /* H */
    if (code == 1 || code == 14) {m->view = GM_GRID; m->search_active=m->tab==GM_CAT_SEARCH; return GM_MOVED;} /* Esc, Backspace */
    return GM_NONE;
  }
  if (code == 15) return next_tab(m, m->shift ? -1 : 1); /* KEY_TAB (repeats too) */
  if (code == 27) return next_tab(m, 1);          /* ] */
  if (code == 26) return next_tab(m, -1);         /* [ */
  if (code==103 && m->pos[m->tab]%GM_PAGE<GM_COLS && m->count) {m->tabs_focus=1;return GM_MOVED;}
  if (code==28 && m->tab==GM_CAT_SEARCH) {m->search_active=1;return GM_MOVED;}
  int r = grid_move(m, code);
  if (r != GM_NONE || value != 1) return r;      /* no repeats past here */
  if (code == 28 && gm_selected(m)) {m->detail_id=(unsigned)(gm_selected(m)-m->games); m->view = GM_DETAILS; m->button = GM_BTN_PLAY; m->message[0] = 0; return GM_MOVED;} /* KEY_ENTER */
  if (code == 35 && gm_selected(m)) return GM_SCORES; /* KEY_H */
  if(code==1) {m->options_active=1;m->option=0;return GM_MOVED;}
  if (code == 16) return GM_QUIT;   /* Q quits; Back/Esc opens Options. */
  return GM_NONE;
}
