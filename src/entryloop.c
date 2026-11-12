/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten. See entryloop.h. */
#include "entryloop.h"
int el_pass(struct hs_screen *h, int r, const struct el_ops *o) {
  bt_beat(&o->beat);
  if (o->service) o->service(o->user);
  if (r & HS_DRAW) o->draw(o->user, h);
  if (r & HS_DONE) return r;
  uint64_t now = o->now(o->user), wake = hs_wake(h, now);
  if (wake > now + EL_WAIT_NS) wake = now + EL_WAIT_NS;
  o->wait_until(o->user, wake);
  r = o->keys(o->user, h);
  return r | hs_step(h, o->now(o->user));
}
