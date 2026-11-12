/* Host test: no audio wait holds the main loop. A fake device that is full (writes answer EAGAIN) and a VOC-heavy scene
 * (a clip started every frame, as when the car drives off in a game with many clips) never hold one frame longer than 100 ms
 * of simulated time; au_wait_until is capped at AU_WAIT_MAX_NS whatever deadline it is given, and a sleep that returns at once
 * cannot spin it. Exits 1 on the first failure. */
#include "audio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int fails, flap;
#define CHECK(c) do {if (!(c)) {printf("FAIL line %d: %s\n", __LINE__, #c); fails++;}} while (0)
struct sim {struct au *a; uint64_t time, rendered; uint32_t queued, cap, preads, writes; int frozen_sleep, full;};
static uint64_t s_now(void *u) { /* each call costs a little; synthesis costs 4 ms per 10 ms slice */
  struct sim *s = u; uint64_t r = s->a->frames - s->rendered; s->rendered = s->a->frames;
  s->time += 2000 + r * 4000000ull / AU_CHUNK; return s->time;
}
static int32_t s_query(void *u, uint32_t *q, uint32_t *c) {struct sim *s = u; if (flap) s->time += 1000000; *q = s->queued; *c = s->cap; return 0;}
static int32_t s_write(void *u, const void *p, uint32_t n) {
  struct sim *s = u; (void)p; s->writes++;
  if (flap) {s->time += 1000000; uint64_t ph = s->time / 1000000 % 600; if (ph < 300) {s->queued = 0; return -11;} if (n > 8) n = 8; s->queued += n / 4u; return (int32_t)n;}
  if (s->full) return -11;
  if (n > (s->cap - s->queued) * 4u) n = (s->cap - s->queued) * 4u;
  s->queued += n / 4u; return n ? (int32_t)n : -11;
}
static void s_sleep(void *u, uint64_t t) {struct sim *s = u; if (!s->frozen_sleep && t > s->time) s->time = t;}
static unsigned services, beats; static int flap;
static void s_beat_service(void *u) {(void)u; beats++; services++;}
static void s_service(void *u) {(void)u; services++;}
/* A clip made of many tiny blocks: every sample is a block of its own, so one source sample costs a block read. */
static uint8_t clip[26 + 7 + 4000 * 8 + 1]; static uint32_t clip_n;
static int32_t pread_clip(void *u, uint32_t off, void *buf, uint32_t n) {
  struct sim *s = u; s->preads++; if (off >= clip_n) return 0; if (n > clip_n - off) n = clip_n - off; memcpy(buf, clip + off, n); return (int32_t)n;
}
static struct sim *cur;
static int open_clip(const char *name, struct vp_io *io) {(void)name; io->user = cur; io->pread = pread_clip; io->close = 0; io->size = clip_n; return 0;}
static void build_clip(void) {
  memcpy(clip, "Creative Voice File\032\032\0\012\001\051\021", 26); clip[20] = 26; clip[21] = 0; uint32_t p = 26;
  clip[p++] = 1; clip[p++] = 3; clip[p++] = 0; clip[p++] = 0; clip[p++] = 0xdd; clip[p++] = 0; clip[p++] = 160; /* one real sample */
  for (int i = 0; i < 4000; i++) {clip[p++] = 4; clip[p++] = 2; clip[p++] = 0; clip[p++] = 0; clip[p++] = 1; clip[p++] = 0; clip[p++] = 1; clip[p++] = 0;} /* empty marker blocks */
  clip[p++] = 0; clip_n = p;
}
int main(void) {
  static struct au a; static uint8_t music[256]; static struct sim s; build_clip();
  au_init(&a, music, sizeof music); a.voc_open = open_clip; s.a = &a; s.cap = 16384; cur = &s;
  struct au_sink sink = {&s, s_query, s_write, s_now};
  /* 1. A far deadline is cut to AU_WAIT_MAX_NS. */
  { uint64_t t0 = s.time; au_wait_until(&sink, t0 + 10000000000ull, s_service, s_sleep);
    CHECK(s.time - t0 <= AU_WAIT_MAX_NS + AU_SLEEP_NS); }
  /* 2. A sleep that returns at once cannot spin the wait. */
  { s.frozen_sleep = 1; services = 0; au_wait_until(&sink, s.time + 10000000000ull, s_service, s_sleep); s.frozen_sleep = 0;
    CHECK(services <= AU_WAIT_SPINS); }
  /* 3. A full device and a clip every frame: no frame (service, pump, a wait of up to 10 ms) holds longer than 100 ms. */
  uint64_t worst = 0;
  for (int mode = 0; mode < 2; mode++) {
    s.full = 1; s.queued = mode ? 0 : s.cap - 200; /* a ring that is empty and refuses writes, then one that is nearly full */
    for (int f = 0; f < 300; f++) {
      uint64_t t0 = s.time; au_voc(&a, "A"); au_service(&a, 0, 0); au_pump(&a, &sink);
      au_wait_until(&sink, s_now(&s) + 10000000ull, s_service, s_sleep);
      if (s.time - t0 > worst) worst = s.time - t0;
    }
  }
  printf("worst frame %llu us, %u block reads\n", (unsigned long long)(worst / 1000), s.preads);
  CHECK(worst <= 100000000ull);
  CHECK(a.vocs > 0);
  /* 4. A flapping device during a page with music: it accepts nothing for 300 ms, then takes a frame or two at a time (each
   * syscall costs 1 ms), over and over, and the ring is dropped when it reopens. The loop (service, pump, wait, a beat per round)
   * never holds one frame past 100 ms and the beat never lapses past 500 ms of simulated time. */
  { flap = 1; s.full = 0; s.queued = 0; a.playing = 0; au_music(&a, "M"); a.cmf.playing = 0;
    uint64_t last_beat = s.time, gap = 0, fworst = 0; uint64_t t_end = s.time + 20000000000ull; beats = 0;
    while (s.time < t_end) {
      uint64_t t0 = s.time; au_voc(&a, "A"); au_service(&a, 0, 0); au_pump(&a, &sink);
      au_wait_until(&sink, s_now(&s) + 10000000ull, s_beat_service, s_sleep);
      if (s.time - t0 > fworst) fworst = s.time - t0;
      if (beats) {if (s.time - last_beat > gap) gap = s.time - last_beat; last_beat = s.time; beats = 0;}
    }
    printf("flapping: worst frame %llu us, worst beat gap %llu us\n", (unsigned long long)(fworst / 1000), (unsigned long long)(gap / 1000));
    CHECK(fworst <= 100000000ull); CHECK(gap <= 500000000ull); }
  puts(fails ? "audio_wait FAILED" : "audio_wait ok"); return fails != 0;
}
