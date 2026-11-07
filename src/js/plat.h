/* SPDX-License-Identifier: MIT; what the JS runtime needs from the machine it
 * runs on: the clock, files, the log, the 640x480 surface and input. The board
 * (plat_sbx.c, sandbox-kernel system calls) and the host tests
 * (tests/host/plat_host.c) each give one. */
#pragma once
#include <stdint.h>
#define PLAT_W 640u
#define PLAT_H 480u
uint64_t plat_clock_ns(void);           /* monotonic */
void plat_sleep_until(uint64_t ns);     /* returns early on input where it can */
void plat_heartbeat(void);              /* the kernel's health call; nothing on the host */
void plat_log(const char *s, uint32_t n);
/* Reads up to cap bytes of a file ("/sd/..." on the board); bytes read or -1. */
int32_t plat_read(const char *path, uint8_t *buf, uint32_t cap);
/* Lists a folder: calls fn for each entry (dir = 1 for folders); count or -1. */
int32_t plat_list(const char *path, void (*fn)(void *user, const char *name, uint32_t n, int dir, uint32_t size), void *user);
uint32_t *plat_fb(void);                /* PLAT_W x PLAT_H XRGB8888, stride PLAT_W */
void plat_present(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
/* The next input event (Linux type/code/value): 1, or 0 when none is waiting. */
int plat_event(uint16_t *type, uint16_t *code, int32_t *value);
