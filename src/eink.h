/* SPDX-License-Identifier: MIT; optional ABI 1.9 panel, pumped by the normal heartbeat.
 * No sleeps or retry loops. Each attempted change is spaced by 2.1 s (<=29/min),
 * including EAGAIN retries; unchanged status generates no device traffic. */
#pragma once
#include <stdint.h>
void ge_init(unsigned major, unsigned minor);
void ge_library(unsigned games);
void ge_play(const char *title);
void ge_saving(int saving);
void ge_poll(void);
