// SPDX-License-Identifier: MIT; plays a game from its DOS files on the host with
// a key script and prints the state after every tick in the Pocket Player host
// test's line format (S lines), so the two can be compared line for line: the
// same core, fed from the original files here and from the packed ones there.
//   gmplay <game folder> <NAME.GAM> <script>   lines: "K <tick> <scan, +128 a release>", "R <seed>", "T <ticks>",
//   "A <tick>" (every key let go before that tick, as the board's session does when a level ends)
#include <stdio.h>
#include <stdlib.h>
#include "hostio.h"
#include "gmcore.h"
gm::World &gp_world();
int main(int argc, char **argv) {
  if (argc < 4) return 2;
  if (gp_open(host_read, argv[1], argv[2]) < 0) { fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
  gm::World &w = gp_world();
  w.hold = false;  // the Pocket Player's host test commits a level change in the same tick
  struct Ev { int tick, key; } evs[4096];
  int nev = 0, maxTick = 0;
  FILE *f = fopen(argv[3], "r");
  if (!f) return 2;
  char line[64];
  while (fgets(line, sizeof line, f)) {
    int t, k; unsigned seed;
    if (sscanf(line, "K %d %d", &t, &k) == 2 && nev < 4096) evs[nev++] = {t, k};
    if (sscanf(line, "A %d", &t) == 1 && nev < 4096) evs[nev++] = {t, -1};
    if (sscanf(line, "T %d", &t) == 1) maxTick = t;
    if (sscanf(line, "R %u", &seed) == 1) w.seed = seed;
  }
  fclose(f);
  if (gp_begin()) { fprintf(stderr, "cannot start level 1\n"); return 2; }
  int e = 0;
  for (int t = 1; t <= maxTick; t++) {
    for (; e < nev && evs[e].tick == t; e++)
      if (evs[e].key < 0) w.releaseAll();
      else w.key(evs[e].key & 127, !(evs[e].key & 128));
    char code = w.tick();
    int32_t ms = 0;
    for (int i = 0; i < 30; i++) ms += w.meter[i];
    printf("S %d %c %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %u\n", t, code ? code : '-', w.level, w.cx[0], w.cx[1], w.cy[0], w.cy[1],
           w.cseq, w.cframe, w.mx, w.my, w.sc.nx, w.sc.ny, w.sc.lsx, w.sc.lsy, w.score, w.nextx, w.nexty, w.scrx, w.scry, w.hitpts, w.lives, ms, gp_frame_hash());
    w.nsnd = 0;
    if (code == 'W') break;
  }
  return 0;
}
