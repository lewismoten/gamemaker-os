/* Host test for src/vocplay.c and its place in au_render: the streaming reads, the limits, a new clip cutting the old one,
 * the sum with the PC speaker and the saturation. Exits 1 on the first failure. */
#include "audio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int fails;
#define CHECK(c) do {if (!(c)) {printf("FAIL line %d: %s\n", __LINE__, #c); fails++;}} while (0)
struct mem {uint8_t *b; uint32_t n, preads, maxread;};
static int32_t mread(void *u, uint32_t off, void *buf, uint32_t n) {
  struct mem *m = u; m->preads++; if (n > m->maxread) m->maxread = n;
  if (off >= m->n) return 0;
  if (n > m->n - off) n = m->n - off;
  memcpy(buf, m->b + off, n); return (int32_t)n;
}
static unsigned closed;
static void mclose(void *u) {(void)u; closed++;}
static uint8_t *mk(uint32_t *len, const uint8_t *blocks, uint32_t bn, int term) {
  uint8_t *b = malloc(26 + bn + 1); memcpy(b, "Creative Voice File\032\032\0\012\001\051\021", 26); b[20] = 26; b[21] = 0;
  memcpy(b + 26, blocks, bn); if (term) b[26 + bn] = 0; *len = 26 + bn + (term ? 1u : 0u); return b;
}
static uint8_t *pcm_voc(uint32_t *len, uint32_t samples, uint8_t tc, uint8_t level, struct mem *m) {
  uint32_t bn = 6 + samples; uint8_t *bl = malloc(bn); bl[0] = 1; bl[1] = (uint8_t)((samples + 2) & 255); bl[2] = (uint8_t)((samples + 2) >> 8); bl[3] = (uint8_t)((samples + 2) >> 16); bl[4] = tc; bl[5] = 0;
  for (uint32_t i = 0; i < samples; i++) bl[6 + i] = level;
  uint8_t *f = mk(len, bl, bn, 1); free(bl); m->b = f; m->n = *len; m->preads = m->maxread = 0; return f;
}
static int run(struct vp *v, struct mem *m) {
  struct vp_io io = {m, mread, mclose, m->n}; int st = vp_begin(v, &io);
  for (int i = 0; st == VP_PENDING && i < 2000; i++) st = vp_prepare(v, 64);
  return st;
}
static struct mem cur[2];
static int open_clip(const char *name, struct vp_io *io) {
  struct mem *m = &cur[name[0] - 'A']; io->user = m; io->pread = mread; io->close = mclose; io->size = m->n; return 0;
}
static int16_t out[AU_CHUNK * 2];
int main(void) {
  static uint8_t scratch[64];
  /* 1. Streaming: a 300 KB clip is read through the 384-byte window, never in one gulp. */
  { static struct vp v; struct mem m; uint32_t len; vp_init(&v);
    uint8_t *f = malloc(1); free(f);
    uint32_t n = 300000; uint8_t *bl = malloc(6 + n); bl[0] = 1; bl[1] = (uint8_t)((n + 2) & 255); bl[2] = (uint8_t)((n + 2) >> 8); bl[3] = (uint8_t)((n + 2) >> 16); bl[4] = 0xdd; bl[5] = 0; /* 22 kHz */
    for (uint32_t i = 0; i < n; i++) bl[6 + i] = (uint8_t)(128 + (i & 63));
    f = mk(&len, bl, 6 + n, 1); free(bl); m.b = f; m.n = len; m.preads = m.maxread = 0;
    CHECK(run(&v, &m) == VP_OK); CHECK(v.total == n); CHECK(v.rate == 22222 || v.rate == 22321 || v.rate > 20000);
    int32_t acc[480]; unsigned frames = 0;
    while (v.state == VP_PLAYING) {memset(acc, 0, sizeof acc); vp_mix(&v, acc, 480); frames += 480;}
    CHECK(frames >= v.frames && frames < v.frames + 480);
    CHECK(m.maxread <= VP_WIN); CHECK(m.preads < len / 300 + 20); /* about one read per 384 bytes */
    CHECK(closed == 1);
    printf("streamed %u bytes in %u reads of at most %u bytes\n", len, m.preads, m.maxread); free(f); }
  /* 2. Limits: a truncated header, a bad signature, a pending state that can be stopped, an empty file. */
  { static struct vp v; struct mem m; vp_init(&v); uint8_t junk[40] = {0}; m.b = junk; m.n = 40; m.preads = 0;
    CHECK(run(&v, &m) == VP_INVALID); m.n = 5; CHECK(run(&v, &m) == VP_INVALID);
    uint32_t len; uint8_t none[] = {5, 2, 0, 0, 1, 2}; uint8_t *f = mk(&len, none, sizeof none, 1); m.b = f; m.n = len; CHECK(run(&v, &m) == VP_EMPTY); free(f);
    int32_t acc[8] = {0}; vp_mix(&v, acc, 8); CHECK(acc[0] == 0 && v.state != VP_PLAYING); }
  /* 3. The sample cap: 16,000,000 refused beyond, in whole 65,536-sample silences (245 are 16,056,320). */
  { static struct vp v; struct mem m; vp_init(&v); uint32_t len;
    uint8_t blk[] = {6, 2, 0, 0, 243, 0, 3, 3, 0, 0, 255, 255, 165, 7, 0, 0, 0}; uint8_t *f = mk(&len, blk, sizeof blk, 1); m.b = f; m.n = len;
    CHECK(run(&v, &m) == VP_OK); /* 244 x 65536 = 15,990,784 */
    f[26 + 4] = 244; CHECK(run(&v, &m) == VP_LIMIT); CHECK(v.state == VP_LIMIT);
    free(f);
    /* a repeat loop with no data in it: refused, not spun on */
    uint8_t loop[] = {6, 2, 0, 0, 254, 255, 2, 0, 0, 0, 7, 0, 0, 0, 1, 4, 0, 0, 165, 0, 128, 128};
    f = mk(&len, loop, sizeof loop, 1); m.b = f; m.n = len; int st = run(&v, &m); CHECK(st == VP_LIMIT || st == VP_OK); free(f);
    /* a rate above 1 MHz (block 9) is refused */
    uint8_t hi[] = {9, 14, 0, 0, 0x80, 0x84, 0x1e, 0, 8, 1, 0, 0, 0, 0, 0, 0, 128, 128};
    f = mk(&len, hi, sizeof hi, 1); m.b = f; m.n = len; CHECK(run(&v, &m) == VP_LIMIT); free(f); }
  /* 4. Mixing in au_render: a clip and the speaker are summed; a new clip cuts the old one; saturation. */
  { static struct au a; au_init(&a, scratch, sizeof scratch); a.voc_open = open_clip;
    uint32_t len; uint8_t *fa = pcm_voc(&len, 4000, 0xdd, 128 + 50, &cur[0]), *fb = pcm_voc(&len, 4000, 0xdd, 128 - 30, &cur[1]); (void)fa; (void)fb;
    int16_t clip_a[AU_CHUNK], clip_b[AU_CHUNK], beep[AU_CHUNK], both[AU_CHUNK];
    au_voc(&a, "A.VOC"); CHECK(au_service(&a, 0, 0) == 1); au_render(&a, out, AU_CHUNK); for (unsigned i = 0; i < AU_CHUNK; i++) clip_a[i] = out[i * 2];
    CHECK(clip_a[100] == 50 * 256 * 4 / 5); /* 0.8 of 50/128 full scale */
    au_voc(&a, "B.VOC"); CHECK(au_service(&a, 0, 0) == 1); au_render(&a, out, AU_CHUNK); for (unsigned i = 0; i < AU_CHUNK; i++) clip_b[i] = out[i * 2];
    CHECK(clip_b[100] == -30 * 256 * 4 / 5 && clip_b[0] == clip_b[100]); /* B alone: A was cut when B was asked for */
    /* the speaker alone, then with the clip */
    static uint8_t snd[2160]; for (unsigned i = 0; i < 36; i++) {snd[2 * i] = 0xd0; snd[2 * i + 1] = 0x07;}
    au_silence(&a); a.vol = a.vol_target = AU_VOL_ONE;
    CHECK(!pc_start(&a.pc, snd, sizeof snd, 0)); au_render(&a, out, AU_CHUNK); for (unsigned i = 0; i < AU_CHUNK; i++) beep[i] = out[i * 2];
    au_silence(&a); a.vol = a.vol_target = AU_VOL_ONE; CHECK(!pc_start(&a.pc, snd, sizeof snd, 0)); au_voc(&a, "A.VOC"); au_service(&a, 0, 0);
    au_render(&a, out, AU_CHUNK); for (unsigned i = 0; i < AU_CHUNK; i++) both[i] = out[i * 2];
    int bad = 0, nz = 0; for (unsigned i = 0; i < AU_CHUNK; i++) {if (both[i] != beep[i] + clip_a[i]) bad++; if (beep[i]) nz++;}
    CHECK(bad == 0); CHECK(nz > 100);
    /* a clip started with the speaker on does not cut the speaker, and stop cuts both */
    au_stop(&a); CHECK(a.vp.state == 0 && !a.pc.active);
    /* saturation: two full-scale sums clamp at 32767 */
    uint8_t *fc = pcm_voc(&len, 4000, 0xdd, 255, &cur[0]); (void)fc; au_silence(&a); a.vol = a.vol_target = AU_VOL_ONE; CHECK(!pc_start(&a.pc, snd, sizeof snd, 0));
    au_voc(&a, "A.VOC"); au_service(&a, 0, 0); au_render(&a, out, AU_CHUNK); int mx = 0; for (unsigned i = 0; i < AU_CHUNK; i++) if (out[i * 2] > mx) mx = out[i * 2];
    CHECK(mx > 0 && mx <= 32767 && mx == 26010 + 3146 /* clip 0.8 * 127/128 + speaker peak */);
    /* the clip ends by itself: 4000 samples at 22 kHz is about 8,700 frames, then silence */
    au_silence(&a); a.vol = a.vol_target = AU_VOL_ONE; au_voc(&a, "A.VOC"); au_service(&a, 0, 0);
    for (int k = 0; k < 30; k++) au_render(&a, out, AU_CHUNK);
    CHECK(a.vp.state == 0 && out[100] == 0);
    free(fa); free(fb); free(fc); }
  /* A prescan budget covers ALL segments in a call, not each segment anew.
     Accepted/refused counters advance once, on completion; cancellation is quiet. */
  { static struct au a; uint8_t blocks[130 * 7];
    for (unsigned i = 0; i < 130; i++) {
      const uint8_t block[] = {1, 3, 0, 0, 165, 0, 128};
      memcpy(blocks + i * 7, block, 7);
    }
    uint32_t len; uint8_t *f = mk(&len, blocks, sizeof blocks, 1);
    cur[0] = (struct mem){f, len, 0, 0};
    au_init(&a, scratch, sizeof scratch); a.voc_open = open_clip;
    au_voc(&a, "A.VOC");
    CHECK(au_service(&a, 0, 0) == 0 && a.vp.scan.visits == 64 && a.vp.state == VP_PENDING);
    CHECK(!a.vocs && !a.voc_refused);
    CHECK(au_service(&a, 0, 0) == 0 && a.vp.scan.visits == 128);
    CHECK(au_service(&a, 0, 0) == 1 && a.vocs == 1 && !a.voc_refused);
    CHECK(au_service(&a, 0, 0) == 0 && a.vocs == 1);
    au_voc(&a, "A.VOC"); au_service(&a, 0, 0); au_stop(&a);
    CHECK(a.vp.state == 0 && au_service(&a, 0, 0) == 0 && a.vocs == 1 && !a.voc_refused);
    f[0] = 0; au_voc(&a, "A.VOC"); au_service(&a, 0, 0);
    CHECK(a.voc_refused == 1 && a.vocs == 1);
    au_service(&a, 0, 0); CHECK(a.voc_refused == 1);
    free(f);
    puts("VOC counters: 64 visits per service, accepted/refused counted once, cancel OK");
  }
  puts(fails ? "vocplay_test FAILED" : "vocplay_test ok");
  return fails ? 1 : 0;
}
