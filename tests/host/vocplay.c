/* Host harness for src/vocplay.c: vochost <file.voc> <out.raw> [chunk] plays one clip through the streaming
 * player (a 384-byte window over a pread of the file), mixes it into a silent accumulator and writes mono s16 at
 * 48 kHz. Prints: status rate total frames preads maxread. tests/test_vocplay.py and tools/voc_compare.ts use it. */
#include "vocplay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *file; static uint32_t flen, preads, maxread, closed;
static int32_t rd(void *u, uint32_t off, void *buf, uint32_t n) {
  (void)u; preads++; if (n > maxread) maxread = n;
  if (off >= flen) return 0;
  if (n > flen - off) n = flen - off;
  memcpy(buf, file + off, n); return (int32_t)n;
}
static void cl(void *u) {(void)u; closed++;}
static struct vp v;
int main(int argc, char **argv) {
  if (argc < 3) return 2;
  FILE *f = fopen(argv[1], "rb"); if (!f) return 3;
  fseek(f, 0, SEEK_END); flen = (uint32_t)ftell(f); fseek(f, 0, SEEK_SET);
  file = malloc(flen ? flen : 1); if (fread(file, 1, flen, f) != flen) return 3; fclose(f);
  unsigned chunk = argc > 3 ? (unsigned)atoi(argv[3]) : 480;
  vp_init(&v);
  struct vp_io io = {0, rd, cl, flen};
  int st = vp_begin(&v, &io);
  unsigned guard = 0;
  while (st == VP_PENDING && guard++ < 1000000) st = vp_prepare(&v, 64);
  FILE *o = fopen(argv[2], "wb"); if (!o) return 3;
  uint32_t frames = 0;
  if (st == VP_OK) {
    frames = v.frames;
    int32_t acc[4096]; int16_t s[4096];
    uint32_t wrote = 0;
    while (v.state == VP_PLAYING && wrote <= frames + chunk) { /* a runaway player is stopped here */
      wrote += chunk;
      memset(acc, 0, sizeof acc);
      vp_mix(&v, acc, chunk);
      for (unsigned i = 0; i < chunk; i++) s[i] = (int16_t)(acc[i] > 32767 ? 32767 : acc[i] < -32768 ? -32768 : acc[i]);
      fwrite(s, 2, chunk, o);
    }
  }
  fclose(o);
  printf("%d %u %u %u %u %u %u\n", st, st == VP_OK ? v.rate : 0, st == VP_OK ? v.total : 0, frames, preads, maxread, closed);
  return 0;
}
