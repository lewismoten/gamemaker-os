/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * The kernel's heartbeat call: report health FIRST, then feed the audio ring. The kernel checks the 2 s rule at the first system
 * call after a gap, and a health call resets it even when late; the audio pump's first call is a clock read, so pumping first
 * turned a late heartbeat into "heartbeat missed 2 s" (pi2 FAULT.TXT, 2026-11-12). Claude Sonnet 5.5 wrote this file. */
#pragma once
struct bt_ops {
  void *user;
  void (*health)(void *user);      /* sbx_health(generation, ++beat) */
  void (*audio)(void *user);       /* the pump; may be 0 */
  void (*hook)(void *user);        /* the splash / picture animation; may be 0 */
};
void bt_beat(const struct bt_ops *o);
