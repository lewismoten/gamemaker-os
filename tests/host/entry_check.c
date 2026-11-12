/* SPDX-License-Identifier: MIT; the name-entry loop (entryloop.c) with a fake clock: three seconds with no keys, then with typing and
 * slow draws (90 ms a stage). The audio pump and the heartbeat must never go quiet for more than 100 ms (the ring holds ~300 ms and
 * the kernel wants a heartbeat every 2 s), and a single pass must never block for more than 20 ms. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "entryloop.h"
#include "audio.h"
static uint64_t sim, last_pump, last_health, max_pump, max_health, max_block; static unsigned draws;
static int pending_key; 
static void tick(uint64_t d) {sim += d;}
static void pump(void *u) {(void)u; if (sim - last_pump > max_pump) max_pump = sim - last_pump; last_pump = sim;}
static void health(void *u) {(void)u; if (sim - last_health > max_health) max_health = sim - last_health; last_health = sim;}
static uint64_t now(void *u) {(void)u; return sim;}
static void sleep_to(void *u, uint64_t w) {(void)u; if (w > sim) sim = w;}
static void service(void *u) {(void)u; pump(0);} /* the kernel's wait: health then pump each slice, as main.c's audio_wait_poll */
static void wait_service(void *u) {health(u); pump(u);}
static void wait_until(void *u, uint64_t wake) {
  struct au_sink sk = {u, 0, 0, now};
  uint64_t before = sim; au_wait_until(&sk, wake, wait_service, sleep_to);
  if (sim - before > max_block) max_block = sim - before;
}
static void draw(void *u, const struct hs_screen *h) {(void)u; (void)h; draws++; for (int i = 0; i < 3; i++) {tick(90000000ull); health(0); pump(0);}}
static int keys(void *u, struct hs_screen *h) {
  (void)u; int r = 0;
  if (pending_key) {r |= hs_event(h, 1, (uint16_t)pending_key, 1, sim); r |= hs_event(h, 1, (uint16_t)pending_key, 0, sim); pending_key = 0;}
  return r;
}
static void run(uint64_t span, int type_every_ms) {
  struct his_table t; memset(&t, 0, sizeof t); struct hs_screen h;
  hs_start(&h, &t, 5000, sim);
  struct el_ops o = {0, {0, health, pump, 0}, now, wait_until, service, draw, keys};
  last_pump = last_health = sim; max_pump = max_health = max_block = 0;
  uint64_t end = sim + span, next = sim + (uint64_t)type_every_ms * 1000000ull;
  for (int r = HS_DRAW; sim < end;) {
    if (type_every_ms && sim >= next) {pending_key = 30; next += (uint64_t)type_every_ms * 1000000ull;}
    r = el_pass(&h, r, &o);
  }
}
int main(void) {
  sim = 1000000000ull;
  run(3000000000ull, 0); /* no keys for 3 s: the cursor blinks, nothing else */
  printf("idle: max pump gap %llu us, heartbeat gap %llu us, one pass blocked %llu us, %u draws\n", (unsigned long long)max_pump / 1000, (unsigned long long)max_health / 1000, (unsigned long long)max_block / 1000, draws);
  assert(max_pump <= 100000000ull && max_health <= 100000000ull && max_block <= EL_WAIT_NS + 1000000ull);
  run(3000000000ull, 400); /* typing every 400 ms, each key a slow redraw */
  printf("typing: max pump gap %llu us, heartbeat gap %llu us, %u draws\n", (unsigned long long)max_pump / 1000, (unsigned long long)max_health / 1000, draws);
  assert(max_pump <= 100000000ull && max_block <= EL_WAIT_NS + 1000000ull);
  assert(max_health <= 100000000ull);
  puts("entry loop: audio and heartbeat never quiet PASS");
  return 0;
}
