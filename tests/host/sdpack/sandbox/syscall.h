/* SPDX-License-Identifier: MIT; host file transport, real vendored ABI records. */
#pragma once
#include <sandbox/abi.h>
int32_t sbx_open(const char *, uint32_t, uint32_t, uint64_t *);
int32_t sbx_read(uint64_t, void *, uint32_t, uint64_t *);
int32_t sbx_close(uint64_t);
int32_t sbx_getdents(uint64_t, void *, uint32_t, uint64_t *);
int32_t sbx_seek(uint64_t, int64_t, uint64_t *);
