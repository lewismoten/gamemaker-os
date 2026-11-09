/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * Host test of audio.c: the PC-speaker sequence, the sequencer's tick rate, and the pump feeding a fake ring
 * (low-level writes only, EAGAIN and partial writes, no lost or repeated sample). Exits nonzero on a failure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"
static int fails;
#define CHECK(c, ...) do {if (!(c)) {fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n");}} while (0)
static uint8_t music_buf[65536], snd_file[2160];
static int32_t reader(void *u, const char *name, uint8_t *buf, uint32_t cap) {
  (void)u; if (strcmp(name, "T.SND")) return -1;
  uint32_t n = sizeof snd_file < cap ? sizeof snd_file : cap; memcpy(buf, snd_file, n); return (int32_t)n;
}
static void render(struct au *a, int16_t *out, unsigned n) {for (unsigned i = 0; i < n; i += AU_CHUNK) au_render(a, out + i * 2, n - i < AU_CHUNK ? n - i : AU_CHUNK);}
static void speaker(void) {
  static struct au a; static int16_t out[12000 * 2];
  for (unsigned t = 0; t < 30; t++) for (unsigned s = 0; s < 36; s++) {
    unsigned hz = s == 0 ? 440 : s == 1 ? 0 : s == 2 ? 19 : s == 3 ? 30000 : s == 35 ? 880 : 1000 + s;
    if (t != 3) hz = 100;
    snd_file[(t * 36 + s) * 2] = (uint8_t)hz; snd_file[(t * 36 + s) * 2 + 1] = (uint8_t)(hz >> 8);
  }
  au_init(&a, music_buf, sizeof music_buf);
  au_beep(&a, "T.SND", 3); CHECK(au_service(&a, reader, 0) == 1, "beep not started");
  render(&a, out, 12000);
  int cross0 = 0, loud0 = 0; /* step 0 is frames 0..329 (36 steps of 48000 * 8191 / 1193180 = 329.5 frames) */
  for (unsigned i = 1; i < 329; i++) {cross0 += (out[i * 2] > 0) != (out[(i - 1) * 2] > 0); loud0 += abs(out[i * 2]) == 3146;}
  CHECK(cross0 >= 5 && cross0 <= 7, "step 0 (440 Hz) crossings %d", cross0);
  CHECK(loud0 == 328, "square amplitude 3146 on %d of 328 frames", loud0);
  for (unsigned step = 1; step <= 2; step++) { /* 0 Hz and 19 Hz (below 20) are silent */
    int quiet = 1; for (unsigned i = (unsigned)(step * 329.5) + 2; i < (unsigned)((step + 1) * 329.5) - 1; i++) quiet &= out[i * 2] == 0;
    CHECK(quiet, "step %u not silent", step);
  }
  int clamp = 0; for (unsigned i = 990; i < 1310; i++) clamp += abs(out[i * 2]) == 3146; /* 30000 Hz clamps to 20000 */
  CHECK(clamp > 300, "step 3 clamped tone has %d loud frames", clamp);
  int late = 0; for (unsigned i = 11800; i < 11860; i++) late |= out[i * 2] != 0;
  CHECK(late, "step 35 (880 Hz) silent");
  int tail = 0; for (unsigned i = 11865; i < 12000; i++) tail |= out[i * 2] != 0;
  CHECK(!tail, "sound runs past 36 steps (11862 frames)");
  CHECK(out[0] == out[1], "channels differ");
  au_beep(&a, "T.SND", 3); au_service(&a, reader, 0); /* a retrigger restarts it at once */
  render(&a, out, 10); CHECK(abs(out[0]) == 3146, "retrigger did not restart");
  au_stop(&a); au_service(&a, reader, 0); render(&a, out, 10); CHECK(out[0] == 0, "stop did not silence");
  au_beep(&a, "NOPE.SND", 1); CHECK(au_service(&a, reader, 0) == 0, "a missing file started a sound");
}
static void ticks(void) {
  static struct au a; static int16_t out[AU_CHUNK * 2];
  FILE *f = fopen("tests/data/syn.cmf", "rb"); CHECK(f != 0, "no tests/data/syn.cmf"); if (!f) return;
  static uint8_t bytes[4096]; uint32_t n = (uint32_t)fread(bytes, 1, sizeof bytes, f); fclose(f);
  CHECK(CMF_TICK_Q16 / 65536.0 > 329.50 && CMF_TICK_Q16 / 65536.0 < 329.52, "samples per tick %f", CMF_TICK_Q16 / 65536.0);
  au_init(&a, music_buf, sizeof music_buf); memcpy(music_buf, bytes, n);
  CHECK(cmf_load(&a.cmf, music_buf, n) == 0, "song refused");
  for (unsigned i = 0; i < 48000; i += AU_CHUNK) au_render(&a, out, AU_CHUNK);
  double want = 48000 * 8191.0 / 1193180; /* a second of audio: 145.67 ticks a second */
  CHECK(a.cmf.ticks >= 145 && a.cmf.ticks <= 146 && (double)a.cmf.ticks * want > 47500, "%u ticks in a second", a.cmf.ticks);
  uint8_t bad[64]; memcpy(bad, bytes, 64); bad[12] = 100; /* a clock other than 146 is refused, as PLAYGAME does */
  CHECK(cmf_load(&a.cmf, bad, 64) == -1, "a 100 ticks/s file was loaded");
}
/* A fake ring of CAP frames, drained by `drain` frames between pumps; takes at most `take` frames a write. */
struct ring {uint32_t cap, queued; uint32_t take; uint64_t t; uint32_t low_writes; int16_t *got; uint32_t gotn; uint32_t max_queued;};
static int32_t rq(void *u, uint32_t *q, uint32_t *c) {struct ring *r = u; *q = r->queued; *c = r->cap; return 0;}
static int32_t rw(void *u, const void *pcm, uint32_t bytes) {
  struct ring *r = u; uint32_t frames = bytes / 4, room = r->cap - r->queued;
  if (!room) return -11;
  if (frames > room) frames = room;
  if (frames > r->take) frames = r->take;
  memcpy(r->got + r->gotn * 2, pcm, frames * 4); r->gotn += frames; r->queued += frames;
  if (r->queued > r->max_queued) r->max_queued = r->queued;
  return (int32_t)(frames * 4);
}
static uint64_t rn(void *u) {struct ring *r = u; return r->t += 1000;}
static void feeding(void) {
  static struct au a, ref; static int16_t got[64000 * 2], want[64000 * 2];
  static uint8_t b2[65536]; struct ring r = {16384, 0, 480, 0, 0, got, 0, 0}; struct au_sink sink = {&r, rq, rw, rn};
  FILE *f = fopen("tests/data/syn.cmf", "rb"); static uint8_t bytes[4096]; uint32_t n = f ? (uint32_t)fread(bytes, 1, sizeof bytes, f) : 0; if (f) fclose(f);
  au_init(&a, music_buf, sizeof music_buf); memcpy(music_buf, bytes, n); cmf_load(&a.cmf, music_buf, n);
  au_init(&ref, b2, sizeof b2); memcpy(b2, bytes, n); cmf_load(&ref.cmf, b2, n);
  int pumps = 0, wrote = 0, quiet_pumps = 0;
  for (unsigned ms = 0; ms < 900; ms += 10) { /* 10 ms between pumps, the ring plays 480 frames each */
    uint32_t before = r.gotn, q0 = r.queued; int w = au_pump(&a, &sink); pumps++;
    if (w == 0) quiet_pumps++; else wrote += w;
    CHECK(w >= 0, "pump failed");
    CHECK(r.gotn - before == (uint32_t)w, "pump counted %d wrote %u", w, r.gotn - before);
    if (q0 >= AU_LOW_FRAMES) CHECK(w == 0, "wrote %d frames with %u queued (not low)", w, q0);
    else CHECK(w > 0, "left a low ring (%u queued) alone", q0);
    r.queued = r.queued > 480 ? r.queued - 480 : 0;
  }
  CHECK(r.max_queued <= AU_FILL_FRAMES + 479 && r.max_queued <= r.cap, "ring filled to %u", r.max_queued);
  CHECK(quiet_pumps > 20, "only %d of %d pumps left the ring alone", quiet_pumps, pumps);
  render(&ref, want, r.gotn); CHECK(!memcmp(want, got, (size_t)r.gotn * 4), "fed samples differ from a straight render (%u frames)", r.gotn);
  /* EAGAIN: a full ring takes nothing, and the chunk is kept and goes out first later */
  struct ring full = {AU_CHUNK, AU_CHUNK, 480, 0, 0, got, 0, 0}; struct au_sink fs = {&full, rq, rw, rn};
  struct au c; static uint8_t b3[65536]; au_init(&c, b3, sizeof b3);
  CHECK(au_pump(&c, &fs) == 0 && c.hold_len == 0, "pumped into a full ring");
  full.queued = 0; full.cap = 480; full.take = 100; /* partial writes of 100 frames */
  int total = 0; for (int i = 0; i < 12 && total < 480; i++) {full.queued = 0; int w = au_pump(&c, &fs); total += w > 0 ? w : 0;}
  CHECK(total == 480 || total > 480, "partial writes lost frames: %d", total);
  printf("fed %u frames (%d reported) in %d pumps; %d quiet\n", r.gotn, wrote, pumps, quiet_pumps);
}
int main(void) {speaker(); ticks(); feeding(); if (!fails) puts("audio ok"); return fails != 0;}
