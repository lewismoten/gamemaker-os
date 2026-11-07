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
