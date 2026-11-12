/* SPDX-License-Identifier: MIT; the kernel's 2 s heartbeat rule against bt_beat with a fake clock. The kernel (core/os.c overdue)
 * checks on each system call: a health call resets the timer first, any other call after a 2 s gap is a fault. A 2.5 s stretch
 * of computing (a slow inflate) ends in a heartbeat: health first survives it, the old order (pump, whose first call is the
 * clock) faulted. */
#include <assert.h>
#include <stdio.h>
#include "beat.h"
static unsigned long long sim, health_at; static int fault;
static void syscall_other(void) {if (sim - health_at >= 2000000000ull) fault = 1;}
static void health(void *u) {(void)u; health_at = sim;}               /* sys_healths changes: the timer resets */
static void audio(void *u) {(void)u; syscall_other(); sim += 1000000;} /* au_pump: clock read first */
static void hook(void *u) {(void)u; syscall_other();}
static void old_order(const struct bt_ops *o) {o->audio(o->user); o->health(o->user); o->hook(o->user); o->audio(o->user);}
int main(void) {
  struct bt_ops o = {0, health, audio, hook};
  sim = 10000000000ull; health_at = sim;
  sim += 2500000000ull; old_order(&o);        /* the bug: a late heartbeat in the old order */
  assert(fault); fault = 0;
  sim = 10000000000ull; health_at = sim;
  sim += 2500000000ull; bt_beat(&o);          /* the fix: the same gap survives */
  assert(!fault && health_at >= 12500000000ull);
  sim += 1900000000ull; bt_beat(&o); assert(!fault);
  o.audio = 0; o.hook = 0; sim += 1000; bt_beat(&o); assert(!fault);
  puts("beat: health first, a late heartbeat survives PASS");
  return 0;
}
