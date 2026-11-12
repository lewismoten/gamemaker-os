/* SPDX-License-Identifier: MIT; real SDK wrappers and sessions, fake kernel. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sandbox/syscall.h>
#include "awake.h"
#include "session.h"
#include "hostio.h"

static uint64_t held, next_token = 0x1234567800000000ull;
static unsigned acquired, released, calls;
static int32_t fail_next;
static uint64_t fake_asleep; /* what operation 2 reports */
static struct gs_session session;
static uint64_t now;

int32_t sbx_syscall(uint32_t call, void *record, uint32_t bytes) {
  calls++;
  assert(call == SBX_CALL_POWER_HOLD); /* defaults: never call 23 */
  assert(bytes == sizeof(sbx_power_hold_rec));
  sbx_power_hold_rec *r = record;
  assert(r->h.size == bytes && !r->h.flags && !r->h.reserved0 && !r->h.reserved1);
  if (fail_next) {int32_t e = fail_next; fail_next = 0; return e;}
  if (r->operation == 2) {
    assert(!r->flags && !r->token); r->flags = fake_asleep; return 0;
  }
  if (r->operation == 1) {
    assert(!held && !r->token);
    assert(r->flags == (SBX_POWER_NO_BLANK | SBX_POWER_NO_SLOWDOWN));
    r->token = held = ++next_token; acquired++;
  } else {
    assert(r->operation == 0 && !r->flags && held && r->token == held);
    held = 0; released++;
  }
  return 0;
}
static void update(void) {assert(!awake_session(&session, 0));}
static void balanced(void) {
  assert(!awake_shutdown()); assert(!held && acquired == released);
}
static void pages(void) {
  for (unsigned i = 0; i < 300 && session.phase == GS_CARDS; i++) {
    update();
    gs_event(&session, 1, 57, 1); now += 1000000000ull;
    gs_step(&session, now); update();
  }
}
static int32_t demo_read(void *u, const char *name, uint8_t *buf, uint32_t cap) {
  if (!strcmp(name, "demo.rec")) {
    const uint8_t rec[] = {3, 0, 1}; /* Esc at tick 3 */
    assert(cap >= sizeof rec); memcpy(buf, rec, sizeof rec); return sizeof rec;
  }
  return host_read(u, name, buf, cap);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  for (unsigned major = 0; major < 3; major++) {
    for (unsigned minor = 0; minor < 11; minor++) {
      awake_init(major, minor); assert(!awake_set(1)); balanced();
      fake_asleep = SBX_POWER_NO_BLANK; unsigned before = calls; assert(!awake_screen_dark()); assert(calls == before); fake_asleep = 0;
    }
  }
  awake_init(2, 11); assert(!awake_set(1)); balanced(); assert(!calls);
  puts("older/incompatible ABI: no syscalls PASS");
  awake_init(1, 11);
  /* Each card stage, including cartoons and timed multi-hour slides. */
  session.phase = GS_CARDS;
  for (int stage = GS_NEXT; stage <= GS_LEVEL_OUT; stage++) {
    session.stage = stage; session.pause = 3 * 60 * 60;
    update(); assert(held);
    unsigned count = calls;
    for (unsigned t = 0; t < 3 * 60 * 60; t++) update();
    assert(calls == count); /* no timeout, token refresh or clock truncation */
    balanced();
  }
  session.stage = GS_PAUSE; session.pause = GS_KEY_WAIT;
  update(); assert(!held);
  session.stage = GS_FLI; update(); assert(held); /* key wait follows animation */
  session.stage = GS_PAUSE; update(); assert(!held);
  puts("cartoons, timed slides, three-hour holds and key waits PASS");

  /* Real launcher: title -> menu -> intro -> play -> pause/save -> scores -> quit. */
  memset(&session, 0, sizeof session);
  assert(!gs_launch(&session, host_read, argv[1], "DICE.GAM", now));
  update(); assert(held); pages(); assert(session.phase == GS_MENU && !held);
  gs_event(&session, 1, 28, 1); pages(); update();
  assert(session.phase == GS_PLAY && held);
  gs_event(&session, 1, 1, 1); update(); assert(session.phase == GS_PAUSED && !held);
  gs_event(&session, 1, 28, 1); update(); assert(session.phase == GS_PLAY && held);
  assert(!awake_session(&session, 1)); assert(!held); /* save/load screen */
  update(); assert(held); /* success/cancel resumes */
  session.phase = GS_DONE; update(); balanced(); /* fatal load / play failure */
  gs_menu_return(&session, now); pages(); update(); assert(!held);
  for (int i = 0; i < 4; i++) gs_event(&session, 1, 108, 1);
  gs_event(&session, 1, 28, 1); update(); assert(session.phase == GS_SCORES && !held);
  gs_menu_return(&session, now); pages();
  assert(gs_event(&session, 1, 1, 1) & GS_EXIT); update(); balanced();
  puts("launcher, pause/resume, save/load overlays, scores, errors and quit PASS");

  for (int cancel = 0; cancel < 2; cancel++) {
    assert(!gs_demo_start(&session, demo_read, argv[1], "DICE.GAM", now));
    update(); assert(held);
    if (cancel) gs_demo_stop(&session);
    else for (unsigned i = 0; i < 20 && session.phase != GS_DONE; i++) {
      now = gs_wake(&session, now); gs_step(&session, now); update();
    }
    assert(session.phase == GS_DONE); update(); balanced();
  }
  assert(gs_launch(&session, host_read, argv[1], "MISSING.GAM", now));
  balanced(); /* startup failure never needs a hold */
  puts("demo end/cancel and failed game start PASS");

  fail_next = -SBX_EMFILE; assert(awake_set(1) == -SBX_EMFILE); assert(!held);
  balanced(); /* failed acquire must not release a junk token */
  assert(!awake_set(1)); assert(held); /* retry */
  fail_next = -SBX_EIO; assert(awake_set(0) == -SBX_EIO); assert(held);
  unsigned count = calls; assert(!awake_set(1)); assert(calls == count);
  balanced(); /* retry same 64-bit release token */
  assert(!awake_set(1)); fail_next = -SBX_EIO;
  assert(awake_shutdown() == -SBX_EIO); assert(held); balanced(); balanced();
  /* A stale token is already absent from the kernel; do not retry it forever. */
  assert(!awake_set(1)); held = 0; released++; fail_next = -SBX_EBADF;
  assert(awake_set(0) == -SBX_EBADF); balanced();
  puts("acquire/release failures, retries and exit cleanup PASS");
  /* ABI 1.12: the screen-dark query; a 1.11 kernel is never asked, and a failure means awake. */
  unsigned seen = calls;
  awake_init(1, 11); fake_asleep = SBX_POWER_NO_BLANK; assert(!awake_screen_dark()); assert(calls == seen);
  awake_init(2, 12); assert(!awake_screen_dark()); assert(calls == seen);
  awake_init(1, 12);
  fake_asleep = 0; assert(!awake_screen_dark());
  fake_asleep = SBX_POWER_NO_SLOWDOWN; assert(!awake_screen_dark()); /* slowed, not blanked: the music goes on */
  fake_asleep = SBX_POWER_NO_BLANK; assert(awake_screen_dark());
  fake_asleep = SBX_POWER_NO_BLANK | SBX_POWER_NO_SLOWDOWN; assert(awake_screen_dark());
  fail_next = -SBX_EINVAL; assert(!awake_screen_dark());
  fake_asleep = 0; assert(!awake_screen_dark());
  puts("screen dark query PASS");
  return 0;
}
