/* SPDX-License-Identifier: MIT; bounded input reads and launcher repeat policy. */
#pragma once
#include <sandbox/abi.h>

#define GI_BATCH 16u
#define GI_DRAIN 4u /* yield even if a producer keeps the queue full */
struct gi_keys {
  uint64_t down[SBX_INPUT_KEYS / 64];
  uint64_t repeat_ns;
};
/* count is always initialized; EOF and malformed byte counts are errors. */
int32_t gi_read(uint64_t fd, sbx_input_event events[GI_BATCH], uint32_t *count);
void gi_reset(struct gi_keys *keys);
/* Preserve presses/releases; reject orphan, stale and burst repeats. */
int gi_accept(struct gi_keys *keys, const sbx_input_event *event, uint64_t now);
