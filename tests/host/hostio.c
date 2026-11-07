/* SPDX-License-Identifier: MIT; host_read (hostio.h), a session and an FLI
 * player for the ctypes tests: gs_start(s, gmos_host_read, "<game folder>",
 * "<NAME.GAM>", 0). */
#include <stdlib.h>
#include "hostio.h"
#include "session.h"
int32_t gmos_host_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {return host_read(user, name, buf, cap);}
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
