/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * The CMF sequencer: a C port of the Sandbox Edition web engine's audio/cmfPlayer.ts and
 * cmfLoad.ts (MIT, copyright (c) 2014 gandrewstone; (c) 2026 Lewis Moten modifications), which
 * port PlayIt() from RSD's code/GM/SBMUSIC.C and the parts of Creative's SBFMDRV it calls.
 * cmf_tick() is one PlayIt call: the timer runs at 1193180 / 8191 Hz (CMF_TICK_Q16 samples). */
#pragma once
#include "opl.h"
/* Samples per tick at 48 kHz with 16 fractional bits: 48000 * 8191 / 1193180 * 65536. */
#define CMF_TICK_Q16 21594946u
struct cmf {
  struct opl *opl;
  const uint8_t *data;          /* the whole file, owned by the caller while it plays */
  uint32_t len, music, off;     /* file length; where the music block starts; PlayIt's offset into it */
  uint32_t inst_at; uint16_t ninst;
  int32_t wait; uint32_t ticks; /* cmf_tick calls so far (tests) */
  uint8_t channel, funct, rhythm, rhythm_keys, playing;
  int16_t notes[16];
  uint8_t programs[16];
};
void cmf_init(struct cmf *c, struct opl *o);
/* 0 and playing, or -1 when the file is refused (not a CMF, not 146 ticks a second, absurd count). */
int cmf_load(struct cmf *c, const uint8_t *data, uint32_t len);
void cmf_stop(struct cmf *c);
void cmf_tick(struct cmf *c);
