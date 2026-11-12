/* SPDX-License-Identifier: MIT; Host tool: renders each UI cue to a 16-bit 48 kHz mono WAV (name_cue.wav, in the directory given)
 * through ui_start/ui_mix alone, so the same file builds against an older uisound.c. Gain: argv[2] percent (default 100; 2500 is
 * about what a speaker plays after the mix is loud enough to judge by ear: the sounds themselves are quiet by design). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uisound.h"
static void le(FILE *f, uint32_t v, int n) {for (int i = 0; i < n; i++) fputc((int)(v >> (8 * i) & 255), f);}
int main(int argc, char **argv) {
  static const char *names[] = {"", "move", "tab", "select", "back", "cant"};
  const char *dir = argc > 1 ? argv[1] : "."; long gain = argc > 2 ? atol(argv[2]) : 100;
  for (int c = 1; c < GM_CUES; c++) {
    static struct ui_voice v; static int16_t pcm[24000]; memset(&v, 0, sizeof v); ui_start(&v, c);
    unsigned n = 0; while (v.active && n < 24000 - 480) {int32_t acc[480] = {0}; ui_mix(&v, acc, 480); for (unsigned k = 0; k < 480; k++) {long s = acc[k] * gain / 100; pcm[n++] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);}}
    n += 2400; /* 50 ms of silence after, so a player does not clip the tail */
    char path[512]; snprintf(path, sizeof path, "%s/%s.wav", dir, names[c]); FILE *f = fopen(path, "wb"); if (!f) return 1;
    fwrite("RIFF", 1, 4, f); le(f, 36 + n * 2, 4); fwrite("WAVEfmt ", 1, 8, f); le(f, 16, 4); le(f, 1, 2); le(f, 1, 2); le(f, 48000, 4); le(f, 96000, 4); le(f, 2, 2); le(f, 16, 2);
    fwrite("data", 1, 4, f); le(f, n * 2, 4); for (unsigned i = 0; i < n; i++) le(f, (uint16_t)(i < 24000 ? pcm[i] : 0), 2); fclose(f);
  }
  return 0;
}
