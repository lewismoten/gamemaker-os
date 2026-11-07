/* SPDX-License-Identifier: MIT; draws the games list into a 640x480 XRGB8888
 * surface (stride in pixels). Pure, so the host can render it to a PNG. */
#pragma once
#include <stdint.h>
#include "menu.h"
#define GM_W 640u
#define GM_H 480u
void gm_draw(uint32_t *px, uint32_t stride, const struct gm_menu *m);
/* Text helpers, also used for the key help: width in pixels at a scale. */
uint32_t gm_text_width(const char *s, uint32_t scale);
void gm_text(uint32_t *px, uint32_t stride, uint32_t x, uint32_t y, const char *s, uint32_t scale, uint32_t color);
