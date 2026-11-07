/* SPDX-License-Identifier: MIT; the C library functions the play core calls
 * that a freestanding build does not have (the SDK's mem.c has memset and memcpy). */
int abs(int v) {return v < 0 ? -v : v;}
