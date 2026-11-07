/* SPDX-License-Identifier: MIT; draws the games list into a 960x540 RGB565
 * surface (stride in pixels; 1920x1080 halved, so it fills a 16:9 screen), after the Sandbox Edition web site's Games page
 * in its dark theme: the navy header, the category tabs with their counts,
 * the tab's line, a grid of game cards (title screen, title, company) with the
 * selected one outlined, and the details view (the title screen, category,
 * title, company and year, the blurb, Play and Back). Pure, so the host can
 * render it to a PNG. */
#pragma once
#include <stdint.h>
#include "menu.h"
#define GM_W 960u
#define GM_H 540u
/* big and big_pal: the selected game's title screen for the details view
   (320 x 200 indices and a 6-bit palette), or 0 when it has none. */
void gm_draw(uint16_t *px, uint32_t stride, const struct gm_menu *m, const uint8_t *big, const uint8_t *big_pal);
/* Where card i of a page sits (for the host tests). */
void gm_card_rect(unsigned i, uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);
