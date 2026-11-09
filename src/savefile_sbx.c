/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten. Claude Sonnet 5.5.
 * savefile.h's ops over the kernel's calls. Board build only; the logic is in
 * savefile.c and is tested on the host. Not linked into GMOS.ELF yet. */
#include "savefile.h"
#include <sandbox/syscall.h>
static uint32_t plen(const char *s) {uint32_t n = 0; while (s[n]) n++; return n;}
static int s_open_write(void *c, const char *p, uint64_t *fd) {(void)c; return sbx_open(p, plen(p), SBX_O_WRONLY | SBX_O_CREAT | SBX_O_TRUNC, fd);}
static int s_write(void *c, uint64_t fd, const void *p, uint32_t n, uint32_t *done) {(void)c; uint64_t d = 0; int e = sbx_write(fd, p, n, &d); *done = (uint32_t)d; return e;}
static int s_sync(void *c, uint64_t fd) {(void)c; return sbx_fsync(fd);}
static int s_close(void *c, uint64_t fd) {(void)c; return sbx_close(fd);}
static int s_rename(void *c, const char *a, const char *b) {(void)c; return sbx_rename(a, plen(a), b, plen(b));}
static int s_open_read(void *c, const char *p, uint64_t *fd) {(void)c; return sbx_open(p, plen(p), SBX_O_RDONLY, fd);}
static int s_read(void *c, uint64_t fd, void *p, uint32_t n, uint32_t *got) {(void)c; uint64_t d = 0; int e = sbx_read(fd, p, n, &d); *got = (uint32_t)d; return e;}
const struct save_ops save_sbx_ops = {0, s_open_write, s_write, s_sync, s_close, s_rename, s_open_read, s_read};
