/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * An integer YM3812 (OPL2) synthesizer, a port of the Sandbox Edition web engine's
 * audio/opl.ts (MIT, copyright (c) 2014 gandrewstone; (c) 2026 Lewis Moten modifications) to
 * fixed-point math: lookup tables (opl_tables.c, from tools/make_opl_tables.py), 32-bit phase
 * accumulators, envelopes in decibels with 24 fractional bits. No floats and no libm in the
 * sample loop; it runs at 48 kHz, mono. Original source, not an emulator of the chip: it
 * follows the web port's structure, as that follows Creative's SBFMDRV register use. */
#pragma once
#include <stdint.h>
#define OPL_RATE 48000u
/* 12-bit units of the summed output: 4096 is one full-scale operator. */
struct opl_op {
  uint8_t am, vib, hold, ksr, mult2, ksl, tl, ar, dr, sl, rr, wave, stage;
  uint32_t phase, inc;
  int32_t env;       /* attenuation in dB, 24 fractional bits */
  int16_t out, prev; /* the last two outputs (feedback) */
  uint32_t base;     /* TL and key scale attenuation, dB with 8 fractional bits */
};
struct opl_ch {
  uint16_t fnum; uint8_t block, key, fb, add;
  struct opl_op op[2];
};
struct opl {
  struct opl_ch ch[9];
  uint8_t regs[256];
  uint8_t wave_select, note_sel, am_depth, vib_depth, rhythm, rhythm_keys;
  uint32_t lfo_t, lfo_v, noise;
};
void opl_reset(struct opl *o);
void opl_write(struct opl *o, unsigned reg, unsigned value);
unsigned opl_read(const struct opl *o, unsigned reg);
/* Adds n mono samples (16-bit scale, gain 0.12 * 0.8 as the web engine's music path) into acc. */
void opl_render(struct opl *o, int32_t *acc, unsigned n);
extern const uint16_t opl_sine[256], opl_exp2[64], opl_note[128];
extern const uint32_t opl_attack[64], opl_decay[64];
extern const uint8_t opl_ksl[16];
