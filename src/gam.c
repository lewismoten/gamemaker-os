/* SPDX-License-Identifier: MIT; original .GAM title reader (gam.h). */
#include "gam.h"
/* CP437 0x80..0xA5 as plain letters; '?' where there is none. */
static const char accents[] = "CueaaaaceeeiiiAAE??ooouuyOU?????aiounN";
static int frame_char(uint8_t c) {return c == ' ' || (c >= 9 && c <= 13) || c == 0xff || (c >= 0xb0 && c <= 0xdf);}
int gm_gam_title(const uint8_t *d, uint32_t n, char out[GM_TITLE_MAX + 1]) {
  const uint8_t *desc; uint32_t cap;
  out[0] = 0;
  if (n >= 2 && d[0] == 'G' && d[1] == 'M') {
    if (n < 305) return -1;                     /* header, scene count, scene 0's description */
    if (!(d[256] | d[257])) return -1;          /* no scenes */
    desc = d + 285; cap = 20;
  } else {
    if (n < 330) return -1;                     /* one whole old-style scene record */
    desc = d + 6; cap = 15;
  }
  uint32_t len = 0;
  while (len < cap && desc[len]) len++;
  uint32_t a = 0, b = len;
  while (a < b && frame_char(desc[a])) a++;
  while (b > a && frame_char(desc[b - 1])) b--;
  uint32_t k = 0;
  for (uint32_t i = a; i < b && k < GM_TITLE_MAX; i++) {
    uint8_t c = desc[i];
    char o = c >= 32 && c < 127 ? (char)c : c >= 0x80 && c <= 0xa5 ? accents[c - 0x80] : frame_char(c) ? ' ' : '?';
    out[k++] = o;
  }
  out[k] = 0;
  return (int)k;
}
