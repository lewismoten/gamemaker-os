/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten; the UI blips (uisound.h). Integer only. */
#include "uisound.h"
#define F(ms) ((uint16_t)((ms) * 48u)) /* ms to frames at 48 kHz */
static const struct {uint16_t hz[UI_SEGS], len[UI_SEGS];} cues[GM_CUES] = {
  {{0, 0}, {0, 0}},
  {{1320, 0}, {F(14), 0}},          /* move: one tick */
  {{880, 1320}, {F(20), F(24)}},    /* tab: a small step up */
  {{1047, 1568}, {F(24), F(32)}},   /* select: a rising fifth-ish pair */
  {{784, 523}, {F(24), F(30)}},     /* back: falls */
  {{196, 0}, {F(56), 0}},           /* can't: one low, soft note */
};
static uint32_t total(const struct ui_voice *v) {uint32_t t = 0; for (unsigned i = 0; i < UI_SEGS; i++) t += v->len[i]; return t;}
int ui_start(struct ui_voice *v, int cue) {
  if (cue <= GM_CUE_NONE || cue >= GM_CUES) return 0;
  if (v->active) {
    /* Keep a blip that is not nearly done, unless the new one means more (select, back and can't beat a move or a tab). */
    uint32_t left = total(v) - v->pos;
    if ((cue <= GM_CUE_TAB && left > F(4)) || (cue == v->cue && left > F(4))) {v->dropped++; return 0;}
  }
  for (unsigned i = 0; i < UI_SEGS; i++) {v->hz[i] = cues[cue].hz[i]; v->len[i] = cues[cue].len[i];}
  v->pos = 0; v->phase = 0; v->cue = (uint8_t)cue; v->active = 1; v->started++;
  return 1;
}
void ui_mix(struct ui_voice *v, int32_t *acc, unsigned n) {
  if (!v->active) return;
  uint32_t end = total(v);
  for (unsigned k = 0; k < n && v->pos < end; k++, v->pos++) {
    uint32_t p = v->pos; unsigned seg = 0;
    while (seg + 1 < UI_SEGS && p >= v->len[seg]) {p -= v->len[seg]; seg++;}
    if (!v->hz[seg]) continue;
    v->phase += (uint32_t)(((uint64_t)v->hz[seg] << 32) / 48000u);
    /* Triangle wave: soft, no buzz. */
    int32_t t = (int32_t)(v->phase >> 16); t = t < 32768 ? t - 16384 : 49151 - t; /* -16384..16383 */
    /* Envelope: 2 ms attack, a linear fade to nothing over the last 40% of the segment, so nothing clicks. */
    uint32_t len = v->len[seg], env = 32768;
    if (p < 96) env = p * 32768u / 96u;
    if (p > len - len * 2u / 5u) env = env * (len - p) / (len * 2u / 5u);
    acc[k] += (int32_t)((int64_t)t * UI_AMPLITUDE / 16384 * env >> 15);
  }
  if (v->pos >= end) v->active = 0;
}
