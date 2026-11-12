/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten; see gmoslog.h. */
#include "gmoslog.h"
static char lines[GL_LINES][GL_LINE_MAX];
static unsigned head, count;     /* head: the next slot; count: lines held */
static uint64_t last_ns; static int dirty, tried;
static uint8_t out[GL_BYTES];
static char *num(char *p, uint64_t v) {char t[20]; unsigned n = 0; do {t[n++] = (char)('0' + v % 10u); v /= 10u;} while (v); while (n) *p++ = t[--n]; return p;}
void gl_add(const char *line, uint64_t now_ns) {
  char *p = lines[head]; *p++ = '['; p = num(p, now_ns / 1000000000ull); *p++ = 's'; *p++ = ']'; *p++ = ' ';
  while (*line && p < lines[head] + GL_LINE_MAX - 1) *p++ = *line++;
  *p = 0; head = (head + 1) % GL_LINES; if (count < GL_LINES) count++; dirty = 1;
}
int gl_flush(const struct save_ops *ops, uint64_t now_ns, int force) {
  if (!dirty || (!force && tried && now_ns - last_ns < GL_EVERY_NS)) return 0;
  if (!force && !tried && now_ns < GL_EVERY_NS) return 0;
  last_ns = now_ns; tried = 1; /* a failed write waits the full interval too, the first one included */
  uint32_t n = 0; unsigned first = (head + GL_LINES - count) % GL_LINES;
  for (unsigned i = 0; i < count; i++) {
    const char *s = lines[(first + i) % GL_LINES];
    while (*s) out[n++] = (uint8_t)*s++;
    out[n++] = '\n';
  }
  int e = save_put(ops, "GMOS", "GMOS.LOG", out, n);
  if (e) return e;
  dirty = 0; return 1;
}
