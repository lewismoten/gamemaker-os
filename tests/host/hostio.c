/* SPDX-License-Identifier: MIT; host_read (hostio.h), a session and an FLI
 * player for the ctypes tests: gs_start(s, gmos_host_read, "<game folder>",
 * "<NAME.GAM>", 0). */
#include <stdlib.h>
#include "hostio.h"
#include "session.h"
#include "menu.h"
/* Exports called by the Python ctypes tests. */
int32_t gmos_host_read(void *user, const char *name, uint8_t *buf, uint32_t cap);
struct gmp *gmos_gmp_open(const char *path);
const char *gmos_gmp_gam(const struct gmp *g);
int gmos_gmp_pocket(const struct gmp *g);
int gmos_gmp_volumes(const struct gmp *g);
void gmos_gmp_close(struct gmp *g);
struct gs_session *gmos_session_new(void);
void gmos_session_say(struct gs_session *s, void (*say)(const char *));
int gmos_session_phase(const struct gs_session *s);
int gmos_session_stage(const struct gs_session *s);
int gmos_session_card(const struct gs_session *s);
int gmos_session_ticks(const struct gs_session *s);
int gmos_session_showing_card(const struct gs_session *s);
int gmos_session_shown(const struct gs_session *s);
int gmos_session_keys(const struct gs_session *s);
const uint8_t *gmos_session_px(const struct gs_session *s);
const uint8_t *gmos_session_pal(const struct gs_session *s);
int gmos_fli_open(const uint8_t *d, uint32_t n);
void gmos_fli_next(uint8_t *px, uint8_t *pal);
int gmos_menu_state(const struct gm_menu *m, int what);
int gmos_menu_music(const struct gm_menu *m);
void gmos_menu_message(struct gm_menu *m, const char *s);
void gmos_menu_pic(struct gm_menu *m, unsigned id, int pic);
void gmos_scan_left(struct gm_menu *m, unsigned n);
int gmos_tab_game(const struct gm_menu *m, unsigned tab, unsigned i);
unsigned gmos_record_bytes(void);
void gmos_session_sound_dir(struct gs_session *s, const char *dir);
int32_t gmos_host_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {return host_read(user, name, buf, cap);}
/* A .GMP for the ctypes tests: gs_start(s, gmp_read, gmos_gmp_open(path), gmos_gmp_gam(g), 0). */
struct gmp *gmos_gmp_open(const char *path) {
  struct host_gmp *h = calloc(1, sizeof *h);
  if (!h || host_gmp_open(h, path)) {free(h); return 0;}
  return &h->g;
}
const char *gmos_gmp_gam(const struct gmp *g) {return g->gam;}
int gmos_gmp_pocket(const struct gmp *g) {return g->pocket;}
int gmos_gmp_volumes(const struct gmp *g) {return (int)g->volumes;}
void gmos_gmp_close(struct gmp *g) {
  if (!g) return;
  struct host_gmp *h = (struct host_gmp *)g->user;
  host_gmp_close(h); free(h);
}
struct gs_session *gmos_session_new(void) {return calloc(1, sizeof(struct gs_session));}
void gmos_session_say(struct gs_session *s, void (*say)(const char *)) {s->say = say;}
int gmos_session_phase(const struct gs_session *s) {return s->phase;}
int gmos_session_stage(const struct gs_session *s) {return s->stage;}
int gmos_session_card(const struct gs_session *s) {return (int)s->card;}
int gmos_session_ticks(const struct gs_session *s) {return (int)s->total_ticks;}
int gmos_session_showing_card(const struct gs_session *s) {return s->showing_card;}
int gmos_session_shown(const struct gs_session *s) {return (int)s->shown;}
int gmos_session_keys(const struct gs_session *s) {return (int)s->keys;}
const uint8_t *gmos_session_px(const struct gs_session *s) {return s->px;}
const uint8_t *gmos_session_pal(const struct gs_session *s) {return s->pal;}
/* An FLI played frame by frame onto a 320 x 200 canvas that starts black. */
static struct gp_fli fli;
static uint8_t fli_px[GP_FLI_PIXELS], fli_pal[768];
int gmos_fli_open(const uint8_t *d, uint32_t n) {
  for (unsigned i = 0; i < sizeof fli_px; i++) fli_px[i] = 0;
  for (unsigned i = 0; i < sizeof fli_pal; i++) fli_pal[i] = 0;
  return gp_fli_open(&fli, d, n) ? -1 : (int)(fli.frames << 16 | fli.speed);
}
void gmos_fli_next(uint8_t *px, uint8_t *pal) {
  gp_fli_frame(&fli, fli_px, fli_pal);
  for (unsigned i = 0; i < fli.w * fli.h; i++) px[i] = fli_px[i];
  for (unsigned i = 0; i < 768; i++) pal[i] = fli_pal[i];
}
/* The games list's state for the tests: 0 tab, 1 position in the tab, 2 view, 3 button, 4 the selected game's index (-1: none). */
#include "menu.h"
int gmos_menu_music(const struct gm_menu *m) {return m->background_music;}
int gmos_menu_state(const struct gm_menu *m, int what) {
  const struct gm_game *g = gm_selected(m);
  return what == 0 ? (int)m->tab : what == 1 ? (int)m->pos[m->tab] : what == 2 ? (int)m->view : what == 3 ? (int)m->button : g ? (int)(g - m->games) : -1;
}
void gmos_scan_left(struct gm_menu *m, unsigned n) {m->scan_left = n;}
void gmos_menu_message(struct gm_menu *m, const char *s) {unsigned k = 0; while (s[k] && k < GM_MESSAGE_MAX) {m->message[k] = s[k]; k++;} m->message[k] = 0;}
void gmos_menu_pic(struct gm_menu *m, unsigned id, int pic) {m->games[id].pic = (int8_t)pic;}
int gmos_tab_game(const struct gm_menu *m, unsigned tab, unsigned i) {const struct gm_game *g = gm_tab_game(m, tab, i); return g ? (int)(g - m->games) : -1;}
/* sizeof(struct gm_record), for the ctypes mirror's size check. */
#include "library.h"
unsigned gmos_record_bytes(void) {return sizeof(struct gm_record);}
void gmos_session_sound_dir(struct gs_session *s, const char *dir) {snd_dir(&s->sound, dir);}
