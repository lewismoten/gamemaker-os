/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * Host tool: renders a CMF song with the C synth (audio.c) to a mono 16-bit 48 kHz WAV, for comparing
 * with the web engine (tools/compare_opl.sh). Usage: audio_render song.cmf seconds out.wav */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"
static struct au au;
static void le32(unsigned char *p, uint32_t v) {p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;}
int main(int argc, char **argv) {
  if (argc < 4) return 2;
  FILE *f = fopen(argv[1], "rb"); if (!f) return 1;
  static uint8_t buf[65536]; uint32_t len = (uint32_t)fread(buf, 1, sizeof buf, f); fclose(f);
  unsigned secs = (unsigned)atoi(argv[2]), n = secs * 48000u;
  au_init(&au, buf, sizeof buf);
  if (cmf_load(&au.cmf, buf, len)) {fprintf(stderr, "refused\n"); return 1;}
  int16_t *pcm = malloc((size_t)n * 2);
  int16_t blk[AU_CHUNK * 2];
  for (unsigned i = 0; i < n; i += AU_CHUNK) {
    unsigned k = n - i < AU_CHUNK ? n - i : AU_CHUNK;
    au_render(&au, blk, k);
    for (unsigned j = 0; j < k; j++) pcm[i + j] = blk[j * 2];
  }
  unsigned char h[45] = "RIFF....WAVEfmt \x10\0\0\0\1\0\1\0\x80\xbb\0\0\0\x77\x01\0\2\0\x10\0data....";
  le32(h + 4, 36 + n * 2); le32(h + 40, n * 2);
  f = fopen(argv[3], "wb"); fwrite(h, 1, 44, f); fwrite(pcm, 2, n, f); fclose(f);
  return 0;
}
