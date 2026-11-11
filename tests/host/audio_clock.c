#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "audio.h"
#include "session.h"

struct clock_ring {
  struct au audio;
  uint8_t music[4096];
  uint64_t time, last_drain, fraction, rendered, written, sleeps, max_sleep, deadline;
  uint32_t capacity, queued, underruns, random, max_write, pumps, fixed_cost;
  int realtime, started, failed;
};

static uint32_t random_next(struct clock_ring *ring) {
  ring->random = ring->random * 1664525u + 1013904223u;
  return ring->random;
}

static uint64_t monotonic(void) {
  struct timespec stamp;
  clock_gettime(CLOCK_MONOTONIC, &stamp);
  return (uint64_t)stamp.tv_sec * 1000000000ull + (uint64_t)stamp.tv_nsec;
}

static void advance(struct clock_ring *ring, uint64_t target) {
  if (ring->realtime) {
    uint64_t now = monotonic();
    while (now < target) {
      uint64_t left = target - now;
      struct timespec delay = {(time_t)(left / 1000000000ull), (long)(left % 1000000000ull)};
      nanosleep(&delay, 0);
      now = monotonic();
    }
    target = now;
  }
  ring->time = target;
  if (ring->started) {
    uint64_t scaled = (target - ring->last_drain) * 48000u + ring->fraction;
    uint64_t drained = scaled / 1000000000ull;
    ring->fraction = scaled % 1000000000ull;
    if (drained > ring->queued) {ring->underruns++; ring->queued = 0;}
    else ring->queued -= (uint32_t)drained;
  }
  ring->last_drain = target;
}

static uint64_t ring_now(void *user) {
  struct clock_ring *ring = user;
  uint64_t rendered = ring->audio.frames - ring->rendered;
  ring->rendered = ring->audio.frames;
  uint32_t slice_cost = ring->fixed_cost ? ring->fixed_cost : 3000000u + random_next(ring) % 2000001u;
  uint64_t cost = rendered * slice_cost / AU_CHUNK;
  if (!ring->fixed_cost && rendered && random_next(ring) % 23u == 0) cost += 5000000u;
  advance(ring, ring->time + cost);
  return ring->time;
}

static int32_t ring_query(void *user, uint32_t *queued, uint32_t *capacity) {
  struct clock_ring *ring = user;
  advance(ring, ring->time);
  *queued = ring->queued; *capacity = ring->capacity;
  return 0;
}

static int32_t ring_write(void *user, const void *pcm, uint32_t bytes) {
  struct clock_ring *ring = user;
  (void)pcm;
  advance(ring, ring->time);
  uint32_t frames = bytes / 4u;
  if (frames > ring->capacity - ring->queued || bytes % 4u) {ring->failed = 1; return -1;}
  ring->queued += frames; ring->written += frames; ring->started = 1;
  if (frames > ring->max_write) ring->max_write = frames;
  return (int32_t)bytes;
}

static void service(void *user) {
  struct clock_ring *ring = user;
  struct au_sink sink = {ring, ring_query, ring_write, ring_now};
  if (au_pump(&ring->audio, &sink) < 0) ring->failed = 1;
  ring->pumps++;
}

static void sleep_until(void *user, uint64_t deadline) {
  struct clock_ring *ring = user;
  if (deadline > ring->deadline || deadline < ring->time) ring->failed = 1;
  uint64_t delay = deadline - ring->time;
  if (delay > ring->max_sleep) ring->max_sleep = delay;
  ring->sleeps++;
  advance(ring, deadline);
}

static int exercise(uint32_t capacity, int realtime) {
  static struct clock_ring ring;
  memset(&ring, 0, sizeof ring);
  ring.capacity = capacity; ring.realtime = realtime; ring.random = 1110;
  ring.time = ring.last_drain = realtime ? monotonic() : 1;
  au_init(&ring.audio, ring.music, sizeof ring.music);
  au_set_playing(&ring.audio, 1);
  FILE *song = fopen("tests/data/syn.cmf", "rb");
  if (!song) return 1;
  uint32_t size = (uint32_t)fread(ring.music, 1, sizeof ring.music, song);
  fclose(song);
  if (au_play_loaded(&ring.audio, size)) return 1;
  for (unsigned i = 0; i < 32 && ring.queued < AU_PLAY_LOW_FRAMES; i++) service(&ring);
  uint32_t target = capacity < AU_PLAY_FILL_FRAMES ? capacity : AU_PLAY_FILL_FRAMES;
  if (ring.queued > target || ring.queued < (capacity < AU_PLAY_LOW_FRAMES ? capacity - AU_REFILL_MARGIN : AU_PLAY_LOW_FRAMES)) ring.failed = 1;
  uint64_t start = ring.time, duration = realtime ? 3000000000ull : 60000000000ull;
  uint32_t ticks = 0;
  struct au_sink sink = {&ring, ring_query, ring_write, ring_now};
  while (ring.time - start < duration && !ring.failed) {
    service(&ring);
    advance(&ring, ring.time + 9000000u + random_next(&ring) % 16000001u);
    advance(&ring, ring.time + random_next(&ring) % 6000001u);
    service(&ring);
    ticks++;
    uint64_t deadline = start + (uint64_t)ticks * GS_TICK_NS10 / 10u;
    ring.deadline = deadline;
    au_wait_until(&sink, deadline, service, sleep_until);
  }
  if (ring.underruns || ring.max_sleep > AU_SLEEP_NS || !ring.sleeps || ring.max_write != AU_CHUNK) ring.failed = 1;
  if (ring.written < duration * 48000u / 1000000000ull) ring.failed = 1;
  uint64_t expected_ticks = duration * 10u / GS_TICK_NS10;
  if (!realtime && (ticks < expected_ticks || ticks > expected_ticks + 2u)) ring.failed = 1;
  printf("%s ring %u: %u ticks, %u pumps, %u underruns, max write %u, max sleep %llu ns%s\n",
         realtime ? "realtime" : "simulated", capacity, ticks, ring.pumps, ring.underruns,
         ring.max_write, (unsigned long long)ring.max_sleep, ring.failed ? " FAIL" : "");
  return ring.failed;
}

static int overloaded(void) {
  static struct clock_ring ring;
  ring.capacity = 16384; ring.fixed_cost = 12000000;
  au_init(&ring.audio, ring.music, sizeof ring.music);
  au_set_playing(&ring.audio, 1);
  service(&ring);
  uint64_t limit = AU_BATCH;
  if (ring.failed || ring.audio.frames != limit || ring.queued >= AU_PLAY_FILL_FRAMES) {
    puts("overloaded synth did not respect refill budget");
    return 1;
  }
  return 0;
}

/* A long draw/read cannot yield internally. Probe at the low-water boundary,
   rather than only immediately after a full refill. Include synth drain. */
static int stall(int play, int old_policy) {
  static struct clock_ring ring;
  memset(&ring, 0, sizeof ring);
  ring.capacity = 16384; ring.fixed_cost = 4000000; ring.time = ring.last_drain = 1;
  au_init(&ring.audio, ring.music, sizeof ring.music);
  au_set_playing(&ring.audio, play);
  FILE *song = fopen("tests/data/syn.cmf", "rb");
  if (!song) return 1;
  uint32_t size = (uint32_t)fread(ring.music, 1, sizeof ring.music, song);
  fclose(song);
  if (au_play_loaded(&ring.audio, size)) return 1;
  for (unsigned i = 0; i < 32 && ring.queued < AU_LOW_FRAMES; i++) service(&ring);
  uint64_t frames = ring.audio.frames;
  for (unsigned repeat = 0; repeat < 20; repeat++) {
    /* The old gameplay policy starts with only 120 ms, even at its target. */
    ring.queued = old_policy ? 5760u : AU_LOW_FRAMES;
    if (!old_policy) service(&ring); /* at the threshold: a legal no-op */
    advance(&ring, ring.time + 150000000u);
    service(&ring); /* after the heavy work */
    for (unsigned i = 0; i < 40; i++) {
      service(&ring);
      advance(&ring, ring.time + 10000000u);
    }
  }
  int failed = ring.failed || (old_policy ? !ring.underruns : ring.underruns != 0);
  if (!old_policy && (ring.audio.gap_max_ns < 150000000u || ring.audio.frames <= frames)) failed = 1;
  printf("150 ms stall %s %s: %u underruns, max gap %llu us, low water %u%s\n",
         play ? "game" : "list", old_policy ? "120 ms counterexample" : "fixed",
         ring.underruns, (unsigned long long)(ring.audio.gap_max_ns / 1000u),
         ring.audio.low_frames, failed ? " FAIL" : "");
  return failed;
}

/* Gap counters must include quiet (no-write) polls and list mode; report resets
   must not erase lifetime headroom sizing. Synthetic time is exact here. */
static int counters(void) {
  static struct clock_ring ring;
  memset(&ring, 0, sizeof ring);
  ring.capacity = 16384; ring.queued = AU_FILL_FRAMES;
  ring.time = ring.last_drain = 1;
  au_init(&ring.audio, ring.music, sizeof ring.music);
  service(&ring);
  advance(&ring, ring.time + 280000000u);
  service(&ring);
  if (ring.audio.gap_max_ns != 280000000u || ring.audio.gap_peak_ns != 280000000u ||
      ring.audio.low_frames != 14400u || ring.audio.frames || ring.audio.writes) return 1;
  ring.audio.gap_max_ns = 0; /* audio_log resets the interval */
  au_set_playing(&ring.audio, 1);
  advance(&ring, ring.time + 10000000u);
  service(&ring);
  if (ring.audio.gap_max_ns != 10000000u || ring.audio.gap_peak_ns != 280000000u ||
      ring.audio.low_frames != 14400u || ring.audio.total_ns || ring.audio.eagain) return 1;
  puts("counters: list/game gaps, interval reset, lifetime low water 14400, quiet polls OK");
  return 0;
}

int main(void) {
  int failed = overloaded();
  failed |= counters();
  failed |= stall(1, 1);
  failed |= stall(1, 0);
  failed |= stall(0, 0);
  failed |= exercise(16384, 0);
  failed |= exercise(4096, 0);
  failed |= exercise(16384, 1);
  failed |= exercise(4096, 1);
  return failed;
}
