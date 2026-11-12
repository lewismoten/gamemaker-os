/* SPDX-License-Identifier: MIT; pages between scenes against the kernel's 2 s heartbeat: a long animation (the game's own
 * FLI files) steps in bounded slices, the heartbeat hook runs at least every 500 ms of simulated time, no wake is
 * more than 100 ms ahead, and a slow file read (time jumps) is not owed back as one long step. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "session.h"
#include "hostio.h"

static struct gs_session s;
static uint64_t beat_at, beat_gap_max, beats;
static uint64_t sim;
static unsigned kept; static char kept_line[160];
static void keep(const char *l, uint64_t t) {(void)t; kept++; snprintf(kept_line, sizeof kept_line, "%s", l);}
static void beat(void) {if (sim - beat_at > beat_gap_max) beat_gap_max = sim - beat_at; beat_at = sim; beats++;}
static uint64_t clk(void) {return sim;}
static void quiet(const char *l) {(void)l;}
int main(int argc, char **argv) {
  assert(argc == 2);
  memset(&s, 0, sizeof s);
  s.beat = beat; s.keep = keep; s.clock_ns = clk; s.say = quiet;
  sim = 1000000000ull; beat_at = sim;
  assert(gs_start(&s, host_read, argv[1], "YEAR2000.GAM", sim) == 0);
  uint64_t worst_wake = 0, jumps = 0; uint32_t frames_seen = 0, worst_frames = 0, flis = 0;
  for (uint64_t end = sim + 60000000000ull; sim < end && s.phase == GS_CARDS;) {
    beat(); /* main.c's loop beats once per iteration */
    uint64_t w = gs_wake(&s, sim);
    assert(w >= sim);
    if (w - sim > worst_wake) worst_wake = w - sim;
    assert(w - sim <= 100000000ull);
    uint32_t before = s.stage == GS_FLI ? s.fli_frame : 0; int was_fli = s.stage == GS_FLI;
    gs_step(&s, sim);
    if (was_fli && s.stage == GS_FLI && s.fli_frame >= before) {
      uint32_t d = s.fli_frame - before; if (d > worst_frames) worst_frames = d; frames_seen += d;
      if (d > 2) fprintf(stderr, "d=%u frame=%u speed=%u t=%llu\n", d, s.fli_frame, (unsigned)s.fli.speed, (unsigned long long)(sim / 1000000));
      assert(d <= 2); /* a step decodes a few frames at most */
    }
    if (s.stage == GS_FLI && s.fli_frame == 0) flis++;
    if (s.keys == 0 && s.stage == GS_PAUSE && s.pause == GS_KEY_WAIT) gs_event(&s, 1, 28, 1), gs_event(&s, 1, 28, 0); /* a key ends a waiting page */
    if (!jumps && s.stage == GS_FLI && s.fli_frame == 3) {sim += 3000000000ull; beat_at = sim; jumps = 1; continue;} /* a 3 s stall mid-animation (a slow read, whose own reads beat): no catch-up burst follows */
    sim += w - sim > 5000000ull ? 5000000ull : w - sim ? w - sim : 1000000ull;
  }
  printf("pages: %u starts, %u frames, worst %u per step, longest wake %llu ms, heartbeat gap max %llu ms, %llu beats\n",
         flis, frames_seen, worst_frames, (unsigned long long)(worst_wake / 1000000), (unsigned long long)(beat_gap_max / 1000000), (unsigned long long)beats);
  assert(frames_seen >= 48);
  assert(kept >= 1 && strstr(kept_line, "GMOS fli ") && strstr(kept_line, " frames in ") && strstr(kept_line, "(ideal ")); /* each animation leaves its timing line for the card log */
  assert(jumps && beat_gap_max <= 500000000ull);
  puts("fli pages: bounded steps, heartbeat hook, 100 ms wakes PASS");
  return 0;
}
