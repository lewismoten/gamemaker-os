/* SPDX-License-Identifier: MIT; host transport, using the real vendored ABI.
 * As with sdpack's host transport, no ARM pointer truncation on a 64-bit host. */
#pragma once
#include <sandbox/abi.h>
int32_t sbx_open(const char *, uint32_t, uint32_t, uint64_t *);
int32_t sbx_close(uint64_t);
int32_t sbx_clock_ns(uint64_t *);
int32_t sbx_eink_page(uint64_t, const uint8_t *);
int32_t sbx_eink_title(uint64_t, sbx_eink_title_rec *, const char *, uint32_t, const uint8_t *, const char *);
int32_t sbx_eink_clear(uint64_t);
