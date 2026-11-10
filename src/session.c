/* SPDX-License-Identifier: MIT; original game session (session.h). The order
 * of pages follows the Pocket Player's play loop and PLAYGAME: the title, the
 * first level's intro, then on a link the level's exit and the next one's
 * intro, after a death the level's intro again, and at the end the won or
 * lost pages. Each page is shown as the Sandbox Edition's showGraphic shows it
 * (a port of PLAYGAME's ShowGraphic): a GIF or BKD on a black palette fading
 * in over 65 retraces, then its pause; an FLI frame by frame over the screen,
 * then its pause; a text page written over the screen, a pause a page. A level
 * fades in from the palette showing and out to black. Key presses wait in a
 * buffer while pages show, as the BIOS kept them for PLAYGAME's GetKey. */
#include "session.h"
#include "gif.h"
#include "text.h"
#define DEFAULT_PAUSE 0x10000u /* the file type's default pause */
#define INPUT_POLL_NS 20000000ull /* while a page shows, look at input every 20 ms */
#define KEY_BUFFER 15u
/* Pictures and animations are read whole (the largest FLI in the Sandbox games is 658 KB). */
uint8_t gp_file_buf[1024 * 1024]; /* also thumb.c's read buffer: thumbs load in the menu, never while a session plays */
#define file gp_file_buf
static uint8_t text[64 * 1024], saved[GP_W * GP_H], canvas[GP_FLI_PIXELS], canvas_pal[768];
static uint8_t listnext;
static const uint8_t black[768];
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
static void copy(uint8_t *d, const uint8_t *s, uint32_t n) {for (uint32_t i = 0; i < n; i++) d[i] = s[i];}
static void zero(uint8_t *d, uint32_t n) {for (uint32_t i = 0; i < n; i++) d[i] = 0;}
static uint64_t retrace(uint64_t k) {return k * 1000000000ull / 70u;} /* k vertical retraces in ns */
static void add(struct gs_session *s, const char *name, unsigned n, uint32_t pause, uint8_t list, int lowercase) {
  if (s->ncards >= GS_MAX_CARDS || !n) return;
  struct gs_card *c = &s->cards[s->ncards++];
  unsigned i = 0;
  for (; i < n && i < GP_NAME_MAX; i++) c->name[i] = lowercase ? (char)lower(name[i]) : name[i];
  c->name[i] = 0;
  c->pause = pause;
  c->list = list; c->clear = 0;
}
/* A graphic slot's pages: a .LST names one per line ("NAME [pause]", parseSlideList:
   the name is up to 15 characters, a missing pause waits for a key); any other file
   is one page with ShowGraphic's default pause for its kind. */
static void queue(struct gs_session *s, const char *slot) {
  if (!slot[0]) return;
  if (!ext_is(slot, "lst")) {unsigned n = 0; while (slot[n]) n++; add(s, slot, n, DEFAULT_PAUSE, 0, 0); return;}
  int32_t n = s->read(s->user, slot, file, sizeof file);
  uint8_t id = ++listnext ? listnext : ++listnext;
  for (int32_t at = 0; at < n;) {
    int32_t end = at;
    while (end < n && file[end] != '\n') end++;
    char line[256]; int32_t k = 0;
    for (int32_t i = at; i < end && k < (int32_t)sizeof line; i++) if (file[i] != '\r' && file[i] != 0x1a) line[k++] = (char)file[i];
    int32_t b = 0;
    while (b < k && line[b] != ' ') b++;
    int32_t c = b;
    while (c < k && line[c] == ' ') c++;
    int32_t e = k;
    while (e > c && (line[e - 1] == ' ' || line[e - 1] == '\t')) e--;
    uint32_t pause = GS_KEY_WAIT, v = 0; int digits = 0;
    for (int32_t i = c; i < e && line[i] >= '0' && line[i] <= '9'; i++) {v = v * 10 + (uint32_t)(line[i] - '0'); digits = 1; if (v > GS_KEY_WAIT) v = GS_KEY_WAIT;}
    if (c < e) pause = digits ? v : 0; /* parseInt: text that is not a number is 0 */
    add(s, line, (unsigned)b, pause, id, 1);
    at = end + 1;
  }
}
static void render_play(struct gs_session *s) {gp_render(s->px); s->showing_card = 0;}
void gs_fade_palette(const uint8_t *from, const uint8_t *to, uint32_t step, uint32_t steps, uint8_t *out) {
  for (unsigned i = 0; i < 768; i++) out[i] = (uint8_t)(from[i] + ((int)to[i] - (int)from[i]) * (int)step / (int)steps);
}
static void fade(struct gs_session *s, int stage, const uint8_t *to, uint64_t now) {
  copy(s->from, s->pal, 768); copy(s->to, to, 768);
  s->stage = stage; s->t = now; s->fade_step = 0;
}
static void pause_from(struct gs_session *s, uint32_t seconds, uint64_t t) {s->stage = GS_PAUSE; s->pause = seconds; s->t = t;}
/* The next page begins when the last one ended: on time (a pause that ran out,
   an animation's or fade's last retrace), or now (a key). */
static void next_from(struct gs_session *s, uint64_t t) {s->stage = GS_NEXT; s->t = t;}
static void say_card(struct gs_session *s, const char *name, uint32_t pause) {
  char line[60], *q = put(line, name);
  if (pause == GS_KEY_WAIT) put(q, ", until a key"); else {q = put(q, ", "); q = num(q, (int32_t)pause); put(q, " s");}
  say2(s, "page ", line);
}
/* drawbkd: { i16 x, i16 y } and a GIF name (fgets of 40, cut at the first newline). */
static int32_t read_picture(struct gs_session *s, const char *name, int bkd, int32_t *x, int32_t *y) {
  *x = *y = 0;
  if (!bkd) return s->read(s->user, name, file, sizeof file);
  int32_t n = s->read(s->user, name, file, 64);
  if (n < 4) return -1;
  *x = (int16_t)(file[0] | file[1] << 8); *y = (int16_t)(file[2] | file[3] << 8);
  char gif[GP_NAME_MAX + 1]; int32_t a = 4, e = 4, k = 0;
  while (e < n && e < 4 + 39 && file[e] != '\r' && file[e] != '\n' && file[e]) e++;
  while (a < e && file[a] == ' ') a++;
  while (e > a && file[e - 1] == ' ') e--;
  for (; a < e && k < (int32_t)GP_NAME_MAX; a++) gif[k++] = (char)file[a];
  gif[k] = 0;
  return k ? s->read(s->user, gif, file, sizeof file) : -1;
}
/* Display_File: lines from row vertpos until the page is full ("MORE..." when
   another line follows) or the text ends. */
static void write_page(struct gs_session *s) {
  for (;;) {
    uint32_t at = s->text_at, start;
    int32_t len = gp_text_line(text, s->text_n, &at, &start);
    if (len < 0) {s->more = 0; return;}
    if (s->vertpos > 20) {gp_write(s->px, 224, 168, s->text_red, "MORE...", 7); s->more = 1; return;}
    s->text_at = at;
    gp_write(s->px, 5 * 8, s->vertpos * 8, s->text_col, (const char *)text + start, (uint32_t)len);
    s->vertpos++;
  }
}
/* Starts the page; GS_DRAW when the screen changed. */
static int begin_card(struct gs_session *s, const struct gs_card *c, uint64_t now) {
  int r = 0;
  s->now_card = c; s->text = 0;
  if (c->clear) {zero(s->px, sizeof s->px); copy(s->pal, s->colors, 768); s->showing_card = 1; r = GS_DRAW;}
  if (c->list != s->listid) {s->listid = c->list; zero(s->listpal, 768);}
  int gif = ext_is(c->name, "gif"), bkd = ext_is(c->name, "bkd"), fli = ext_is(c->name, "fli") || ext_is(c->name, "flc"), txt = ext_is(c->name, "txt");
  uint32_t pause = c->pause != DEFAULT_PAUSE ? c->pause : gif ? 15 : bkd ? 20 : txt ? 90 : 0;
  if (gif || bkd) {
    uint8_t pal[768];
    zero(s->pal, 768); /* SetAllPalTo(&Black) */
    s->showing_card = 1;
    int32_t x, y, n = read_picture(s, c->name, bkd, &x, &y);
    if (n < 0 || gp_gif_draw(file, (uint32_t)n, x, y, pal, s->px)) {
      zero(s->px, sizeof s->px); /* GraphMode: a blank screen, no pause */
      say2(s, "page not a picture: ", c->name);
      next_from(s, now); return GS_DRAW;
    }
    if (c->list) copy(s->listpal, pal, 768);
    say_card(s, c->name, pause);
    fade(s, GS_FADE, pal, now); s->pause = pause;
    return GS_DRAW;
  }
  if (fli) {
    int32_t n = s->read(s->user, c->name, file, sizeof file);
    if (n < 0 || gp_fli_open(&s->fli, file, (uint32_t)n)) {say2(s, "page not an animation: ", c->name); next_from(s, now); return r;}
    copy(canvas_pal, s->pal, 768);
    uint32_t k = s->fli.w * s->fli.h; copy(canvas, s->px, k < GP_W * GP_H ? k : GP_W * GP_H);
    char line[64], *q = put(line, c->name); q = put(q, ", "); q = num(q, (int32_t)s->fli.frames); q = put(q, " frames "); q = num(q, (int32_t)s->fli.speed); q = put(q, "/70 s apart, then ");
    if (pause == GS_KEY_WAIT) put(q, "a key"); else {q = num(q, (int32_t)pause); put(q, " s");}
    say2(s, "page ", line);
    s->stage = GS_FLI; s->t = now; s->fli_frame = 0; s->pause = pause; s->showing_card = 1;
    return r;
  }
  if (txt) {
    const uint8_t *cols = c->list ? s->listpal : s->colors;
    s->text_col = gp_match(cols, 50, 10, 50);      /* RGBPurple */
    s->text_red = gp_match(s->colors, 63, 10, 10); /* RGBRed */
    copy(saved, s->px, sizeof saved);
    s->keys = 0; /* while (GetKey(1)) GetKey(0) */
    s->showing_card = 1; s->text = 1; s->vertpos = 5; s->text_at = 0;
    int32_t n = s->read(s->user, c->name, text, sizeof text);
    if (n < 0) {
      char line[GP_NAME_MAX + 16], *q = put(line, "The file "); q = put(q, c->name); q = put(q, " is");
      gp_write(s->px, 40, 80, s->text_red, "Missing File:", 13);
      gp_write(s->px, 40, 96, s->text_red, line, (uint32_t)(q - line));
      gp_write(s->px, 40, 104, s->text_red, "missing or cannot be opened.", 28);
      s->text_n = 0; s->more = 0;
    } else {
      s->text_n = gp_text_mode(text, (uint32_t)n);
      write_page(s);
    }
    say_card(s, c->name, pause);
    s->shown++;
    pause_from(s, pause, now);
    return GS_DRAW;
  }
  say2(s, "page skipped: ", c->name);
  next_from(s, now);
  return r;
}
static int cards_done(struct gs_session *s, uint64_t now) {
  s->now_card = 0;
  if (s->after == GS_DONE) {snd_end(&s->sound); s->phase = GS_DONE; return GS_EXIT;}
  int ok = s->begun ? gp_finish() : gp_begin();
  s->begun = 1;
  if (ok) {say2(s, "level files missing", ""); snd_end(&s->sound); s->phase = GS_DONE; return GS_EXIT;}
  snd_music(&s->sound, gp_song(GP_SONG_LEVEL, gp_level())); /* loadscene plays the level's song */
  char line[24]; num(line, gp_level() + 1); say2(s, "level ", line);
  render_play(s);
  copy(s->colors, gp_palette(), 768); /* s.colors = w.palette */
  fade(s, GS_LEVEL_IN, s->colors, now);
  return GS_DRAW;
}
/* A 'D', 'L' or 'W' from a tick, once the level has faded out: queue what goes
   between, as the Pocket Player does. */
static void level_change(struct gs_session *s, char r) {
  int from = gp_level();
  gp_release_all();
  s->ncards = s->card = 0;
  if (r == 'W') {
    say2(s, "won", "");
    snd_music(&s->sound, gp_song(GP_SONG_WON, 0));
    queue(s, gp_graphic(GP_EXIT, from)); queue(s, gp_graphic(GP_WON, 0)); s->after = GS_DONE;
  } else if (r == 'D' && gp_lives() <= 0) {
    say2(s, "game over", "");
    snd_music(&s->sound, gp_song(GP_SONG_LOST, 0));
    queue(s, gp_graphic(GP_LOST, 0)); s->after = GS_DONE;
  } else if (r == 'L') {
    queue(s, gp_graphic(GP_EXIT, from)); queue(s, gp_graphic(GP_INTRO, gp_pending_level())); s->after = GS_PLAY;
  } else {
    char line[24]; num(line, gp_lives() - 1); say2(s, "lost a life; lives left ", line);
    queue(s, gp_graphic(GP_INTRO, from)); s->after = GS_PLAY;
  }
  s->keys = 0; /* m.flushKeys() after the scene */
}
/* FLI::View: frame k shows k * speed retraces after the start; all at once at speed 0. */
static int fli_step(struct gs_session *s, uint64_t now) {
  int r = 0;
  while (s->fli_frame < s->fli.frames && now >= s->t + retrace((uint64_t)s->fli_frame * s->fli.speed)) {
    if (gp_fli_frame(&s->fli, canvas, canvas_pal)) copy(s->pal, canvas_pal, 768);
    uint32_t w = s->fli.w < GP_W ? s->fli.w : GP_W, h = s->fli.h < GP_H ? s->fli.h : GP_H;
    for (uint32_t y = 0; y < h; y++) copy(s->px + y * GP_W, canvas + y * s->fli.w, w);
    s->fli_frame++; r = GS_DRAW;
  }
  uint64_t end = s->t + retrace((uint64_t)s->fli.frames * s->fli.speed);
  if (s->fli_frame >= s->fli.frames && now >= end) {
    if (s->now_card->list) copy(s->listpal, s->pal, 768); /* GetAllPal(NewCols) */
    s->shown++;
    pause_from(s, s->pause, end);
  }
  return r;
}
/* Runs the pages phase until it has to wait. */
static int run_cards(struct gs_session *s, uint64_t now) {
  int r = 0;
  for (unsigned guard = 0; guard < 4 * GS_MAX_CARDS + 8; guard++) {
    switch (s->stage) {
      case GS_NEXT: {
        uint64_t at = s->t < now ? s->t : now;
        if (s->card < s->ncards) {r |= begin_card(s, &s->cards[s->card++], at); continue;}
        r |= cards_done(s, at);
        if (r & GS_EXIT) return r;
        continue;
      }
      case GS_FADE: case GS_LEVEL_IN: case GS_LEVEL_OUT: {
        uint64_t step = (now - s->t) * 70u / 1000000000ull;
        if (now >= s->t + retrace(step + 1)) step++; /* step k begins at retrace(k), rounded down to the ns */
        if (step < GS_FADE_STEPS) {
          if (step != s->fade_step) {s->fade_step = (uint32_t)step; gs_fade_palette(s->from, s->to, s->fade_step, GS_FADE_STEPS, s->pal); r |= GS_DRAW;}
          return r;
        }
        copy(s->pal, s->to, 768); r |= GS_DRAW;
        uint64_t end = s->t + retrace(GS_FADE_STEPS);
        if (s->stage == GS_FADE) {s->shown++; pause_from(s, s->pause, end); return r;} /* the whole picture is drawn before a waiting key ends it */
        if (s->stage == GS_LEVEL_IN) {s->phase = GS_PLAY; s->t0 = now; s->ticks = 0; s->keys = 0; return r;}
        level_change(s, s->change);
        next_from(s, end);
        continue;
      }
      case GS_FLI:
        r |= fli_step(s, now);
        if (s->stage == GS_FLI || r) return r; /* the last frame is drawn before its pause */
        continue;
      case GS_PAUSE: {
        uint64_t end = s->t + (uint64_t)s->pause * 1000000000ull;
        if (s->keys) {s->keys--; end = now;}
        else if (s->pause == GS_KEY_WAIT || now < end) return r;
        if (s->text && s->more) {
          copy(s->px, saved, sizeof saved); s->vertpos = 5; write_page(s); s->shown++;
          pause_from(s, s->pause, end); r |= GS_DRAW; continue;
        }
        if (s->text) {copy(s->px, saved, sizeof saved); s->text = 0; r |= GS_DRAW;}
        next_from(s, end);
        continue;
      }
    }
  }
  return r;
}
int gs_start(struct gs_session *s, gp_read_fn read, void *user, const char *gam_name, uint64_t now_ns) {
  s->read = read; s->user = user; s->begun = 0; s->total_ticks = 0; s->ran = 0; s->showing_card = 0;
  s->keys = 0; s->shown = 0; s->listid = 0; s->text = 0; s->now_card = 0;
  zero(s->px, sizeof s->px); zero(s->pal, 768); zero(s->colors, 768); zero(s->listpal, 768);
  s->levels = gp_open(read, user, gam_name);
  if (s->levels < 0) {say2(s, "cannot read ", gam_name); return -1;}
  s->sound.say = s->say;
  snd_game(&s->sound, gam_name);
  snd_music(&s->sound, gp_song(GP_SONG_TITLE, 0));
  /* PLAYGAME's menu sets `colors` to its picture's palette before play */
  const char *menu = gp_graphic(GP_MENU, 0);
  int32_t x, y, n = ext_is(menu, "gif") || ext_is(menu, "bkd") ? read_picture(s, menu, ext_is(menu, "bkd"), &x, &y) : -1;
  if (n < 0 || gp_gif_palette(file, (uint32_t)n, s->colors)) zero(s->colors, 768);
  s->ncards = s->card = 0;
  queue(s, gp_graphic(GP_TITLE, 0));
  unsigned intro = s->ncards;
  queue(s, gp_graphic(GP_INTRO, 0));
  if (intro < s->ncards) s->cards[intro].clear = 1; /* m.screen.clear() before the first scene */
  s->after = GS_PLAY; s->phase = GS_CARDS; next_from(s, now_ns);
  run_cards(s, now_ns);
  if (s->phase == GS_DONE) {snd_end(&s->sound); return -1;}
  return 0;
}
/* PLAYGAME's Recording::DoPlayBack, once a game tick before the tick: the timer counts up and each
   recorded key whose time has come is pressed (a scan code, bit 7 set: released). The web engine's
   recorder does the same per tick. A gap over 4096 ticks is a DOS demo's wait at a page (the preview
   plays no pages), so it counts as one tick. Returns 1 when the demo's Esc comes. */
static int demo_feed(struct gs_session *s) {
  s->demo_timer++;
  while (s->demo_i < s->demo_n) {
    const uint8_t *r = text + 3 * s->demo_i;
    uint32_t delta = (uint32_t)(r[0] | r[1] << 8);
    if (delta > 4096) delta = 1;
    if (s->demo_last + delta > s->demo_timer) break;
    s->demo_last += delta; s->demo_i++;
    if ((r[2] & 0x7f) == 1) return 1;
    gp_key(r[2] & 0x7f, !(r[2] & 0x80));
  }
  return 0;
}
void gs_demo_stop(struct gs_session *s) {
  if (s->demo) {gp_release_all(); s->demo = 0; s->phase = GS_DONE;}
}
int gs_demo_start(struct gs_session *s, gp_read_fn read, void *user, const char *gam_name, uint64_t now_ns) {
  s->read = read; s->user = user; s->begun = 0; s->total_ticks = 0; s->ran = 0; s->showing_card = 0; s->demo = 0;
  s->keys = 0; s->text = 0; s->now_card = 0;
  zero(s->px, sizeof s->px); zero(s->pal, 768);
  if (gp_open(read, user, gam_name) < 0) return -1;
  const char *name = gp_demo();
  int32_t n = name[0] ? read(user, name, text, 1000u * 3u) : -1;
  if (n < 3 || gp_begin()) return -1;
  s->demo = 1; s->demo_n = (uint32_t)n / 3; s->demo_i = s->demo_timer = s->demo_last = 0;
  s->begun = 1; s->phase = GS_PLAY; s->t0 = now_ns; s->ticks = 0;
  render_play(s); copy(s->pal, gp_palette(), 768);
  return 0;
}
static int demo_step(struct gs_session *s, uint64_t now) {
  uint64_t due = (now - s->t0) * 10u / GS_TICK_NS10;
  while (s->ticks < due && s->ran < 4) {
    if (demo_feed(s)) {gs_demo_stop(s); return GS_EXIT;}
    char r = gp_tick();
    s->ticks++; s->total_ticks++; s->ran++;
    {int n = gp_take_sound(); /* the preview plays the game's effects, as PLAYGAME's demo does (its play loop is the real one) */
     if (n >= 0 && s->sound.event) {
       const char *voc = gp_voc_name(n);
       if (!voc[0]) s->sound.event(s->sound.event_user, SND_EV_BEEP, gp_sound_file(), n);
       else {char nm[SND_VOC_NAME_MAX]; snd_voc_file(nm, voc); s->sound.event(s->sound.event_user, SND_EV_VOC, nm, 0);}
     }}
    if (r == 'W' || (r == 'D' && gp_lives() <= 0) || ((r == 'L' || r == 'D') && gp_finish())) {gs_demo_stop(s); return GS_EXIT;}
    if (r) gp_release_all(); /* a link or a lost life: the next level or try starts at once, keys let go */
  }
  if (s->ticks + 4 < due) {s->t0 = now; s->ticks = 0;}
  if (!s->ran) return 0;
  render_play(s); copy(s->pal, gp_palette(), 768);
  return GS_DRAW;
}
int gs_demo_idle_due(uint64_t last, uint64_t now) {return now >= last && now - last >= GS_DEMO_IDLE_NS;}
int gs_demo_stops(uint16_t type, int32_t value) {return type == 1 && value == 1;}
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
    if (value == 1) {gp_release_all(); say2(s, "back to the list (Esc)", ""); snd_end(&s->sound); s->phase = GS_DONE; return GS_EXIT;}
    return 0;
  }
  if (s->phase == GS_CARDS) {if (value == 1 && s->stage != GS_LEVEL_IN && s->keys < KEY_BUFFER) s->keys++; return 0;}
  int scan = gs_scan(code);
  if (scan && value != 2) gp_key(scan, value != 0);
  return 0;
}
int gs_step(struct gs_session *s, uint64_t now) {
  s->ran = 0;
  if (s->phase == GS_DONE) return GS_EXIT;
  if (s->phase == GS_CARDS) return run_cards(s, now);
  if (s->demo) return demo_step(s, now);
  uint64_t due = (now - s->t0) * 10u / GS_TICK_NS10;
  while (s->ticks < due && s->ran < 4) {
    char r = gp_tick();
    s->ticks++; s->total_ticks++; s->ran++;
    snd_effect(&s->sound, s->total_ticks);
    if (r) { /* the level fades out from the frame of this tick */
      render_play(s);
      s->change = r; s->phase = GS_CARDS;
      fade(s, GS_LEVEL_OUT, black, now);
      return GS_DRAW;
    }
  }
  if (s->ticks + 4 < due) {s->t0 = now; s->ticks = 0;} /* far behind: drop the lost time */
  if (!s->ran) return 0;
  render_play(s);
  return GS_DRAW;
}
uint64_t gs_wake(const struct gs_session *s, uint64_t now) {
  if (s->phase == GS_PLAY) return s->t0 + ((uint64_t)(s->ticks + 1) * GS_TICK_NS10 + 9u) / 10u;
  if (s->phase != GS_CARDS) return now;
  uint64_t poll = now + INPUT_POLL_NS, at = poll;
  switch (s->stage) {
    case GS_NEXT: return now;
    case GS_FADE: case GS_LEVEL_IN: case GS_LEVEL_OUT: at = s->t + retrace(s->fade_step + 1u); break;
    case GS_FLI: at = s->t + retrace((uint64_t)(s->fli_frame < s->fli.frames ? s->fli_frame : s->fli.frames) * s->fli.speed); break;
    case GS_PAUSE: if (s->pause != GS_KEY_WAIT) at = s->t + (uint64_t)s->pause * 1000000000ull; break;
  }
  return at < now ? now : at < poll ? at : poll;
}
int gs_scores_picture(struct gs_session *s, int open, gp_read_fn read, void *user, const char *gam, uint8_t *px, uint8_t *pal) {
  if (open) {s->read = read; s->user = user; if (gp_open(read, user, gam) < 0) return -1;}
  const char *name = gp_graphic(GP_SCORES, 0);
  int bkd = ext_is(name, "bkd");
  if (!(ext_is(name, "gif") || bkd)) return -1;
  int32_t x, y, n = read_picture(s, name, bkd, &x, &y);
  zero(px, GP_W * GP_H); zero(pal, 768);
  return n < 0 || gp_gif_draw(file, (uint32_t)n, x, y, pal, px) ? -1 : 0;
}
static uint32_t rgb(const uint8_t *pal, uint8_t i) {
  uint32_t r = pal[i * 3] & 63, g = pal[i * 3 + 1] & 63, b = pal[i * 3 + 2] & 63;
  return ((r << 2 | r >> 4) << 16) | ((g << 2 | g >> 4) << 8) | (b << 2 | b >> 4);
}
void gs_show(const uint8_t *px, const uint8_t *pal, uint32_t *fb, uint32_t stride) {
  uint32_t lut[256];
  for (unsigned i = 0; i < 256; i++) lut[i] = rgb(pal, (uint8_t)i);
  for (uint32_t y = 0; y < 480; y++) {
    uint32_t *row = fb + y * stride;
    if (y < GS_CARD_Y || y >= GS_CARD_Y + 2 * GP_H) {for (uint32_t x = 0; x < 640; x++) row[x] = 0; continue;}
    const uint8_t *src = px + ((y - GS_CARD_Y) >> 1) * GP_W;
    for (uint32_t x = 0; x < GP_W; x++) {uint32_t c = lut[src[x]]; row[2 * x] = c; row[2 * x + 1] = c;}
  }
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
