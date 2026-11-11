/* SPDX-License-Identifier: MIT; real input policy/menu, fake USB event stream. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sandbox/syscall.h>
#include "input.h"
#include "menu.h"
#include "thumb.h"
#include "hostio.h"

static sbx_input_event queue[128];
static unsigned queued, at, reads;
static int32_t failure;
static uint64_t bad_bytes;
static uint64_t device = 1, now = 1000000000ull;
static struct gm_menu menu;
static struct gi_keys keys;

int32_t sbx_read(uint64_t fd, void *out, uint32_t cap, uint64_t *bytes) {
  reads++;
  assert(cap == GI_BATCH * sizeof queue[0]);
  if (fd != device) return -SBX_ENODEV;
  if (failure) return failure; /* like the SDK: leave bytes untouched on errors */
  if (bad_bytes) {*bytes = bad_bytes; return 0;}
  unsigned n = queued - at;
  if (n > GI_BATCH) n = GI_BATCH;
  memcpy(out, queue + at, n * sizeof queue[0]); at += n;
  *bytes = n * sizeof queue[0];
  return 0;
}
static void push(uint16_t code, int32_t value) {
  assert(queued < sizeof queue / sizeof queue[0]);
  queue[queued++] = (sbx_input_event){now, 1, code, value};
}
static void clear(void) {queued = at = 0; failure = 0; bad_bytes = 0;}
static void start(void) {
  clear(); gi_reset(&keys); memset(&menu, 0, sizeof menu);
  for (unsigned i = 0; i < 12; i++) {
    char dir[9]; snprintf(dir, sizeof dir, "GAME%02u", i);
    assert(gm_add(&menu, dir, "GAME.GAM", dir, 0));
  }
  gm_finish(&menu);
}
static unsigned poll(uint64_t fd) {
  sbx_input_event events[GI_BATCH]; uint32_t count = 999;
  int32_t status = gi_read(fd, events, &count);
  if (status) {assert(!count); return 0;}
  assert(count <= GI_BATCH);
  unsigned accepted = 0;
  for (unsigned i = 0; i < count; i++) {
    if (gi_accept(&keys, &events[i], now)) {
      int action = gm_event(&menu, events[i].type, events[i].code, events[i].value);
      assert(action != GM_CHOSEN); /* stale Enter repeats must never start play */
      accepted++;
    }
  }
  return accepted;
}
static unsigned decode_reads;
static int unplug_during_decode;
static int32_t decode_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {
  assert(menu.view == GM_DETAILS && menu.button == GM_BTN_PLAY);
  if (!decode_reads++) {
    push(28, 2); push(106, 1);
    for (unsigned i = 0; i < 60; i++) push(106, 2);
    if (unplug_during_decode) device++;
    now += 2000000000ull;
  }
  return host_read(user, name, buf, cap);
}
static void decode(void *folder, int unplug) {
  static uint8_t picture[GP_W * GP_H], palette[768];
  decode_reads = 0; unplug_during_decode = unplug;
  assert(!gm_title_picture(decode_read, folder, "DICE.GAM", picture, palette));
  assert(decode_reads > 1); /* real file loading and picture decode */
}
int main(int argc, char **argv) {
  assert(argc == 2);
  sbx_input_event events[GI_BATCH]; uint32_t count;
  start();
  push(106, 1); poll(device);
  unsigned pos = menu.pos[menu.tab];
  clear(); now += 200000000ull;
  for (unsigned i = 0; i < 60; i++) push(106, 2);
  unsigned accepted = 0;
  for (unsigned i = 0; i < GI_DRAIN; i++) accepted += poll(device);
  assert(accepted == 1 && at == 60 && menu.pos[menu.tab] == pos + 1);
  clear(); now += 200000000ull;
  for (unsigned i = 0; i < 60; i++) push(106, 2);
  now += 2000000000ull; /* a slow page drew while USB queued repeats */
  for (unsigned i = 0; i < GI_DRAIN; i++) assert(!poll(device));
  assert(menu.pos[menu.tab] == pos + 1);
  puts("60 fresh/stale queued repeats PASS");

  start(); push(28, 1); poll(device); assert(menu.view == GM_DETAILS);
  /* Decode runs outside input dispatch: keys queue without touching the menu.
   * Hub removal makes the old descriptor unusable; the new open is a new epoch. */
  clear(); uint64_t old = device; decode(argv[1], 1);
  assert(menu.view == GM_DETAILS && menu.button == GM_BTN_PLAY);
  gi_reset(&keys); menu.shift = 0;
  assert(gi_read(old, events, &count) == -SBX_ENODEV && !count);
  clear(); /* state_get on attachment empties the old device's queue */
  push(28, 0); push(28, 2); push(106, 1); poll(device);
  assert(menu.view == GM_DETAILS && menu.button == GM_BTN_BACK);
  clear(); menu.button = GM_BTN_PLAY; decode(argv[1], 0);
  unsigned arrived = 0;
  for (unsigned i = 0; i < GI_DRAIN; i++) arrived += poll(device);
  assert(arrived == 1 && menu.button == GM_BTN_BACK); /* keep press, drop stale repeats */
  puts("hotplug mid-load and keys during details decode PASS");

  start(); push(28, 0); push(106, 0); poll(device);
  assert(menu.view == GM_GRID && !menu.pos[menu.tab]);
  push(106, 2); assert(!poll(device));
  puts("release without press and orphan repeat PASS");

  clear(); reads = 0;
  assert(gi_read(device, events, &count) == -SBX_ENODEV && !count && reads == 1);
  failure = -SBX_EAGAIN;
  assert(gi_read(device, events, &count) == -SBX_EAGAIN && !count);
  failure = -SBX_EBADF;
  assert(gi_read(device, events, &count) == -SBX_EBADF && !count);
  failure = 0;
  const uint64_t invalid[] = {1, sizeof events - 1, sizeof events + 16, UINT64_MAX};
  for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
    bad_bytes = invalid[i];
    assert(gi_read(device, events, &count) == -SBX_EIO && !count);
  }
  sbx_input_event out_of_range = {now, 1, UINT16_MAX, 2};
  assert(!gi_accept(&keys, &out_of_range, now));
  puts("empty/closed queue, errors and invalid byte counts PASS");
  return 0;
}
