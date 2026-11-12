/* SPDX-License-Identifier: MIT */
#include "input.h"
#include <sandbox/syscall.h>

int32_t gi_read(uint64_t fd, sbx_input_event events[GI_BATCH], uint32_t *count) {
  uint64_t bytes = 0;
  *count = 0;
  int32_t status = sbx_read(fd, events, GI_BATCH * sizeof events[0], &bytes);
  if (status) return status;
  if (!bytes) return -SBX_ENODEV;
  if (bytes > GI_BATCH * sizeof events[0] || bytes % sizeof events[0]) return -SBX_EIO;
  *count = (uint32_t)(bytes / sizeof events[0]);
  return 0;
}
void gi_reset(struct gi_keys *keys) {
  *keys = (struct gi_keys){0};
}
int gi_accept(struct gi_keys *keys, const sbx_input_event *event, uint64_t now) {
  if (event->type != 1) return 1;
  if (event->code >= SBX_INPUT_KEYS) return 0;
  uint64_t bit = 1ull << (event->code % 64);
  uint64_t *word = &keys->down[event->code / 64];
  if (event->value == 0) {*word &= ~bit; return 1;}
  if (event->value == 1) {*word |= bit; keys->repeat_ns = now; return 1;}
  if (event->value != 2 || !(*word & bit)) return 0;
  /* Only a recent repeat may move the launcher, at most ten times a second.
   * Timestamps come from the same monotonic clock as now. */
  if (event->ns > now || now - event->ns > 100000000ull ||
      now - keys->repeat_ns < 100000000ull) return 0;
  keys->repeat_ns = now;
  return 1;
}
