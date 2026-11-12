/* SPDX-License-Identifier: MIT; per-card thumbnail cost on the host: reads each game folder given
 * (or every folder under a root), times the title-screen decode and the shrink to a card. The
 * kernel's idle loop draws one card per pass, so one card's cost is the longest the input and
 * music loops wait on it. Usage: thumbcost <folder-of-game-folders> [/subfolder, e.g. /disk]. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include "hostio.h"
#include "thumb.h"
#include "menu.h"
#include "view.h"
static double now(void) {struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6;}
static uint8_t px[64000], pal[768];
static uint16_t out[GM_THUMB_W * GM_THUMB_H], frame[GM_W * GM_H];
static struct gm_menu menu;
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  DIR *d = opendir(argv[1]); struct dirent *e; double dec = 0, shr = 0, worst = 0; int n = 0;
  while (d && (e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char dir[1024], gam[256] = ""; snprintf(dir, sizeof dir, "%s/%s%s", argv[1], e->d_name, argc > 2 ? argv[2] : "");
    DIR *g = opendir(dir); struct dirent *f; if (!g) continue;
    while ((f = readdir(g))) {size_t l = strlen(f->d_name); if (l > 4 && !strcasecmp(f->d_name + l - 4, ".gam")) {snprintf(gam, sizeof gam, "%s", f->d_name); break;}}
    closedir(g); if (!gam[0]) continue;
    double t0 = now(); int r = gm_title_picture(host_read, dir, gam, px, pal); double t1 = now();
    if (r) continue;
    gm_shrink(px, pal, out, GM_THUMB_W, GM_THUMB_H); double t2 = now();
    if (menu.count < GM_PAGE) {gm_add(&menu, e->d_name, gam, e->d_name, 0); menu.games[menu.count - 1].pic = 1; gm_thumb_put(menu.count - 1, px, pal);}
    dec += t1 - t0; shr += t2 - t1; n++; if (t2 - t0 > worst) worst = t2 - t0;
  }
  if (d) closedir(d);
  printf("cards %d  decode %.2f ms/card  shrink %.2f ms/card  worst %.2f ms\n", n, n ? dec / n : 0, n ? shr / n : 0, worst);
  gm_finish(&menu);
  double t0 = now(); for (int i = 0; i < 20; i++) gm_draw(frame, GM_W, &menu, 0, 0);
  printf("page redraw (%u cards drawn) %.2f ms\n", menu.count, (now() - t0) / 20);
  return 0;
}
