/* SPDX-License-Identifier: MIT; host_read (hostio.h) and a session for the
 * ctypes tests: gs_start(s, gmos_host_read, "<game folder>", "<NAME.GAM>", 0). */
#include <stdlib.h>
#include "hostio.h"
#include "session.h"
int32_t gmos_host_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {return host_read(user, name, buf, cap);}
struct gs_session *gmos_session_new(void) {return calloc(1, sizeof(struct gs_session));}
void gmos_session_say(struct gs_session *s, void (*say)(const char *)) {s->say = say;}
int gmos_session_phase(const struct gs_session *s) {return s->phase;}
int gmos_session_card(const struct gs_session *s) {return (int)s->card;}
int gmos_session_ticks(const struct gs_session *s) {return (int)s->total_ticks;}
int gmos_session_showing_card(const struct gs_session *s) {return s->showing_card;}
