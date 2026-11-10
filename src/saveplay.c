/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * saveplay.h. Texts, colors and positions are PLAYGAME.C's. Claude Sonnet 5.5 wrote this file. */
#include "saveplay.h"
#include "game.h"
#include "hiscreen.h"
#include "hisload.h"
#include "text.h"
#define K_ESC 1
#define K_ENTER 28
#define K_KPENTER 96
#define K_BACK 14
#define K_LEFT 105
#define K_UP 103
#define K_DOWN 108
#define K_LSHIFT 42
#define K_RSHIFT 54
#define K_LCTRL 29
#define K_RCTRL 97
#define NAMES_BYTES (SP_SLOTS * 21u)
#define BACKUP_AT 65536u /* the load keeps the play it replaces at this offset of the file buffer */
_Static_assert(sizeof(struct save_game) <= 64 * 1024, "the DOS records must fit the session's text buffer");
static unsigned slen(const char *s) {unsigned n = 0; while (s[n]) n++; return n;}
static void put(uint8_t *px, int x, int y, uint8_t c, const char *s) {gp_write(px, x, y, c, s, slen(s));}
static void fill(uint8_t *px, int x0, int y0, int x1, int y1, uint8_t c) { /* BoxFill */
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (x >= 0 && x < (int)GP_W && y >= 0 && y < (int)GP_H) px[y * GP_W + x] = c;
}
static void slot_file(char out[16], unsigned slot) {const char *p = "GAME"; unsigned n = 0; while (*p) out[n++] = *p++; out[n++] = (char)('0' + slot); out[n++] = '.'; out[n++] = 'S'; out[n++] = 'A'; out[n++] = 'V'; out[n] = 0;}
static int marker_y(int choice) {return 35 - 8 + choice * 14;}
static void marker(struct sp_menu *m, uint8_t c) {int y = marker_y(m->sel); fill(m->s->px, 60, y, 65, y + 4, c);}
static uint8_t pixel(const struct sp_menu *m, int x, int y) {return m->s->px[y * GP_W + x];}
static void message(struct sp_menu *m, int x, const char *text, uint64_t now) {
  put(m->s->px, x, 185, m->msg_col, text); m->stage = SP_MSG; m->t0 = now;
}
static void cursor(struct sp_menu *m, int on) { /* GGet's box (or, with the line full, a bar) at the end of the answer */
  int x = 80 + (int)m->len * 8, y = 40 + m->sel * 14; uint8_t c = on ? m->name_col : m->back;
  if (m->len < SP_NAME_MAX) {
    for (int i = x; i <= x + 6; i++) {m->s->px[y * GP_W + i] = c; m->s->px[(y + 7) * GP_W + i] = c;}
    for (int j = y; j <= y + 7; j++) {m->s->px[j * GP_W + x] = c; m->s->px[j * GP_W + x + 6] = c;}
  } else for (int j = y; j <= y + 7; j++) m->s->px[j * GP_W + x - 1] = c;
  m->cur_drawn = on;
}
void sp_start(struct sp_menu *m, int mode, const struct save_ops *ops, const char *key, struct gs_session *s, uint64_t now) {
  uint8_t *px = s->px, *pal = s->pal; uint8_t raw[NAMES_BYTES]; uint32_t got = 0; int have;
  m->mode = mode; m->stage = SP_PICK; m->sel = 1; m->shift = m->ctrl = 0; m->cur_on = m->cur_drawn = 0; m->len = 0;
  m->ops = ops; m->s = s; m->t0 = m->blink = now;
  unsigned k = 0; for (; key[k] && k < 8; k++) m->key[k] = key[k]; m->key[k] = 0;
  s->showing_card = 1;
  have = !gs_slot_picture(s, mode == SP_SAVE ? GP_SAVEPIC : GP_LOADPIC, px, pal);
  if (!have) { /* the level's own palette on black, with the title */
    for (unsigned i = 0; i < GP_W * GP_H; i++) px[i] = 0;
    const uint8_t *lp = gp_palette(); for (unsigned i = 0; i < 768; i++) pal[i] = lp[i];
  }
  m->msg_col = gp_match(pal, 10, 10, 63); /* "Yellow" in PLAYGAME is RGBBlue.Match */
  m->name_col = gp_match(pal, 63, 63, 0); m->mark_col = gp_match(pal, 63, 10, 10);
  if (!have) {
    put(px, 124, 10, m->msg_col, mode == SP_SAVE ? "Save Game" : "Load Game");
    if (mode == SP_SAVE) for (int i = 0; i < 10; i++) put(px, 73, 40 + i * 14, m->msg_col, ">");
  }
  for (unsigned i = 0; i < NAMES_BYTES; i++) raw[i] = 0;
  int e = save_get(ops, m->key, "SAVNAMES.DAT", raw, sizeof raw, &got);
  if (e == SAVE_EFBIG) e = 0; /* DOS reads the first 210 bytes */
  int drawn = mode == SP_SAVE || !e; /* SelectGameToLoad shows the names only if the file opens */
  for (unsigned i = 0; i < SP_SLOTS; i++) {
    unsigned c = 0; for (; c < SP_NAME_MAX; c++) {char ch = (char)raw[i * 21 + c]; if (!ch) break; m->names[i][c] = ch;}
    m->names[i][c] = 0;
    if (drawn) put(px, 80, 40 + (int)i * 14, m->name_col, m->names[i]);
  }
  m->prev = pixel(m, 60, marker_y(1)); marker(m, m->mark_col);
}
static void write_names(struct sp_menu *m) { /* PLAYGAME writes SavNames.dat before the game; a failure is not reported */
  uint8_t raw[NAMES_BYTES]; for (unsigned i = 0; i < NAMES_BYTES; i++) raw[i] = 0;
  for (unsigned i = 0; i < SP_SLOTS; i++) for (unsigned c = 0; m->names[i][c]; c++) raw[i * 21 + c] = (uint8_t)m->names[i][c];
  save_put(m->ops, m->key, "SAVNAMES.DAT", raw, sizeof raw);
}
static int do_save(struct sp_menu *m, unsigned slot, uint64_t now) {
  uint32_t cap, ncap, n = 0; uint8_t *wire = gs_wire(&cap); struct save_game *g = gs_state_buf(&ncap); char f[16]; slot_file(f, slot);
  int e = gp_state_save(g) ? -1 : save_game_encode(g, wire, cap, &n);
  if (!e) e = save_put(m->ops, m->key, f, wire, n);
  if (e) {message(m, 40, "I cannot save your game!", now); if (m->s->say) m->s->say("GMOS game not saved"); return SP_DRAW;}
  message(m, 112, "Game Saved!", now);
  if (m->s->say) m->s->say("GMOS game saved");
  return SP_DRAW;
}
/* LoadGame: read and check the file first; the play is replaced only once it is whole. The play before is kept (as DOS
   records) so a level whose files cannot be read after the checks puts it back. */
static int do_load(struct sp_menu *m, unsigned slot, uint64_t now) {
  uint32_t cap, ncap, n = 0, bn = 0; uint8_t *wire = gs_wire(&cap); struct save_game *g = gs_state_buf(&ncap); char f[16]; slot_file(f, slot);
  int e = save_get(m->ops, m->key, f, wire, SAVE_GAME_MAX_BYTES, &n);
  if (!e) e = save_game_decode(g, wire, n);
  if (!e && (gp_state_save(g) || save_game_encode(g, wire + BACKUP_AT, cap - BACKUP_AT, &bn))) e = -1;
  if (!e) e = save_game_decode(g, wire, n); /* again: the backup used the buffer */
  int r = e ? -1 : gp_state_load(g);
  if (r == -2) { /* half done: put the play before back */
    if (save_game_decode(g, wire + BACKUP_AT, bn) || gp_state_load(g)) {message(m, 40, "I cannot save your game!", now); return SP_DRAW | SP_FATAL | SP_DONE;}
    r = -1;
  }
  if (r) {
    message(m, 40, "This game has not been saved!", now); /* the nearest DOS text for a missing or damaged file */
    if (m->s->say) {m->s->say(e == SAVE_ENOENT ? "GMOS no saved game in that slot" : "GMOS saved game refused");}
    return SP_DRAW;
  }
  message(m, 88, "Old Game Loaded!", now);
  if (m->s->say) m->s->say("GMOS game loaded");
  return SP_DRAW | SP_LOADED;
}
int sp_event(struct sp_menu *m, uint16_t type, uint16_t code, int32_t value, uint64_t now) {
  if (type != 1) return 0;
  if (code == K_LSHIFT || code == K_RSHIFT) {m->shift = value != 0; return 0;}
  if (code == K_LCTRL || code == K_RCTRL) {m->ctrl = value != 0; return 0;}
  if (value == 0) return 0;
  if (m->stage == SP_MSG) return value == 1 ? SP_DONE : 0; /* PauseTimeKey(2): a key ends it */
  if (m->stage == SP_PICK) {
    if (value != 1) return 0;
    if (code == K_ESC) return SP_DONE; /* choice 0: SaveGame returns, LoadGame goes back */
    if (code == K_UP || code == K_DOWN) {
      marker(m, m->prev);
      m->sel += code == K_UP ? -1 : 1;
      if (m->sel < 1) m->sel = 10;
      if (m->sel > 10) m->sel = 1;
      m->prev = pixel(m, 60, marker_y(m->sel)); marker(m, m->mark_col);
      return SP_DRAW;
    }
    if (code != K_ENTER && code != K_KPENTER) return 0;
    unsigned slot = (unsigned)m->sel - 1;
    if (m->mode == SP_LOAD) return do_load(m, slot, now);
    m->sel = (int)slot; m->stage = SP_NAME; m->back = pixel(m, 79, 40 + (int)slot * 14);
    m->len = slen(m->names[slot]); m->blink = now; cursor(m, 1); m->cur_on = 1;
    return SP_DRAW;
  }
  /* SP_NAME: GGet over the slot's row */
  char *ans = m->names[m->sel]; int y = 40 + m->sel * 14, ch = m->ctrl ? 0 : hs_ascii(code, m->shift);
  if (code == K_ESC) {cursor(m, 0); return value == 1 ? SP_DONE : 0;} /* done = -1: nothing is saved, SavNames stays */
  cursor(m, 0); m->blink = now; m->cur_on = 1;
  if (code == K_ENTER || code == K_KPENTER) {
    if (value != 1) {cursor(m, 1); return SP_DRAW;}
    write_names(m);
    return do_save(m, (unsigned)m->sel, now);
  }
  if (code == K_BACK || code == K_LEFT) {
    if (m->len) {m->len--; fill(m->s->px, 80 + (int)m->len * 8, y, 80 + (int)(m->len + 1) * 8 - 1, y + 7, m->back); ans[m->len] = 0;}
  } else if (ch && m->len < SP_NAME_MAX) {ans[m->len++] = (char)ch; ans[m->len] = 0; put(m->s->px, 80, y, m->name_col, ans);}
  cursor(m, 1);
  return SP_DRAW;
}
static int cursor_on(const struct sp_menu *m, uint64_t now) {return (now - m->blink) / 1000000ull % 900u < 500u;}
int sp_step(struct sp_menu *m, uint64_t now) {
  if (m->stage == SP_MSG) return now - m->t0 >= SP_PAUSE_NS ? SP_DONE : 0;
  if (m->stage != SP_NAME) return 0;
  int on = cursor_on(m, now);
  if (on == m->cur_drawn) return 0;
  cursor(m, on); return SP_DRAW;
}
uint64_t sp_wake(const struct sp_menu *m, uint64_t now) {
  return m->stage == SP_MSG ? m->t0 + SP_PAUSE_NS : m->stage == SP_NAME ? now + 20000000ull : now + 500000000ull;
}
