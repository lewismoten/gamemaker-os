/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten. Health first, then audio, hook, audio (see beat.h). */
#include "beat.h"
void bt_beat(const struct bt_ops *o) {
  o->health(o->user);
  if (o->audio) o->audio(o->user);
  if (o->hook) o->hook(o->user);
  if (o->audio) o->audio(o->user);
}
