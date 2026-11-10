/* SPDX-License-Identifier: MIT; fake /dev/eink for the production panel client. */
#include <assert.h>
#include <string.h>
#include <sandbox/syscall.h>
#include "eink.h"
/* Exports called by the Python ctypes tests. */
void fake_reset(void);
void fake_time(uint64_t ns);
void fake_error(int e);
void fake_open_error(int e);
void fake_clock_error(int e);
int fake_count(int which);
uint64_t fake_at(int i);
const char *fake_line(void);
const uint8_t *fake_page(void);
const uint8_t *fake_mark(void);
static uint64_t now, times[1024];
static int opens, closes, updates, pages, titles, clears, page_shown, error, open_error, clock_error;
static uint8_t page[SBX_EINK_PAGE_BYTES], mark[SBX_EINK_MARK_BYTES];
static char line[17];
void fake_reset(void) {
  now = 0; opens = closes = updates = pages = titles = clears = page_shown = error = open_error = clock_error = 0;
  memset(page, 0, sizeof page); memset(mark, 0, sizeof mark); memset(line, 0, sizeof line);
}
void fake_time(uint64_t ns) {now = ns;}
void fake_error(int e) {error = e;}
void fake_open_error(int e) {open_error = e;}
void fake_clock_error(int e) {clock_error = e;}
int fake_count(int which) {int v[] = {opens, closes, updates, pages, titles, clears, page_shown}; return v[which];}
uint64_t fake_at(int i) {return times[i];}
const char *fake_line(void) {return line;}
const uint8_t *fake_page(void) {return page;}
const uint8_t *fake_mark(void) {return mark;}
int32_t sbx_open(const char *path, uint32_t bytes, uint32_t flags, uint64_t *fd) {
  assert(bytes == 9 && !memcmp(path, "/dev/eink", 9) && flags == SBX_O_WRONLY);
  opens++; if (open_error) return open_error; *fd = 42; return 0;
}
int32_t sbx_close(uint64_t fd) {assert(fd == 42); closes++; return 0;}
int32_t sbx_clock_ns(uint64_t *ns) {if (clock_error) return clock_error; *ns = now; return 0;}
static int attempt(uint64_t fd) {assert(fd == 42 && updates < 1024); times[updates++] = now; return error;}
int32_t sbx_eink_page(uint64_t fd, const uint8_t *bits) {
  pages++; memcpy(page, bits, sizeof page); int e = attempt(fd); if (!e) page_shown = 1; return e;
}
int32_t sbx_eink_title(uint64_t fd, sbx_eink_title_rec *r, const char *name, uint32_t bytes, const uint8_t *m, const char *build) {
  (void)r; assert(bytes == 13 && !memcmp(name, "GAME-MAKER OS", 13) && m && strlen(build) <= 16);
  titles++; memcpy(mark, m, sizeof mark); strcpy(line, build); return attempt(fd);
}

int32_t sbx_eink_clear(uint64_t fd) {
  clears++; int e = attempt(fd); if (!e) page_shown = 0; return e;
}
