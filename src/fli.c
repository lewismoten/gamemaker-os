/* SPDX-License-Identifier: MIT; FLI decoder (fli.h), a C port of Game-Maker:
 * Sandbox Edition's formats/fli.ts (parseFli and applyFliFrame, MIT), which
 * follows RSD's code/Image/FLI/FLI.CPP: chunks 11 COLOR (64 levels), 12 LC
 * (line delta), 13 BLACK, 15 BRUN (run-length frame), 16 COPY, and the FLC
 * chunks 4 COLOR_256 and 7 SS2. Truncated packets stop at their chunk boundary. */
#include "fli.h"
static void (*checkpoint)(void);
void gp_decode_hook(void (*hook)(void)) {checkpoint = hook;}
void gp_decode_checkpoint(void) {if (checkpoint) checkpoint();}
#define FLI_MAGIC 0xaf11u
#define FLC_MAGIC 0xaf12u
#define FRAME_MAGIC 0xf1fau
static uint32_t u16(const struct gp_fli *f, uint32_t at) {return (at < f->n ? f->d[at] : 0) | (uint32_t)(at < f->n && f->n - at > 1 ? f->d[at + 1] : 0) << 8;}
static uint32_t u32(const struct gp_fli *f, uint32_t at) {return at < f->n && f->n - at >= 4 ? u16(f, at) | u16(f, at + 2) << 16 : 0;}
static uint8_t byte(const struct gp_fli *f, uint32_t at) {return at < f->n ? f->d[at] : 0;}
int gp_fli_open(struct gp_fli *f, const uint8_t *d, uint32_t n) {
  f->d = d; f->n = n;
  if (!d || n < 128) return -1;
  uint32_t magic = u16(f, 4);
  if (magic != FLI_MAGIC && magic != FLC_MAGIC) return -1;
  f->frames = u16(f, 6);
  f->w = u16(f, 8); if (!f->w) f->w = 320;
  f->h = u16(f, 10); if (!f->h) f->h = 200;
  if (f->w * f->h > GP_FLI_PIXELS) return -1;
  f->speed = u16(f, 16);
  if (magic == FLC_MAGIC) {
    uint64_t ms = u32(f, 16);
    f->speed = (uint32_t)((ms * 70u * 2u + 1000u) / 2000u); /* Math.round(ms * 70 / 1000) */
  }
  f->next = magic == FLC_MAGIC ? (u32(f, 80) ? u32(f, 80) : 128) : 128;
  f->found = 0;
  if (f->next < 128 || f->next > n) return -1;
  return 0;
}
/* The next frame record (records with another magic, such as FLC prefix chunks, are skipped); 0 if none. */
static uint32_t next_record(struct gp_fli *f) {
  while (f->found < f->frames && f->next <= f->n && f->n - f->next >= 16) {
    uint32_t at = f->next, size = u32(f, at);
    gp_decode_checkpoint();
    if (size < 16 || size > f->n - at) {f->next = f->n; return 0;}
    f->next = at + size;
    if (u16(f, at + 4) == FRAME_MAGIC) {f->found++; return at;}
  }
  return 0;
}
static void put(uint8_t *px, uint32_t size, uint32_t i, uint8_t v) {if (i < size) px[i] = v;}
static void color(const struct gp_fli *f, uint32_t p, uint32_t end, uint8_t *pal, int eight) {
  if (p > end || end - p < 2) return;
  uint32_t packets = u16(f, p), idx = 0; p += 2;
  for (uint32_t i = 0; i < packets && end - p >= 2; i++) {
    gp_decode_checkpoint();
    idx += byte(f, p++);
    uint32_t count = byte(f, p++); if (!count) count = 256;
    for (uint32_t j = 0; j < count && idx < 256; j++, idx++)
      for (int k = 0; k < 3; k++) {if (p >= end) return; uint8_t v = byte(f, p++); pal[idx * 3 + (uint32_t)k] = eight ? v >> 2 : v & 63;}
  }
}
static void brun(const struct gp_fli *f, uint32_t p, uint32_t end, uint8_t *px) {
  for (uint32_t y = 0; y < f->h && p < end; y++) {
    gp_decode_checkpoint();
    p++; /* packet count (unreliable for wide images; decoded by width) */
    uint32_t x = 0, row = y * f->w, work = 0;
    while (x < f->w && p < end) {
      if (!(work++ & 255u)) gp_decode_checkpoint();
      int n = (int8_t)byte(f, p++);
      if (n >= 0) {if (p >= end) return; uint8_t v = byte(f, p++); for (int i = 0; i < n && x < f->w; i++) px[row + x++] = v;}
      else for (int i = 0; i < -n && x < f->w && p < end; i++) px[row + x++] = byte(f, p++);
    }
  }
}
static void lc(const struct gp_fli *f, uint32_t p, uint32_t end, uint8_t *px) {
  if (p > end || end - p < 4) return;
  uint32_t size = f->w * f->h, y = u16(f, p), lines = u16(f, p + 2);
  p += 4;
  for (uint32_t l = 0; l < lines && y < f->h && p < end; l++, y++) {
    gp_decode_checkpoint();
    uint32_t packets = byte(f, p++), x = 0;
    for (uint32_t i = 0; i < packets && end - p >= 2; i++) {
      x += byte(f, p++);
      int n = (int8_t)byte(f, p++);
      uint32_t row = y * f->w;
      if (n >= 0) for (int j = 0; j < n && p < end; j++) put(px, size, row + x++, byte(f, p++));
      else {if (p >= end) return; uint8_t v = byte(f, p++); for (int j = 0; j < -n; j++) put(px, size, row + x++, v);}
    }
  }
}
static void ss2(const struct gp_fli *f, uint32_t p, uint32_t end, uint8_t *px) {
  if (p > end || end - p < 2) return;
  uint32_t size = f->w * f->h, lines = u16(f, p);
  int32_t y = 0;
  p += 2;
  while (lines > 0 && end - p >= 2 && y < (int32_t)f->h) {
    gp_decode_checkpoint();
    int32_t word = (int16_t)u16(f, p); p += 2;
    if ((word & 0xc000) == 0xc000) {y -= word; continue;} /* skip lines (word is negative) */
    if ((word & 0xc000) == 0x8000) {
      put(px, size, (uint32_t)y * f->w + f->w - 1, (uint8_t)(word & 0xff));
      if (end - p < 2) return;
      word = (int16_t)u16(f, p); p += 2;
    }
    uint32_t x = 0, row = (uint32_t)y * f->w;
    for (int32_t i = 0; i < word && end - p >= 2; i++) {
      if (!(i & 31)) gp_decode_checkpoint();
      x += byte(f, p++);
      int n = (int8_t)byte(f, p++);
      if (n >= 0) for (int j = 0; j < n && end - p >= 2; j++) {put(px, size, row + x++, byte(f, p++)); put(px, size, row + x++, byte(f, p++));}
      else {if (end - p < 2) return; uint8_t a = byte(f, p++), b = byte(f, p++); for (int j = 0; j < -n; j++) {put(px, size, row + x++, a); put(px, size, row + x++, b);}}
    }
    y++; lines--;
  }
}
int gp_fli_frame(struct gp_fli *f, uint8_t *px, uint8_t *pal) {
  uint32_t start = next_record(f);
  if (!start) return 0;
  uint32_t size = f->w * f->h, end = start + u32(f, start), chunks = u16(f, start + 6), p = start + 16;
  if (end > f->n || end < start) end = f->n;
  int changed = 0;
  for (uint32_t i = 0; i < chunks && p <= end && end - p >= 6; i++) {
    uint32_t csize = u32(f, p), type = u16(f, p + 4), data = p + 6;
    gp_decode_checkpoint();
    if (csize < 6 || csize > end - p) break;
    uint32_t cend = p + csize;
    switch (type) {
      case 11: case 4: color(f, data, cend, pal, type == 4); changed = 1; break;
      case 12: lc(f, data, cend, px); break;
      case 7: ss2(f, data, cend, px); break;
      case 13: for (uint32_t k = 0; k < size; k++) px[k] = 0; break;
      case 15: brun(f, data, cend, px); break;
      case 16: for (uint32_t k = 0; k < size && data + k < cend; k++) px[k] = f->d[data + k]; break;
    }
    p += csize;
  }
  return changed;
}
