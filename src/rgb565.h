/* SPDX-License-Identifier: MIT; 0x00RRGGBB to the 16-bit screen's RGB565
 * (rrrrrggggggbbbbb), each channel rounded to its nearest step, and an
 * ordered (4 x 4 Bayer) version for gradients, so they do not band. */
#pragma once
#include <stdint.h>
static inline uint32_t gm_q5(uint32_t v) {return (v * 31u + 127u) / 255u;}
static inline uint32_t gm_q6(uint32_t v) {return (v * 63u + 127u) / 255u;}
static inline uint16_t gm_565(uint32_t c) {
  return (uint16_t)(gm_q5(c >> 16 & 255u) << 11 | gm_q6(c >> 8 & 255u) << 5 | gm_q5(c & 255u));
}
static inline uint16_t gm_565_dither(uint32_t c, uint32_t x, uint32_t y) {
  static const uint8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  uint32_t t = bayer[(y & 3u) * 4u + (x & 3u)], out = 0; /* 0..15: where in a step the threshold sits */
  for (int s = 16; s >= 0; s -= 8) {
    uint32_t v = c >> s & 255u, steps = s == 8 ? 63u : 31u, q = v * steps * 16u / 255u; /* sixteenths of a step */
    uint32_t k = q / 16u + (q % 16u > t ? 1u : 0u);
    out = out << (s == 8 ? 6 : 5) | (k > steps ? steps : k);
  }
  return (uint16_t)out;
}
