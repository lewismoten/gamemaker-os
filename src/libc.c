/* SPDX-License-Identifier: MIT; the C library functions the play core calls
 * that a freestanding build does not have (the SDK's mem.c has memset and memcpy). */
#include <stddef.h>
int abs(int v) {return v < 0 ? -v : v;}
/* The .GMP index (gmsmall.h) compares and measures entry names. */
size_t strlen(const char *s) {size_t n = 0; while (s[n]) n++; return n;}
char *strchr(const char *s, int c) { /* savefile.c */
  for (;; s++) {if (*s == (char)c) return (char *)s; if (!*s) return 0;}
}
int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {a++; b++;}
  return (unsigned char)*a - (unsigned char)*b;
}
int memcmp(const void *a, const void *b, size_t n) {
  const unsigned char *x = a, *y = b;
  for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return x[i] - y[i];
  return 0;
}
