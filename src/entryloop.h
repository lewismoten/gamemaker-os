/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten. Claude Sonnet 5.5 wrote this file.
 * One pass of a text-entry screen's loop (the Top Ten name entry): health first, then the audio pump, then a draw if one is due, then a
 * wait of at most EL_WAIT_NS that feeds both again every 10 ms, then the keys. A screen that waits for keys must never go quiet: the
 * audio ring drains in about 300 ms (the HDMI side repeats or garbles on a dry ring) and the kernel wants a heartbeat every 2 s. */
#pragma once
#include <stdint.h>
#include "hiscreen.h"
#include "beat.h"
#define EL_WAIT_NS 20000000ull /* the longest one pass blocks waiting for a key */
struct el_ops {
  void *user;
  struct bt_ops beat;                            /* health, audio pump, picture hook */
  uint64_t (*now)(void *user);
  void (*wait_until)(void *user, uint64_t wake); /* feeds health and audio while it waits; returns by wake */
  void (*service)(void *user);                   /* the audio's file reads (may be 0) */
  void (*draw)(void *user, const struct hs_screen *h); /* draw, pump between stages, present */
  int (*keys)(void *user, struct hs_screen *h);  /* read and apply the pending keys; the HS_ flags */
};
/* `r` is what the last pass returned. Returns the flags to pass back in; HS_DONE ends the screen. */
int el_pass(struct hs_screen *h, int r, const struct el_ops *o);
