/* SPDX-License-Identifier: MIT; host input transport without ARM pointer truncation. */
#pragma once
#include <sandbox/abi.h>
int32_t sbx_read(uint64_t, void *, uint32_t, uint64_t *);
