/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * While the screen is dark (au_sleep) the pump renders and writes nothing, so the synth costs no CPU; waking resumes
 * the song from where it stood, fading in from silence, with no catch-up for the time that passed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "audio.h"
static struct au au;
static uint8_t song[4096];
static uint64_t clock_ns, written_frames; static uint32_t queued;
static int32_t q(void *u, uint32_t *qu, uint32_t *cap) {(void)u; *qu = queued; *cap = 16384; return 0;}
static int32_t w(void *u, const void *p, uint32_t bytes) {(void)u; (void)p; written_frames += bytes / 4; queued += bytes / 4; return (int32_t)bytes;}
static uint64_t now(void *u) {(void)u; return clock_ns;}
static const struct au_sink sink = {0, q, w, now};
/* The ring drains in real time (48 frames per ms). */
static void tick(uint64_t ns) {clock_ns += ns; uint32_t d = (uint32_t)(ns / 1000000u) * 48u; queued = queued > d ? queued - d : 0;}
int main(void) {
  FILE *f = fopen("tests/data/syn.cmf", "rb"); assert(f);
  uint32_t n = (uint32_t)fread(song, 1, sizeof song, f); fclose(f);
  au_init(&au, song, sizeof song); clock_ns = 1000000000ull;
  assert(!cmf_load(&au.cmf, song, n)); au.vol = au.vol_target = AU_VOL_ONE;
  for (int i = 0; i < 50; i++) {assert(au_pump(&au, &sink) >= 0); tick(10000000ull);}
  assert(au.cmf.playing && written_frames > 0 && au.vol == AU_VOL_ONE);
  au_sleep(&au, 1);
  uint64_t frames = au.frames, wrote = written_frames; uint32_t writes = au.writes;
  for (int i = 0; i < 3000; i++) {assert(au_pump(&au, &sink) == 0); tick(10000000ull);} /* 30 s dark */
  assert(au.frames == frames && written_frames == wrote && au.writes == writes); /* nothing rendered, nothing written */
  assert(au.vol == 0 && au.cmf.playing);                                         /* the song stands still */
  au_sleep(&au, 1); assert(au.asleep);                                           /* idempotent */
  au_sleep(&au, 0);
  assert(!au.asleep && au.vol == 0 && au.vol_target == AU_VOL_ONE);              /* fades in from silence */
  assert(au_pump(&au, &sink) >= 0);
  assert(written_frames > wrote && au.pump_gap_ns == 0);                         /* sound again, no 30 s catch-up */
  assert(written_frames - wrote <= AU_FILL_FRAMES);
  for (int i = 0; i < 100; i++) {au_pump(&au, &sink); tick(10000000ull);}
  assert(au.vol == AU_VOL_ONE && au.cmf.playing);                                /* back at full volume within a second */
  /* a key wakes the sounds first: a menu sound asked right after au_sleep(0) is heard, one asked while asleep is not */
  au_sleep(&au, 1); au_sleep(&au, 0);
  uint64_t before = written_frames; au_ui(&au, 1);
  for (int i = 0; i < 5; i++) {assert(au_pump(&au, &sink) >= 0); tick(10000000ull);}
  assert(written_frames > before);
  /* the song's clocks stand still for the dark time: a deadline moves later by exactly that, and 0 stays unset */
  assert(au_shift_dark(1000, 200, 700) == 1500 && au_shift_dark(0, 200, 700) == 0 && au_shift_dark(1000, 700, 200) == 1000);
  puts("sleep stops the synth and wake resumes it PASS");
  return 0;
}
