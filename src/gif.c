/* SPDX-License-Identifier: MIT; GIF decoder (gif.h), a C port of Game-Maker:
 * Sandbox Edition's formats/gif.ts (decodeGif and lzwDecode, MIT), which
 * mirrors GIFC.C: LSB-first LZW codes from min size + 1 up to 12 bits, a local
 * color table replacing the global one, interlaced rows in four passes, data
 * that ends early leaving index 0. */
#include "gif.h"
#define W 320u
#define H 200u
static int16_t prefix[4096];
static uint8_t suffix[4096], first[4096], stack[4097], codes[W * H];
static uint32_t lzw(const uint8_t *d, uint32_t n, uint32_t at, int min, uint8_t *out, uint32_t count) {
  int clear = 1 << min, eoi = clear + 1, size = min + 1, next = clear + 2, prev = -1;
  uint32_t buf = 0, bits = 0, o = 0, left = 0; /* left: bytes left in the current sub-block */
  for (int i = 0; i < clear; i++) {prefix[i] = -1; suffix[i] = (uint8_t)i; first[i] = (uint8_t)i;}
  while (o < count) {
    while (bits < (uint32_t)size) {
      if (!left) {if (at >= n || !(left = d[at++])) return o;}
      if (at >= n) return o;
      buf |= (uint32_t)d[at++] << bits; bits += 8; left--;
    }
    int code = (int)(buf & ((1u << size) - 1)); buf >>= size; bits -= (uint32_t)size;
    if (code == clear) {size = min + 1; next = clear + 2; prev = -1; continue;}
    if (code == eoi) break;
    int c = code, sp = 0;
    if (prev >= 0 && code >= next) {stack[sp++] = first[prev]; c = prev;}
    while (c >= clear && sp < 4096) {stack[sp++] = suffix[c]; c = prefix[c];}
    stack[sp++] = (uint8_t)c;
    int head = c;
    while (sp > 0 && o < count) out[o++] = stack[--sp];
    if (prev >= 0 && next < 4096) {
      prefix[next] = (int16_t)prev; suffix[next] = (uint8_t)head; first[next] = first[prev]; next++;
      if (next == 1 << size && size < 12) size++;
    }
    if (prev < 0) first[code] = (uint8_t)code;
    prev = code;
  }
  return o;
}
static void colors(const uint8_t *d, uint32_t n, uint32_t *at, uint32_t count, uint8_t *pal) {
  for (uint32_t i = 0; i < 768; i++) pal[i] = 0;
  for (uint32_t i = 0; i < count * 3; i++) pal[i] = *at < n ? d[(*at)++] >> 2 : 0;
}
int gp_gif(const uint8_t *d, uint32_t n, uint8_t *pal, uint8_t *px) {
  if (n < 13 || d[0] != 'G' || d[1] != 'I' || d[2] != 'F') return -1;
  uint32_t at = 10; uint8_t flags = d[at++]; at += 2;
  if (flags & 0x80) colors(d, n, &at, 2u << (flags & 7), pal);
  else for (uint32_t i = 0; i < 768; i++) pal[i] = 0;
  for (;;) {
    if (at >= n) return -1;
    uint8_t b = d[at++];
    if (b == 0x21) {at++; while (at < n && d[at]) at += d[at] + 1u; at++;}
    else if (b == 0x2c) break;
    else return -1;
  }
  if (at + 9 > n) return -1;
  at += 4;
  uint32_t w = d[at] | d[at + 1] << 8, h = d[at + 2] | d[at + 3] << 8; at += 4;
  if (w != W || h != H) return -1;
  flags = d[at++];
  if (flags & 0x80) colors(d, n, &at, 2u << (flags & 7), pal);
  if (at >= n) return -1;
  int min = d[at++];
  if (min < 2 || min > 8) return -1;
  for (uint32_t i = 0; i < W * H; i++) codes[i] = 0;
  lzw(d, n, at, min, codes, W * H);
  if (!(flags & 0x40)) {for (uint32_t i = 0; i < W * H; i++) px[i] = codes[i]; return 0;}
  static const uint8_t start[4] = {0, 4, 2, 1}, step[4] = {8, 8, 4, 2};
  uint32_t src = 0;
  for (int p = 0; p < 4; p++)
    for (uint32_t y = start[p]; y < H; y += step[p], src += W)
      for (uint32_t x = 0; x < W; x++) px[y * W + x] = codes[src + x];
  return 0;
}
