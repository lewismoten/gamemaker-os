/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * UI sounds: short, quiet blips for GMOS's own menus (library, details, Options), one more voice in the audio mix
 * (audio.h) that never touches the song or a voice clip. gm_event names the cue (menu.h, m->cue); the player starts
 * it with au_ui. One sound plays at a time (FM notes with pitch sweeps, arpeggios and a noise tap; at most 150 ms, the confirm 250 ms): a cue that arrives while a longer or equal one is still
 * sounding is dropped, so a burst of fast moves queues nothing and nothing waits. */
#pragma once
#include <stdint.h>
enum {GM_CUE_NONE, GM_CUE_MOVE, GM_CUE_TAB, GM_CUE_SELECT, GM_CUE_BACK, GM_CUE_CANT, GM_CUES};
#define UI_AMPLITUDE 1700 /* about 6% of full scale at the loudest peak, under the 0.096 song */
#define UI_SEGS 4u
#define UI_MAX_FRAMES 12000u /* 250 ms: no cue is longer */
struct ui_voice {
  uint32_t pos, phase, mphase, rng;   /* frames played; carrier and modulator phase; the noise tap's state */
  uint8_t cue, active;
  uint32_t started, dropped;          /* counters, for the log and the tests */
};
/* Starts a cue (GM_CUE_*) unless one is sounding that would be cut short by it; returns 1 when started. */
int ui_start(struct ui_voice *v, int cue);
/* Adds n frames (at most 1920) of the blip to acc (mono, like the other voices). */
void ui_mix(struct ui_voice *v, int32_t *acc, unsigned n);
