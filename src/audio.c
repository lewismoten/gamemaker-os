/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten; see audio.h. */
#include "audio.h"
static void copy(char *to, const char *from) {unsigned i = 0; while (from && from[i] && i < AU_NAME_MAX) {to[i] = from[i]; i++;} to[i] = 0;}
static int same(const char *a, const char *b) {unsigned i = 0; while (a[i] && a[i] == b[i]) i++; return a[i] == b[i];}
void au_init(struct au *a) {
  uint8_t *b = (uint8_t *)a;
  for (unsigned i = 0; i < sizeof *a; i++) b[i] = 0;
  cmf_init(&a->cmf, &a->opl); pc_init(&a->pc);
}
void au_music(struct au *a, const char *cmf) {copy(a->want_music, cmf); a->music_pending = 1;}
void au_beep(struct au *a, const char *snd, int tune) {copy(a->want_snd, snd); a->want_tune = tune; a->beep_pending = 1;}
void au_stop(struct au *a) {a->want_music[0] = 0; a->music_pending = 1; a->beep_pending = 0; pc_stop(&a->pc);}
int au_service(struct au *a, int32_t (*read)(void *, const char *, uint8_t *, uint32_t), void *user) {
  int started = 0;
  if (a->music_pending) {
    a->music_pending = 0;
    if (!a->want_music[0]) cmf_stop(&a->cmf);
    else {
      int32_t n = read ? read(user, a->want_music, a->music, AU_CMF_MAX) : -1;
      if (n > 0 && !cmf_load(&a->cmf, a->music, (uint32_t)n)) {a->until_q16 = 0; a->music_loads++; started = 1;}
      else cmf_stop(&a->cmf);
    }
  }
  if (a->beep_pending) {
    a->beep_pending = 0;
    if (!same(a->snd_name, a->want_snd)) {
      int32_t n = read && a->want_snd[0] ? read(user, a->want_snd, a->snd, sizeof a->snd) : -1;
      a->snd_len = n > 0 ? (uint32_t)n : 0; copy(a->snd_name, n > 0 ? a->want_snd : "");
    }
    if (a->snd_len && a->want_tune >= 0 && !pc_start(&a->pc, a->snd, a->snd_len, (unsigned)a->want_tune)) {a->beeps++; started = 1;}
  }
  return started;
}
void au_render(struct au *a, int16_t *out, unsigned n) {
  int32_t acc[AU_CHUNK];
  int16_t sp[AU_CHUNK * 2];
  unsigned i = 0;
  for (unsigned k = 0; k < n; k++) acc[k] = 0;
  if (a->cmf.playing) while (i < n) { /* the worklet's loop: tick when due, then render up to the next one */
    if ((int32_t)a->until_q16 <= 0) {cmf_tick(&a->cmf); a->until_q16 += CMF_TICK_Q16;}
    unsigned run = (a->until_q16 + 65535u) >> 16;
    if (run > n - i) run = n - i;
    opl_render(&a->opl, acc + i, run);
    a->until_q16 -= run << 16; i += run;
  } else opl_render(&a->opl, acc, n); /* a stopped song's last notes ring out their release */
  if (a->pc.active) {
    pc_render(&a->pc, sp, n);
    for (unsigned k = 0; k < n; k++) acc[k] += sp[k * 2];
  }
  for (unsigned k = 0; k < n; k++) {
    int32_t v = acc[k]; if (v > 32767) v = 32767; if (v < -32768) v = -32768;
    out[k * 2] = out[k * 2 + 1] = (int16_t)v;
  }
  a->frames += n;
}
int au_pump(struct au *a, const struct au_sink *s) {
  uint32_t queued = 0, cap = 0, sent = 0;
  if (s->query(s->user, &queued, &cap)) return -1;
  if (queued >= AU_LOW_FRAMES && !a->hold_len) return 0;
  uint32_t want = AU_FILL_FRAMES - (queued < AU_FILL_FRAMES ? queued : AU_FILL_FRAMES);
  if (cap && cap > queued && cap - queued < want) want = cap - queued;
  while (a->hold_len || want >= AU_CHUNK) {
    if (!a->hold_len) {
      uint64_t t0 = s->now(s->user);
      au_render(a, a->hold, AU_CHUNK);
      a->total_ns += s->now(s->user) - t0;
      a->hold_len = AU_CHUNK * 4u; a->hold_off = 0;
      want -= AU_CHUNK;
    }
    int32_t w = s->write(s->user, (const uint8_t *)a->hold + a->hold_off, a->hold_len);
    if (w <= 0) {a->eagain++; break;} /* none fit: the rendered chunk is kept and offered first next time */
    a->writes++; a->hold_off += (uint32_t)w; a->hold_len -= (uint32_t)w; sent += (uint32_t)w / 4u;
  }
  return (int)sent;
}
