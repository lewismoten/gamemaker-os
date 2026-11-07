/* SPDX-License-Identifier: MIT; a game's title screen for its card and its
 * details view, read from the game's own files as the Sandbox Edition web
 * site's Games page picks it (web/src/gfx/titlePicture.ts): scene 0's title
 * graphic, else its menu picture; a GIF, a backdrop's window of one, an
 * animation's last frame (or its most colorful, when the last is much
 * plainer), or the picture a .LST holds longest. Game-Maker's own splash
 * screens and blank pictures are passed over. Pure: files come through the
 * reader the platform gives. */
#pragma once
#include <stdint.h>
#include "game.h"
#define GM_THUMB_W 194u
#define GM_THUMB_H 121u
#define GM_THUMB_SLOTS 24u /* cards kept drawn, the least recently used given up first */
/* The title screen as 320 x 200 palette indices and its palette (768 bytes,
   6-bit VGA): 0, or -1 when the game has none. Uses the play core's .GAM
   reader (gp_open), so not while a game plays. */
int gm_title_picture(gp_read_fn read, void *user, const char *gam_name, uint8_t *px, uint8_t *pal);
/* A 320 x 200 picture boxed down to w x h XRGB8888: each pixel the average of
   the pixels it covers, so lettering stays readable. */
void gm_shrink(const uint8_t *px, const uint8_t *pal, uint32_t *out, uint32_t w, uint32_t h);
/* The card picture of game `id` if it is kept (0 if not), and keeping one. */
const uint32_t *gm_thumb_get(unsigned id);
void gm_thumb_put(unsigned id, const uint8_t *px, const uint8_t *pal);
void gm_thumb_clear(void);
