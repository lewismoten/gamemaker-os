/* SPDX-License-Identifier: MIT; production session with the real host play core. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "session.h"
#include "hostio.h"

/* Independent arithmetic oracle: ns stays within uint64, but ten times it
 * need not. This form also checks exact/one-ns-early tick boundaries. */
static uint64_t tick_at(uint64_t n) {
  return (uint64_t)(((__uint128_t)n * GS_TICK_NS10 + 9) / 10);
}
static struct gs_session s;
static void start(char *folder) {
  assert(gp_open(host_read, folder, "DICE.GAM") >= 0);
  assert(!gp_begin());
  memset(&s, 0, sizeof s);
  s.phase = GS_PLAY;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  start(argv[1]);
  s.t0 = 1000000000ull;
  s.ticks = UINT32_MAX;
  s.total_ticks = UINT32_MAX - 1;
  uint64_t next = s.t0 + tick_at((uint64_t)UINT32_MAX + 1);
  assert(gs_wake(&s, next - 1) == next); /* used to return the epoch */
  assert(!gs_step(&s, next - 1) && !s.ran);
  uint64_t at = s.t0 + tick_at((uint64_t)UINT32_MAX + 12);
  for (unsigned i = 0; i < 3; i++) {
    int r = gs_step(&s, at);
    assert(s.ran == 4);
    assert(s.ticks == (uint64_t)UINT32_MAX + (i + 1) * 4);
    assert(r == (i == 2 ? GS_DRAW : 0));
  }
  /* Total IDs intentionally remain modulo 2^32; sound's unsigned delta
   * and the report histogram must still see the actual four-tick slice. */
  assert(s.total_ticks == 10);
  assert(gs_wake(&s, at) == s.t0 + tick_at(s.ticks + 1));
  puts("UINT32_MAX: anchored wake, bounded catch-up and modular total IDs PASS");

  /* Past the former elapsed*10 overflow (~58 years) and the former
   * ticks*period overflow (~58 years), up to almost the ns clock lifetime. */
  const uint64_t ages[] = {UINT64_MAX / 10 + 1000000000ull,
                           UINT64_MAX / 2, UINT64_MAX - 1000000000ull};
  for (unsigned k = 0; k < sizeof ages / sizeof ages[0]; k++) {
    start(argv[1]);
    uint64_t n = (uint64_t)((__uint128_t)ages[k] * 10 / GS_TICK_NS10);
    s.ticks = n - 1;
    uint64_t deadline = tick_at(n);
    assert(gs_wake(&s, deadline - 1) == deadline);
    assert(!gs_step(&s, deadline - 1) && !s.ran);
    assert(gs_step(&s, deadline) == GS_DRAW && s.ran == 1);
    assert(s.ticks == n);
    assert(gs_wake(&s, deadline) == tick_at(n + 1));
  }
  puts("long-duration conversion boundaries PASS");
  gs_resume(&s, 123456789);
  assert(s.ticks == 0 && gs_wake(&s, 123456789) == 123456789 + tick_at(1));
  puts("resume reanchors level clock PASS");
  return 0;
}
