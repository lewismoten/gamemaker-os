/* SPDX-License-Identifier: MIT; original game session (session.h). The order
 * of cards follows the Pocket Player's play loop and PLAYGAME: the title, the
 * first level's intro, then on a link the level's exit and the next one's
 * intro, after a death the level's intro again, and at the end the won or
 * lost cards. Only GIF cards are shown so far; FLI animations, backdrops and
 * text pages are skipped (and logged). */
#include "session.h"
#include "gif.h"
#define KEY_WAIT 65535u
#define INPUT_POLL_NS 20000000ull /* while a card shows, look at input every 20 ms */
static uint8_t file[160 * 1024];
static char *put(char *p, const char *s) {while (*s) *p++ = *s++; *p = 0; return p;}
static char *num(char *p, int32_t v) {
  char t[12]; int k = 0; uint32_t n = v < 0 ? (uint32_t)-v : (uint32_t)v;
  if (v < 0) *p++ = '-';
  do t[k++] = (char)('0' + n % 10); while (n /= 10);
  while (k) *p++ = t[--k];
  *p = 0; return p;
}
static void say2(const struct gs_session *s, const char *a, const char *b) {
  char line[100], *p = put(line, "GMOS "); p = put(p, a); put(p, b);
  if (s->say) s->say(line);
}
static int lower(int c) {return c >= 'A' && c <= 'Z' ? c + 32 : c;}
static int ext_is(const char *name, const char *ext) {
  const char *dot = 0;
  for (const char *p = name; *p; p++) if (*p == '.') dot = p;
  if (!dot) return 0;
  for (dot++; *dot && *ext; dot++, ext++) if (lower(*dot) != *ext) return 0;
  return !*dot && !*ext;
}
static void add(struct gs_session *s, const char *name, unsigned n, uint32_t pause) {
  if (s->ncards >= GS_MAX_CARDS || !n) return;
  struct gs_card *c = &s->cards[s->ncards++];
  unsigned i = 0;
  for (; i < n && i < GP_NAME_MAX; i++) c->name[i] = name[i];
  c->name[i] = 0;
  c->pause = (uint16_t)(pause > KEY_WAIT ? KEY_WAIT : pause);
}
/* A graphic slot's cards: a .LST names one picture per line ("NAME [pause]", a
   missing pause waits for a key); any other file is one card with ShowGraphic's
   default pause (GIF 15 s, backdrop 20, text 90, animation 0). */
static void queue(struct gs_session *s, const char *slot) {
  if (!slot[0]) return;
  if (!ext_is(slot, "lst")) {
    unsigned n = 0; while (slot[n]) n++;
    add(s, slot, n, ext_is(slot, "bkd") ? 20 : ext_is(slot, "txt") ? 90 : ext_is(slot, "fli") ? 0 : 15);
    return;
  }
  int32_t n = s->read(s->user, slot, file, sizeof file);
  for (int32_t at = 0; at < n;) {
    int32_t end = at;
    while (end < n && file[end] != '\n') end++;
    int32_t a = at, b = at;
    while (b < end && file[b] != ' ' && file[b] != '\r' && file[b] != 0x1a) b++;
    int32_t c = b;
    while (c < end && file[c] == ' ') c++;
    uint32_t pause = KEY_WAIT; int digits = 0, junk = 0; uint32_t v = 0;
    for (int32_t i = c; i < end && file[i] != '\r' && file[i] != 0x1a; i++) {
      if (file[i] >= '0' && file[i] <= '9' && !junk) {v = v * 10 + (uint32_t)(file[i] - '0'); digits = 1; if (v > KEY_WAIT) v = KEY_WAIT;}
      else junk = 1;
    }
    if (c < end && file[c] != '\r' && file[c] != 0x1a) pause = digits ? v : 0; /* parseInt: text that is not a number is 0 */
    add(s, (const char *)file + a, (unsigned)(b - a), pause);
    at = end + 1;
  }
}
static void render_play(struct gs_session *s) {
  gp_render(s->px);
  const uint8_t *p = gp_palette();
  for (unsigned i = 0; i < 768; i++) s->pal[i] = p[i];
  s->showing_card = 0;
}
/* Shows the next card that can be shown; 0 when the queue is done. */
static int next_card(struct gs_session *s, uint64_t now) {
  for (; s->card < s->ncards; s->card++) {
    const struct gs_card *c = &s->cards[s->card];
    int32_t n = ext_is(c->name, "gif") ? s->read(s->user, c->name, file, sizeof file) : -1;
    if (n < 0 || gp_gif(file, (uint32_t)n, s->pal, s->px)) {
      say2(s, "card skipped (only GIF so far): ", c->name);
      continue;
    }
    char line[60], *q = put(line, c->name);
    if (c->pause == KEY_WAIT) put(q, ", until a key"); else {q = put(q, ", "); q = num(q, c->pause); put(q, " s");}
    say2(s, "card ", line);
    s->showing_card = 1; s->key_pressed = 0;
    s->card_until = c->pause == KEY_WAIT ? 0 : now + (uint64_t)c->pause * 1000000000ull;
    s->card++;
    return 1;
  }
  return 0;
}
static int cards_done(struct gs_session *s, uint64_t now) {
  if (s->after == GS_DONE) {s->phase = GS_DONE; return GS_EXIT;}
  int ok = s->begun ? gp_finish() : gp_begin();
  s->begun = 1;
  if (ok) {say2(s, "level files missing", ""); s->phase = GS_DONE; return GS_EXIT;}
  char line[24]; num(line, gp_level() + 1); say2(s, "level ", line);
  s->phase = GS_PLAY; s->t0 = now; s->ticks = 0;
  render_play(s);
  return GS_DRAW;
}
static int run_cards(struct gs_session *s, uint64_t now) {
  s->phase = GS_CARDS;
  if (next_card(s, now)) return GS_DRAW;
  return cards_done(s, now);
}
int gs_start(struct gs_session *s, gp_read_fn read, void *user, const char *gam_name, uint64_t now_ns) {
  s->read = read; s->user = user; s->begun = 0; s->total_ticks = 0; s->ran = 0; s->showing_card = 0;
  s->levels = gp_open(read, user, gam_name);
  if (s->levels < 0) {say2(s, "cannot read ", gam_name); return -1;}
  s->ncards = s->card = 0;
  queue(s, gp_graphic(GP_TITLE, 0));
  queue(s, gp_graphic(GP_INTRO, 0));
  s->after = GS_PLAY;
  for (unsigned i = 0; i < GP_W * GP_H; i++) s->px[i] = 0;
  run_cards(s, now_ns);
  return s->phase == GS_DONE ? -1 : 0;
}
int gs_scan(uint16_t code) {
  switch (code) {
    case 103: return 72;  /* up */
    case 108: return 80;  /* down */
    case 105: return 75;  /* left */
    case 106: return 77;  /* right */
    case 102: return 71;  /* home: up and left */
    case 104: return 73;  /* page up: up and right */
    case 107: return 79;  /* end: down and left */
    case 109: return 81;  /* page down: down and right */
    case 110: return 82;  /* insert */
    case 111: return 83;  /* delete */
    case 96: return 28;   /* keypad enter */
  }
  return code >= 2 && code <= 88 ? code : 0; /* the rest of the PC keyboard: Linux codes are its scan codes */
}
int gs_event(struct gs_session *s, uint16_t type, uint16_t code, int32_t value) {
  if (type != 1 || s->phase == GS_DONE) return 0;
  if (code == 1) {
    if (value == 1) {gp_release_all(); say2(s, "back to the list (Esc)", ""); s->phase = GS_DONE; return GS_EXIT;}
    return 0;
  }
  if (s->phase == GS_CARDS) {if (value == 1) s->key_pressed = 1; return 0;}
  int scan = gs_scan(code);
  if (scan && value != 2) gp_key(scan, value != 0);
  return 0;
}
/* A 'D', 'L' or 'W' from a tick: queue what goes between, as the Pocket Player does. */
static int level_change(struct gs_session *s, char r, uint64_t now) {
  int from = gp_level();
  gp_release_all();
  s->ncards = s->card = 0;
  if (r == 'W') {
    say2(s, "won", "");
    queue(s, gp_graphic(GP_EXIT, from)); queue(s, gp_graphic(GP_WON, 0)); s->after = GS_DONE;
  } else if (r == 'D' && gp_lives() <= 0) {
    say2(s, "game over", "");
    queue(s, gp_graphic(GP_LOST, 0)); s->after = GS_DONE;
  } else if (r == 'L') {
    queue(s, gp_graphic(GP_EXIT, from)); queue(s, gp_graphic(GP_INTRO, gp_pending_level())); s->after = GS_PLAY;
  } else {
    char line[24]; num(line, gp_lives() - 1); say2(s, "lost a life; lives left ", line);
    queue(s, gp_graphic(GP_INTRO, from)); s->after = GS_PLAY;
  }
  return run_cards(s, now);
}
int gs_step(struct gs_session *s, uint64_t now) {
  s->ran = 0;
  if (s->phase == GS_DONE) return GS_EXIT;
  if (s->phase == GS_CARDS) {
    if (!s->key_pressed && (!s->card_until || now < s->card_until)) return 0;
    if (next_card(s, now)) return GS_DRAW;
    return cards_done(s, now);
  }
  uint64_t due = (now - s->t0) * 10u / GS_TICK_NS10;
  while (s->ticks < due && s->ran < 4) {
    char r = gp_tick();
    s->ticks++; s->total_ticks++; s->ran++;
    if (r) return level_change(s, r, now) | GS_DRAW;
  }
  if (s->ticks + 4 < due) {s->t0 = now; s->ticks = 0;} /* far behind: drop the lost time */
  if (!s->ran) return 0;
  render_play(s);
  return GS_DRAW;
}
uint64_t gs_wake(const struct gs_session *s, uint64_t now) {
  if (s->phase == GS_PLAY) return s->t0 + ((uint64_t)(s->ticks + 1) * GS_TICK_NS10 + 9u) / 10u;
  if (s->phase == GS_CARDS) {
    uint64_t poll = now + INPUT_POLL_NS;
    return s->card_until && s->card_until < poll ? s->card_until : poll;
  }
  return now;
}
static uint32_t rgb(const uint8_t *pal, uint8_t i) {
  uint32_t r = pal[i * 3] & 63, g = pal[i * 3 + 1] & 63, b = pal[i * 3 + 2] & 63;
  return ((r << 2 | r >> 4) << 16) | ((g << 2 | g >> 4) << 8) | (b << 2 | b >> 4);
}
void gs_draw(const struct gs_session *s, uint32_t *fb, uint32_t stride) {
  uint32_t lut[256];
  for (unsigned i = 0; i < 256; i++) lut[i] = rgb(s->pal, (uint8_t)i);
  uint32_t x0 = s->showing_card ? 0 : GS_VIEW_X, y0 = s->showing_card ? GS_CARD_Y : GS_VIEW_Y;
  uint32_t w = s->showing_card ? GP_W : GP_VIEW_W, h = s->showing_card ? GP_H : GP_VIEW_H;
  for (uint32_t y = 0; y < 480; y++) {
    uint32_t *row = fb + y * stride;
    if (y < y0 || y >= y0 + 2 * h) {for (uint32_t x = 0; x < 640; x++) row[x] = 0; continue;}
    const uint8_t *src = s->px + ((y - y0) >> 1) * GP_W;
    for (uint32_t x = 0; x < x0; x++) row[x] = 0;
    for (uint32_t x = 0; x < w; x++) {uint32_t c = lut[src[x]]; row[x0 + 2 * x] = c; row[x0 + 2 * x + 1] = c;}
    for (uint32_t x = x0 + 2 * w; x < 640; x++) row[x] = 0;
  }
}
