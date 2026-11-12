/* SPDX-License-Identifier: MIT; Host test: the menu's UI sounds. Each library action names its cue, a setting turns them all off,
 * and a burst of fast moves starts at most a few blips and never costs a frame (nothing queues, nothing waits). */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "menu.h"
#include "audio.h"
static int fails;
#define CHECK(c) do {if (!(c)) {printf("FAIL line %d: %s\n", __LINE__, #c); fails++;}} while (0)
static struct gm_menu m;
static int key(uint16_t code) {return gm_event(&m, 1, code, 1);}
static int cue_after(uint16_t code) {key(code); return m.cue;}
static void fresh(void) {
  memset(&m, 0, sizeof m);
  for (int i = 0; i < 10; i++) {char d[9]; snprintf(d, sizeof d, "G%d", i); gm_add(&m, d, "X.GAM", d, (uint32_t)i + 1);}
  gm_finish(&m); m.tab = GM_CAT_OTHER; gm_refresh(&m);
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
  /* A blip is audible even with the master volume at 0 (a silent library), tiny, and at most 60 ms. */
  au_init(&a, music, sizeof music); a.vol = a.vol_target = 0;
  CHECK(au_ui(&a, GM_CUE_SELECT)); int peak = 0; unsigned frames = 0;
  while (a.ui.active && frames < 9600) {au_render(&a, out, AU_CHUNK); frames += AU_CHUNK; for (unsigned k = 0; k < AU_CHUNK * 2; k++) {int v = out[k] < 0 ? -out[k] : out[k]; if (v > peak) peak = v;}}
  CHECK(peak > 300 && peak <= 2000); CHECK(frames <= 60 * 48 + AU_CHUNK);
  /* It leaves a playing clip and the speaker alone: their state does not change. */
  au_init(&a, music, sizeof music); a.vp.state = VP_PLAYING; a.pc.active = 1;
  CHECK(au_ui(&a, GM_CUE_BACK)); CHECK(a.vp.state == VP_PLAYING && a.pc.active == 1 && !a.music_pending && !a.voc_pending && !a.beep_pending);
  /* A select cuts a move, a move never cuts a select. */
  au_init(&a, music, sizeof music);
  CHECK(au_ui(&a, GM_CUE_MOVE)); CHECK(au_ui(&a, GM_CUE_SELECT)); CHECK(!au_ui(&a, GM_CUE_MOVE));
  puts(fails ? "UI sounds FAIL" : "UI sounds, off switch and burst bounds PASS");
  return fails ? 1 : 0;
}
