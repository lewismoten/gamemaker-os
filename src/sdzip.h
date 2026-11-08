/* SPDX-License-Identifier: MIT */
#pragma once
#include "zip.h"
struct gz_zip *sz_open(const char *, uint32_t, unsigned);
void sz_close(void);
extern uint32_t sz_io_errors;
extern void (*sz_beat)(void);
