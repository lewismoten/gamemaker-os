/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * GMOS's own log on the card: the last GL_LINES summary lines (the 5 s frame and audio lines), kept in RAM and
 * written to /OS/SAVES/GMOS/GMOS.LOG at most every GL_EVERY_NS, from the foreground only (never an IRQ path), so a
 * hardware test can be read back with usb_kernel.py get. One bounded write (GL_BYTES at most) through savefile.h's
 * staged write. Claude Sonnet 5.5 wrote this file. */
#pragma once
#include <stdint.h>
#include "savefile.h"
#define GL_LINES 200u
#define GL_LINE_MAX 160u
#define GL_EVERY_NS 30000000000ull
#define GL_BYTES (GL_LINES * GL_LINE_MAX)
/* Adds a line (cut to GL_LINE_MAX - 1 bytes, stamped with the seconds since boot); the oldest goes when full. */
void gl_add(const char *line, uint64_t now_ns);
/* Writes the file if lines are new and GL_EVERY_NS passed since the last write (or force); returns 1 if it wrote, 0 if not due, negative errno on a failed write (retried at the next due time). */
int gl_flush(const struct save_ops *ops, uint64_t now_ns, int force);
