/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * Host test of gmoslog.c: the last 200 lines, in order, one bounded write at most every 30 s. */
#include <stdio.h>
#include <string.h>
#include "gmoslog.h"
static int fails, opens, last_path_ok; static uint8_t file[GL_BYTES + 64]; static uint32_t file_n, stage_n;
#define CHECK(c, ...) do {if (!(c)) {fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n");}} while (0)
static int o_w(void *c, const char *p, uint64_t *fd) {(void)c; (void)fd; opens++; last_path_ok = !strcmp(p, "/sd/OS/SAVES/GMOS/GMOS.TMP"); stage_n = 0; return 0;}
static int w_w(void *c, uint64_t fd, const void *p, uint32_t n, uint32_t *d) {(void)c; (void)fd; memcpy(file + stage_n, p, n); stage_n += n; *d = n; return 0;}
static int s_s(void *c, uint64_t fd) {(void)c; (void)fd; return 0;}
static int c_c(void *c, uint64_t fd) {(void)c; (void)fd; return 0;}
static int r_r(void *c, const char *f, const char *t) {(void)c; (void)f; last_path_ok &= !strcmp(t, "/sd/OS/SAVES/GMOS/GMOS.LOG"); file_n = stage_n; return 0;}
static int o_r(void *c, const char *p, uint64_t *fd) {(void)c; (void)p; (void)fd; return -2;}
static int r_d(void *c, uint64_t fd, void *p, uint32_t n, uint32_t *g) {(void)c; (void)fd; (void)p; (void)n; *g = 0; return 0;}
int main(void) {
  struct save_ops o = {0, o_w, w_w, s_s, c_c, r_r, o_r, r_d}; char l[64];
  gl_add("first", 1000000000ull);
  CHECK(gl_flush(&o, 5000000000ull, 0) == 0 && !opens, "wrote before 30 s");
  CHECK(gl_flush(&o, 31000000000ull, 0) == 1 && opens == 1 && last_path_ok, "no write when due");
  CHECK(file_n == strlen("[1s] first\n") && !memcmp(file, "[1s] first\n", file_n), "file text");
  CHECK(gl_flush(&o, 99000000000ull, 0) == 0 && opens == 1, "wrote with nothing new");
  for (unsigned i = 0; i < 300; i++) {snprintf(l, sizeof l, "line %u", i); gl_add(l, 40000000000ull + i * 1000000000ull);}
  CHECK(gl_flush(&o, 45000000000ull, 0) == 0, "wrote inside the 30 s");
  CHECK(gl_flush(&o, 62000000000ull, 0) == 1, "not written when due again");
  unsigned nl = 0; for (uint32_t i = 0; i < file_n; i++) nl += file[i] == '\n';
  CHECK(nl == GL_LINES && file_n <= GL_BYTES, "%u lines, %u bytes", nl, file_n);
  CHECK(!memcmp(file, "[140s] line 100\n", 16), "oldest kept line wrong: %.20s", file);
  char long_line[400]; memset(long_line, 'x', 399); long_line[399] = 0; gl_add(long_line, 70000000000ull);
  CHECK(gl_flush(&o, 70000000000ull, 1) == 1 && file_n <= GL_BYTES, "forced flush or a long line broke the bound");
  if (!fails) puts("gmoslog ok");
  return fails != 0;
}
