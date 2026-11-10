/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten; see vocplay.h. Original C, written with
 * Claude Sonnet 5.5; the block rules and ADPCM tables are formats/voc.ts's (MIT, as in vocplay.h), whose
 * ADPCM tables follow the Sound Blaster DSP as DOSBox's sblaster.cpp implements it. */
#include "vocplay.h"
enum {K_PCM8, K_PCM16, K_ADPCM, K_SILENCE};
static const int8_t SC1[64] = {0,1,2,3,4,5,6,7,0,-1,-2,-3,-4,-5,-6,-7,1,3,5,7,9,11,13,15,-1,-3,-5,-7,-9,-11,-13,-15,2,6,10,14,18,22,26,30,-2,-6,-10,-14,-18,-22,-26,-30,4,12,20,28,36,44,52,60,-4,-12,-20,-28,-36,-44,-52,-60};
static const uint8_t AD1[64] = {0,0,0,0,0,16,16,16,0,0,0,0,0,16,16,16,240,0,0,0,0,16,16,16,240,0,0,0,0,16,16,16,240,0,0,0,0,16,16,16,240,0,0,0,0,16,16,16,240,0,0,0,0,0,0,0,240,0,0,0,0,0,0,0};
static const int8_t SC2[40] = {0,1,2,3,0,-1,-2,-3,1,3,5,7,-1,-3,-5,-7,2,6,10,14,-2,-6,-10,-14,4,12,20,28,-4,-12,-20,-28,5,15,25,35,-5,-15,-25,-35};
static const uint8_t AD2[40] = {0,0,0,8,0,0,0,8,248,0,0,8,248,0,0,8,248,0,0,8,248,0,0,8,248,0,0,8,248,0,0,8,248,0,0,0,248,0,0,0};
static const int8_t SC3[24] = {0,1,0,-1,1,3,-1,-3,2,6,-2,-6,4,12,-4,-12,8,24,-8,-24,6,48,-16,-48};
static const uint8_t AD3[24] = {0,4,0,4,252,4,252,4,252,4,252,4,252,4,252,4,252,4,252,4,252,0,252,0};
static uint32_t tc_rate(uint32_t div) {return (1000000u + div / 2) / div;} /* Math.round(1e6 / (256 - tc)) */
static unsigned codes(unsigned codec) {return codec == 1 ? 2u : codec == 2 ? 3u : 4u;}

void vp_init(struct vp *v) {uint8_t *b = (uint8_t *)v; for (unsigned i = 0; i < sizeof *v; i++) b[i] = 0;}
void vp_stop(struct vp *v) {
  void (*close)(void *) = v->io.close; void *user = v->io.user;
  v->io.pread = 0; v->io.close = 0; v->state = 0; v->win_len = 0;
  if (close) close(user);
}
/* Bytes at off, zero past the end (a JS typed array reads undefined as 0 in these bit operations). */
static void rd(struct vp *v, uint32_t off, uint8_t *b, unsigned n) {
  int32_t got = off < v->io.size ? v->io.pread(v->io.user, off, b, n) : 0;
  if (got < 0) got = 0;
  for (unsigned i = (unsigned)got; i < n; i++) b[i] = 0;
}
static unsigned at(struct vp *v, uint32_t off) { /* one source byte through the window */
  if (!v->win_len || off < v->win_off || off - v->win_off >= v->win_len) {
    int32_t got = off < v->io.size ? v->io.pread(v->io.user, off, v->win, VP_WIN) : 0;
    v->win_off = off; v->win_len = got > 0 ? (uint32_t)got : 0;
    if (!v->win_len) return 128;
  }
  return v->win[off - v->win_off];
}
/* The next piece of sample data, following decodeVoc's block loop. 1: a segment; 0: the end; 2: out of budget (call again); -1: refused. */
static int next(struct vp *v, struct vp_cur *c, struct vp_seg *seg, unsigned budget) {
  while (budget--) {
    if (c->p >= v->io.size || c->visits++ >= VP_MAX_VISITS) return 0;
    uint8_t h[16]; rd(v, c->p, h, 16);
    unsigned type = h[0]; if (!type) return 0;
    uint32_t size = h[1] | (uint32_t)h[2] << 8 | (uint32_t)h[3] << 16, d = c->p + 4, end = d + size;
    if (end > v->io.size || end < d) end = v->io.size;
    uint32_t dlen = end > d ? end - d : 0; /* a block cut short by the end of the file */
    seg->len = 0; seg->init = 0; seg->off = 0; seg->kind = K_PCM8;
    switch (type) {
    case 1: case 2:
      if (type == 1) {
        if (!c->rate) c->rate = tc_rate(256u - h[4]);
        c->codec = h[5]; c->ad_started = 0;
        seg->off = d + 2; seg->len = dlen > 2 ? dlen - 2 : 0;
      } else {seg->off = d; seg->len = dlen;}
      if (c->codec > 3) seg->len = 0; /* an unknown codec decodes to nothing */
      else if (c->codec) {
        seg->kind = K_ADPCM;
        if (!c->ad_started) {c->ad_started = 1; seg->init = 1;} /* the first byte is the reference sample */
      }
      break;
    case 3:
      if (!c->rate) c->rate = tc_rate(256u - h[6]);
      seg->kind = K_SILENCE; seg->len = (h[4] | (uint32_t)h[5] << 8) + 1u;
      break;
    case 6: c->rstart = d + size; c->rcount = h[4] | (uint32_t)h[5] << 8; c->have_rstart = 1; break;
    case 7:
      if (c->have_rstart && c->rcount > 0 && c->rcount < 0xffff) {
        c->rcount--; c->p = c->rstart;
        if (++c->gap > VP_GAP_VISITS) return -1;
        continue;
      }
      break;
    case 8: {
      unsigned tc = h[4] | (unsigned)h[5] << 8;
      {uint32_t div = (h[7] == 1 ? 2u : 1u) * (65536u - tc); c->rate = (256000000u + div / 2) / div;}
      break;
    }
    case 9:
      c->rate = h[4] | (uint32_t)h[5] << 8 | (uint32_t)h[6] << 16 | (uint32_t)h[7] << 24;
      seg->off = d + 12; seg->len = dlen > 12 ? dlen - 12 : 0;
      if (h[8] == 16) {seg->kind = K_PCM16; seg->len &= ~1u;}
      break;
    }
    c->p = d + size; if (c->p < d) c->p = v->io.size;
    if (seg->len) {c->gap = 0; return 1;}
    if (++c->gap > VP_GAP_VISITS) return -1;
  }
  return 2;
}
static uint32_t seg_samples(const struct vp_seg *s, unsigned codec) {
  if (s->kind == K_PCM16) return s->len / 2;
  if (s->kind == K_ADPCM) return (s->len - (s->init && s->len ? 1u : 0u)) * codes(codec);
  return s->len;
}
static void first_block(struct vp *v, struct vp_cur *c) {
  uint8_t h[22]; rd(v, 0, h, 22);
  c->p = h[20] | (unsigned)h[21] << 8;
}
int vp_begin(struct vp *v, const struct vp_io *io) {
  vp_stop(v);
  v->io = *io; v->primed = v->ended = 0; v->phase = v->frame = v->frames = v->total = v->rate = 0;
  uint8_t h[20]; rd(v, 0, h, 20);
  static const char sig[] = "Creative Voice File\032";
  for (unsigned i = 0; i < 20; i++) if (h[i] != (uint8_t)sig[i]) {vp_stop(v); v->state = VP_INVALID; return VP_INVALID;}
  struct vp_cur c = {0}; first_block(v, &c);
  v->scan = c; v->state = VP_PENDING;
  return VP_PENDING;
}
int vp_prepare(struct vp *v, unsigned budget) {
  if (v->state != VP_PENDING) return v->state == VP_PLAYING ? VP_OK : v->state;
  struct vp_seg seg;
  for (;;) {
    int r = next(v, &v->scan, &seg, budget);
    if (r == 2) return VP_PENDING;
    if (r < 0) {vp_stop(v); v->state = VP_LIMIT; return VP_LIMIT;}
    if (!r) break;
    uint32_t n = seg_samples(&seg, v->scan.codec);
    if (n > VP_MAX_SAMPLES - v->total) {vp_stop(v); v->state = VP_LIMIT; return VP_LIMIT;}
    v->total += n;
  }
  uint32_t rate = v->scan.rate ? v->scan.rate : 8000u;
  if (!v->total) {vp_stop(v); v->state = VP_EMPTY; return VP_EMPTY;}
  if (rate > 1000000u) {vp_stop(v); v->state = VP_LIMIT; return VP_LIMIT;}
  v->rate = rate;
  v->frames = (uint32_t)((uint64_t)v->total * VP_RATE / rate); if (!v->frames) v->frames = 1;
  struct vp_cur c = {0}; first_block(v, &c);
  v->play = c; v->seg_open = 0; v->win_len = 0; v->primed = 0; v->ended = 0; v->phase = 0; v->frame = 0;
  v->state = VP_PLAYING;
  return VP_OK;
}
/* The next source sample (s16, before the engine's level); 0 at the end. */
static int pull(struct vp *v, int32_t *out) {
  while (!v->seg_open || v->seg_pos >= seg_samples(&v->seg, v->play.codec)) {
    if (next(v, &v->play, &v->seg, 1000) != 1) return 0;
    v->seg_open = 1; v->seg_pos = 0; v->ad_sub = 0;
    v->ad_pos = v->seg.off + (v->seg.init && v->seg.len ? 1u : 0u);
    if (v->seg.kind == K_ADPCM && v->seg.init) {v->ad_ref = (uint8_t)(v->seg.len ? at(v, v->seg.off) : 128u); v->ad_scale = 0;}
  }
  struct vp_seg *s = &v->seg; int32_t x = 0;
  switch (s->kind) {
  case K_PCM8: x = ((int32_t)at(v, s->off + v->seg_pos) - 128) * 256; break;
  case K_PCM16: {uint32_t o = s->off + v->seg_pos * 2; x = (int16_t)(at(v, o) | at(v, o + 1) << 8); break;}
  case K_SILENCE: x = 0; break;
  default: {
    unsigned codec = v->play.codec, n = codes(codec), byte = at(v, v->ad_pos), shift = 8, width = 0;
    for (unsigned k = 0; k <= v->ad_sub; k++) {width = n == 2 ? 4u : n == 3 ? (k < 2 ? 3u : 2u) : 2u; shift -= width;}
    unsigned code = (byte >> shift) & ((1u << width) - 1u);
    if (n == 3 && width == 2) code <<= 1;
    const int8_t *sc = codec == 1 ? SC1 : codec == 2 ? SC2 : SC3; const uint8_t *ad = codec == 1 ? AD1 : codec == 2 ? AD2 : AD3;
    int max = codec == 1 ? 63 : codec == 2 ? 39 : 23;
    int idx = (int)code + v->ad_scale; if (idx < 0) idx = 0; if (idx > max) idx = max;
    int ref = v->ad_ref + sc[idx]; if (ref < 0) ref = 0; if (ref > 255) ref = 255;
    v->ad_ref = (uint8_t)ref; v->ad_scale = (uint8_t)(v->ad_scale + ad[idx]);
    x = (ref - 128) * 256;
    if (++v->ad_sub == n) {v->ad_sub = 0; v->ad_pos++;}
  }
  }
  v->seg_pos++; *out = x; return 1;
}
void vp_mix(struct vp *v, int32_t *acc, unsigned n) {
  if (v->state != VP_PLAYING) return;
  if (!v->primed) { /* the first two source samples */
    v->primed = 1;
    if (!pull(v, &v->s0)) {v->s0 = 0; v->ended = 1;}
    if (v->ended || !pull(v, &v->s1)) {v->s1 = v->s0; v->ended = 1;}
  }
  for (unsigned i = 0; i < n; i++) {
    if (v->frame >= v->frames) {vp_stop(v); return;}
    int32_t f = (int32_t)v->phase;
    int64_t w = ((int64_t)v->s0 * (int32_t)(VP_RATE - (uint32_t)f) + (int64_t)v->s1 * f) * 4; /* the level 0.8 = 4/5 */
    int64_t q = (int64_t)VP_RATE * 5, y = (w >= 0 ? w + q / 2 : w - q / 2) / q;
    acc[i] += (int32_t)y; /* the web engine's level: a clip is 0.8 of full scale */
    v->frame++;
    v->phase += v->rate;
    while (v->phase >= VP_RATE) {
      v->phase -= VP_RATE; v->s0 = v->s1;
      if (!v->ended && !pull(v, &v->s1)) v->ended = 1;
    }
  }
}
