/* SPDX-License-Identifier: MIT; GIF pictures for the cards between scenes
 * (title, level intro and exit, won, lost), as PLAYGAME's GIFC.C draws them:
 * the first image only, at (0,0), palette reduced to 6-bit VGA values. */
#pragma once
#include <stdint.h>
/* A 320 x 200 picture into px (64000 indices) and pal (768, 6-bit); 0, or -1
   when it is not a GIF of that size. */
int gp_gif(const uint8_t *d, uint32_t n, uint8_t *pal, uint8_t *px);
