/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten; the UI sounds (uisound.h): two-operator FM notes in the OPL2 manner,
 * with pitch sweeps, short arpeggios and a noise click. All integer, deterministic. */
#include "uisound.h"
#define F(ms) ((uint16_t)((ms) * 48u)) /* ms to frames at 48 kHz */
/* One note: carrier sweeps hz0 to hz1; the modulator runs at ratio/4 of it and its depth (idx, 1/16 radian) fades with the note; click: noise burst. */
struct note {uint16_t hz0, hz1, len; uint8_t ratio4, idx, amp, click;};
static const struct note cues[GM_CUES][UI_SEGS] = {
  {{0}},
  /* move: a dry, glassy tick: noise tap, then a quick bell-ish blip sliding up */
  {{1500, 2000, F(52), 8, 40, 200, 90}},
  /* tab: two-step skip, sliding up into each note, mellow */
  {{660, 740, F(44), 4, 22, 220, 50}, {990, 1110, F(62), 4, 26, 220, 0}},
  /* select: bright confirm chime, a rising triad then a bell on top (C, E, G, high C) */
  {{1047, 1047, F(40), 4, 30, 200, 120}, {1319, 1319, F(40), 4, 30, 210, 0}, {1568, 1568, F(44), 4, 30, 220, 0}, {2093, 2093, F(116), 14, 36, 240, 0}},
  /* back: soft, two notes falling with a downward slide, nearly a sine */
  {{880, 700, F(70), 4, 12, 200, 30}, {622, 440, F(80), 4, 10, 190, 0}},
  /* can't: a muffled low bonk, then a lower one */
  {{190, 150, F(52), 4, 34, 230, 60}, {140, 105, F(78), 4, 28, 220, 0}},
};
static uint32_t total(const struct ui_voice *v) {uint32_t t = 0; for (unsigned i = 0; i < UI_SEGS; i++) t += cues[v->cue][i].len; return t;}
/* sin of a 32-bit phase, Q15 result: a parabola refined once (about 0.1% off), good for a blip. */
static int32_t sn(uint32_t ph) {
  int32_t u = (int16_t)(ph >> 16), a = u < 0 ? -u : u;
  int32_t y = (int32_t)(((int64_t)4 * u * (32768 - a)) >> 15);
  int32_t ya = y < 0 ? -y : y;
  return y + (int32_t)(((int64_t)225 * (((int64_t)y * ya >> 15) - y)) / 1000);
}
int ui_start(struct ui_voice *v, int cue) {
  if (cue <= GM_CUE_NONE || cue >= GM_CUES) return 0;
  if (v->active) {
    /* Keep a blip that is not nearly done, unless the new one means more (select, back and can't beat a move or a tab). */
    uint32_t left = total(v) - v->pos;
    if ((cue <= GM_CUE_TAB && left > F(4)) || (cue == v->cue && left > F(4))) {v->dropped++; return 0;}
  }
  v->pos = 0; v->phase = 0; v->mphase = 0; v->rng = 0x2545F491u; v->cue = (uint8_t)cue; v->active = 1; v->started++;
  return 1;
}
void ui_mix(struct ui_voice *v, int32_t *acc, unsigned n) {
  if (!v->active) return;
  uint32_t end = total(v);
  for (unsigned k = 0; k < n && v->pos < end; k++, v->pos++) {
    uint32_t p = v->pos; unsigned seg = 0;
    while (seg + 1 < UI_SEGS && p >= cues[v->cue][seg].len) {p -= cues[v->cue][seg].len; seg++;}
    const struct note *nt = &cues[v->cue][seg];
    if (!nt->len) continue;
    uint32_t len = nt->len;
    int32_t hz = (int32_t)nt->hz0 + ((int32_t)nt->hz1 - (int32_t)nt->hz0) * (int32_t)p / (int32_t)len;
    uint32_t mhz = (uint32_t)hz * nt->ratio4 / 4u;
    v->phase += (uint32_t)hz * 89478u + (uint32_t)hz / 2u; v->mphase += mhz * 89478u + mhz / 2u;
    /* Amplitude: 1 ms attack, then a squared fade to nothing at the note's end (no click). The depth follows the same fade. */
    uint32_t rem = len - p, env = (rem * 32768u) / len; env = env * env >> 15;
    uint32_t att = p < 48 ? p * 32768u / 48u : 32768u;
    uint32_t depth = (uint32_t)nt->idx * env >> 15;
    uint32_t off = (uint32_t)((int64_t)sn(v->mphase) * (int32_t)depth * 1304);
    int32_t s = sn(v->phase + off);
    int32_t o = (int32_t)(((int64_t)s * nt->amp >> 8) * (int32_t)(env * att >> 15) >> 15);
    if (nt->click && p < 144) { /* the tap: noise, first-differenced so it is bright, gone in 3 ms */
      v->rng = v->rng * 1664525u + 1013904223u;
      int32_t nz = (int32_t)(v->rng >> 16) - 32768;
      o += (int32_t)((int64_t)nz * nt->click / 256 * (int32_t)(144 - p) / 144 >> 1);
    }
    acc[k] += (int32_t)((int64_t)o * UI_AMPLITUDE >> 15);
  }
  if (v->pos >= end) v->active = 0;
}
