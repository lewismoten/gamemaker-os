/* SPDX-License-Identifier: MIT; the host tests' file reader for game.h: a
 * game folder on disk, its names tried as the .GAM spells them, then in lower
 * and upper case (DOS names are case-blind; the card's are upper case). */
#pragma once
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
static int32_t host_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {
  char path[1024];
  for (int pass = 0; pass < 3; pass++) {
    int n = snprintf(path, sizeof path, "%s/", (const char *)user);
    for (const char *s = name; *s && n < (int)sizeof path - 1; s++)
      path[n++] = (char)(pass == 1 ? tolower((unsigned char)*s) : pass == 2 ? toupper((unsigned char)*s) : *s);
    path[n] = 0;
    FILE *f = fopen(path, "rb");
    if (!f) continue;
    size_t got = fread(buf, 1, cap, f);
    fclose(f);
    return (int32_t)got;
  }
  return -1;
}
/* A .GMP on disk for gmp.h: path is NAME.GMP; NAME.G01, NAME.G02 ... beside it
   are its further volumes while they exist (any case). */
#include "gmp.h"
struct host_gmp {FILE *f[GMP_VOLUMES_MAX]; struct gmp g;};
static int host_gmp_at(void *user, unsigned v, uint32_t off, void *buf, uint32_t n) {
  FILE *f = ((struct host_gmp *)user)->f[v];
  return !f || fseek(f, (long)off, SEEK_SET) || fread(buf, 1, n, f) != n ? -1 : 0;
}
static void host_gmp_close(struct host_gmp *h) {
  for (unsigned v = 0; v < GMP_VOLUMES_MAX; v++) if (h->f[v]) {fclose(h->f[v]); h->f[v] = 0;}
}
static int host_gmp_open(struct host_gmp *h, const char *path) {
  memset(h, 0, sizeof *h);
  size_t n = strlen(path);
  if (n < 4 || n > 1000) return -1;
  for (unsigned v = 0; v < GMP_VOLUMES_MAX; v++) {
    char p[1024]; memcpy(p, path, n + 1);
    if (v) {p[n - 2] = (char)('0' + v / 10); p[n - 1] = (char)('0' + v % 10); p[n - 3] = isupper((unsigned char)p[n - 3]) ? 'G' : 'g';}
    FILE *f = fopen(p, "rb");
    if (!f) break;
    if (fseek(f, 0, SEEK_END)) {fclose(f); break;}
    long sz = ftell(f);
    if (sz < 0 || sz > 0x7fffffffL) {fclose(f); break;}
    h->f[v] = f; h->g.size[v] = (uint32_t)sz; h->g.volumes = v + 1;
  }
  h->g.read_at = host_gmp_at; h->g.user = h;
  if (gmp_open(&h->g)) {host_gmp_close(h); return -1;}
  return 0;
}
