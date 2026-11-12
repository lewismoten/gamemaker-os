/* SPDX-License-Identifier: MIT; Host test: the menu's UI sounds. Each library action names its cue, a setting turns them all off,
 * and a burst of fast moves starts at most a few blips and never costs a frame (nothing queues, nothing waits). */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "menu.h"
#include "audio.h"
static int fails;
static void cues_and_latency(void);
#define CHECK(c) do {if (!(c)) {printf("FAIL line %d: %s\n", __LINE__, #c); fails++;}} while (0)
static struct gm_menu m;
static int key(uint16_t code) {return gm_event(&m, 1, code, 1);}
static int cue_after(uint16_t code) {key(code); return m.cue;}
static void fresh(void) {
  memset(&m, 0, sizeof m);
  for (int i = 0; i < 10; i++) {char d[9]; snprintf(d, sizeof d, "G%d", i); gm_add(&m, d, "X.GAM", d, (uint32_t)i + 1);}
  gm_finish(&m); m.tab = GM_CAT_OTHER; gm_refresh(&m);
}
/* Each cue: renders the same twice (a hash), stays within its length and peak, ends at silence, and each sound differs from the others. */
static uint32_t render(int cue, unsigned *len, int *peak) {
  struct ui_voice v; memset(&v, 0, sizeof v); ui_start(&v, cue); uint32_t h = 2166136261u; unsigned n = 0; *peak = 0; int last = 0;
  while (v.active && n < 20000) {
    int32_t acc[AU_CHUNK] = {0}; ui_mix(&v, acc, AU_CHUNK);
    for (unsigned k = 0; k < AU_CHUNK; k++) {int32_t x = acc[k]; h = (h ^ (uint32_t)x) * 16777619u; int ax = x < 0 ? -x : x; if (ax > *peak) *peak = ax; last = x;}
    n += AU_CHUNK;
  }
  *len = n; return h * 2u + (uint32_t)(last != 0);
}
/* A model of the ring: it drains 480 frames per 10 ms, the loop pumps and wakes every 10 ms, and a key arrives mid-wake. The sound is
 * heard when its first frame reaches the play head: what the ring held at the press (it is write-only, so a blip waits behind it) plus
 * the wake and the 10 ms slice. */
struct mr {uint32_t queued, cap; uint64_t written; int16_t *buf;};
static int32_t mq(void *u, uint32_t *q, uint32_t *c) {struct mr *r = u; *q = r->queued; *c = r->cap; return 0;}
static int32_t mw(void *u, const void *p, uint32_t bytes) {struct mr *r = u; uint32_t n = bytes / 4; if (n > r->cap - r->queued) n = r->cap - r->queued; if (!n) return -11;
  memcpy(r->buf + r->written * 2, p, n * 4); r->written += n; r->queued += n; return (int32_t)(n * 4);}
static uint64_t mn(void *u) {(void)u; static uint64_t t; return t += 1000;} /* the CPU timer: 1 us a call */
static void cues_and_latency(void) {
  uint32_t hs[GM_CUES]; for (int c = 1; c < GM_CUES; c++) {
    unsigned len, len2; int pk, pk2; uint32_t h = render(c, &len, &pk), h2 = render(c, &len2, &pk2);
    CHECK(h == h2 && len == len2 && pk == pk2);                           /* deterministic */
    CHECK(pk > 400 && pk <= 2000); CHECK(len <= (c == GM_CUE_SELECT ? 250u : 150u) * 48 + AU_CHUNK);
    CHECK((h & 1) == 0);                                                  /* ended at silence: no click */
    hs[c] = h; for (int d = 1; d < c; d++) CHECK(hs[d] != hs[c]);
  }
  static struct au a; static uint8_t music[64]; static int16_t pcm[48000 * 2 * 2]; static struct mr r; struct au_sink sink = {&r, mq, mw, mn};
  double worst = 0, sum = 0; int cnt = 0;
  for (unsigned phase = 0; phase < 10; phase++) {
    au_init(&a, music, sizeof music); au_set_playing(&a, 0); memset(&r, 0, sizeof r); r.cap = 16384; r.buf = pcm; memset(pcm, 0, sizeof pcm);
    uint64_t played = 0; int pressed = 0; uint64_t press_at = 0; unsigned ms = 0; int dry = 0;
    for (; ms < 1600; ms += 10) {
      if (ms == 800) {pressed = 1; press_at = played; au_ui(&a, GM_CUE_SELECT);} /* the key, then the loop's pump */
      int maxslice = 0; (void)maxslice;
      int w = au_pump(&a, &sink); CHECK(w >= 0);
      if (pressed == 1) { /* first nonzero frame of the new sound */
        for (uint64_t i = press_at; i < r.written; i++) if (pcm[i * 2]) {double lat = (double)(i - press_at) / 48.0 + 10.0 /* the wake */ + phase; sum += lat; cnt++; if (lat > worst) worst = lat; pressed = 2; break;}
      }
      if (r.queued < 480) dry++;
      uint32_t d = r.queued < 480 ? r.queued : 480; r.queued -= d; played += d; /* the ring plays 10 ms */
    }
    CHECK(pressed == 2); CHECK(dry == 0);
  }
  printf("press to the first sound sample: %.0f ms on average, %.0f ms worst (library ring, %u ms low mark; with a song playing it is the long ring, as before)\n", cnt ? sum / cnt : 0, worst, AU_QUIET_LOW_FRAMES / 48);
  CHECK(worst <= 100);
}
int main(void) {
  fresh();
  CHECK(m.settings.ui_sounds == 1);
  CHECK(cue_after(106) == GM_CUE_MOVE);          /* Right: next card */
  CHECK(cue_after(108) == GM_CUE_MOVE);          /* Down */
  CHECK(cue_after(15) == GM_CUE_TAB);            /* Tab: next tab */
  CHECK(cue_after(26) == GM_CUE_TAB);            /* [ */
  m.tab = GM_CAT_OTHER; m.pos[m.tab] = 0;
  CHECK(cue_after(105) == GM_CUE_CANT);          /* Left at the first card: blocked */
  CHECK(cue_after(28) == GM_CUE_SELECT && m.view == GM_DETAILS);
  CHECK(cue_after(106) == GM_CUE_MOVE);          /* a details button */
  CHECK(cue_after(1) == GM_CUE_BACK && m.view == GM_GRID);
  CHECK(cue_after(1) == GM_CUE_SELECT && m.options_active); /* Esc opens Options */
  CHECK(cue_after(108) == GM_CUE_MOVE);
  CHECK(cue_after(1) == GM_CUE_BACK && !m.options_active);
  CHECK(cue_after(50) == GM_CUE_NONE);           /* M only toggles music */
  /* The Options row for the setting. */
  m.options_active = 1; m.option = GM_OPT_SOUNDS;
  CHECK(cue_after(28) == GM_CUE_NONE && m.settings.ui_sounds == 0); /* turning them off is silent */
  CHECK(cue_after(28) == GM_CUE_SELECT && m.settings.ui_sounds == 1); /* turning them on blips */
  /* Off: nothing is named for any action. */
  fresh(); m.settings.ui_sounds = 0;
  int sum = 0; const uint16_t codes[] = {106, 108, 15, 28, 106, 1, 1, 108, 1, 105, 105, 105, 105};
  for (unsigned i = 0; i < sizeof codes / sizeof *codes; i++) {key(codes[i]); sum += m.cue;}
  CHECK(sum == 0);
  /* A burst of fast moves: the player starts few blips, each frame renders within its slice, no state is queued. */
  static struct au a; static uint8_t music[64]; au_init(&a, music, sizeof music);
  int16_t out[AU_CHUNK * 2]; unsigned started = 0, dropped0 = 0; (void)dropped0;
  struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
  for (unsigned i = 0; i < 20000; i++) {started += (unsigned)au_ui(&a, GM_CUE_MOVE); if (i % 50 == 0) au_render(&a, out, AU_CHUNK);}
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double ms = (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
  CHECK(started < 20000 / 10); CHECK(a.ui.dropped > 0); CHECK(ms < 500);
  /* A blip is audible even with the master volume at 0 (a silent library), tiny, and at most 250 ms (the confirm; the rest 150 ms). */
  au_init(&a, music, sizeof music); a.vol = a.vol_target = 0;
  CHECK(au_ui(&a, GM_CUE_SELECT)); int peak = 0; unsigned frames = 0;
  while (a.ui.active && frames < 9600) {au_render(&a, out, AU_CHUNK); frames += AU_CHUNK; for (unsigned k = 0; k < AU_CHUNK * 2; k++) {int v = out[k] < 0 ? -out[k] : out[k]; if (v > peak) peak = v;}}
  CHECK(peak > 300 && peak <= 2000); CHECK(frames <= 250 * 48 + AU_CHUNK);
  /* It leaves a playing clip and the speaker alone: their state does not change. */
  au_init(&a, music, sizeof music); a.vp.state = VP_PLAYING; a.pc.active = 1;
  CHECK(au_ui(&a, GM_CUE_BACK)); CHECK(a.vp.state == VP_PLAYING && a.pc.active == 1 && !a.music_pending && !a.voc_pending && !a.beep_pending);
  /* A select cuts a move, a move never cuts a select. */
  au_init(&a, music, sizeof music);
  CHECK(au_ui(&a, GM_CUE_MOVE)); CHECK(au_ui(&a, GM_CUE_SELECT)); CHECK(!au_ui(&a, GM_CUE_MOVE));
  cues_and_latency();
  puts(fails ? "UI sounds FAIL" : "UI sounds, off switch and burst bounds PASS");
  return fails ? 1 : 0;
}
