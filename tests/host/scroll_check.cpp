// SPDX-License-Identifier: MIT; the scrolling picture moves a little every tick, never in block jumps with stills between.
// Holds Right from tick 1 and measures, for the judged ticks (after the view caught up), how far
// the 640x480 picture gs_draw makes (what the Pi shows) shifted since the last tick (best alignment of every row, game pixels).
// A scroll that moves 4..5 px per tick shows PLAYGAME's 4 px plus 0-3 px fine pan; block jumps of 16 px with stills between fail.
// usage: scroll_check folder NAME.GAM from to min max [-v]: judges ticks from..to (the run goes to "to"), shifts must be min..max
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "hostio.h"
extern "C" {
#include "session.h"
}
#include "gmcore.h"
static long cost(const std::vector<uint32_t> &prev, const std::vector<uint32_t> &cur, int s) {
  long bad = 0, n = 0;
  for (int y = (int)GS_VIEW_Y; y < (int)GS_VIEW_Y + 390; y += 2) for (int x = 40; x < 560; x += 2) {
    int xs = x + 2 * s; if (xs < 40 || xs >= 560) continue;
    n++; if (prev[y * 640 + xs] != cur[y * 640 + x]) bad++;
  }
  return bad * 1000 / (n ? n : 1);
}
// How far the scene moved left (positive) since the last picture: cur(x) == prev(x + 2 s).
static int measure(const std::vector<uint32_t> &prev, const std::vector<uint32_t> &cur, long *mis) {
  int best = 0; long bestc = 1001;
  for (int s = -20; s <= 20; s++) {long c = cost(prev, cur, s); if (c < bestc) {bestc = c; best = s;}}
  *mis = bestc; return best;
}
int main(int argc, char **argv) {
  if (argc < 7) {fprintf(stderr, "usage: scroll_check folder NAME.GAM from to min max [-v]\n"); return 2;}
  unsigned from = (unsigned)atoi(argv[3]), n = (unsigned)atoi(argv[4]); int lo = atoi(argv[5]), hi = atoi(argv[6]), v = argc > 7;
  if (gp_open(host_read, argv[1], argv[2]) < 0) return 1;
  gp_world().seed = 1; if (gp_begin()) return 1;
  static gs_session s; s = {}; s.phase = GS_PLAY; gs_resume(&s, 0);
  gp_key(77, 1);
  static uint32_t fb[640 * 480];
  std::vector<uint32_t> prev, cur;
  int bad = 0, hist[41] = {0}; unsigned judged = 0;
  for (unsigned t = 1; t <= n; t++) {
    uint64_t at = ((uint64_t)t * GS_TICK_NS10 + 9) / 10;
    do {gs_step(&s, at);} while (s.phase == GS_PLAY && s.total_ticks < t);
    if (s.phase != GS_PLAY) break;
    gs_draw(&s, fb, 640);
    cur.assign(fb, fb + 640 * 480);
    if (t > 1) {
      long mis; int m = measure(prev, cur, &mis);
      if (v) printf("t=%u shift=%d mismatch=%ld/1000\n", t, m, mis);
      if (t >= from) {judged++; hist[m + 20 < 0 ? 0 : m + 20 > 40 ? 40 : m + 20]++; if (m < lo || m > hi) bad++;}
    }
    prev.swap(cur);
  }
  printf("ticks %u (judged from tick %u: %u): out-of-range shifts %d; shift histogram:", n, from, judged, bad);
  for (int i = 0; i <= 40; i++) if (hist[i]) printf(" %d:%d", i - 20, hist[i]);
  puts(bad || !judged ? "\nFAIL" : "\nPASS: the picture scrolls steadily every tick");
  return bad || !judged ? 1 : 0;
}
