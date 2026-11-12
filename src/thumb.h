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
struct gm_menu;
#define GM_THUMB_W 218u /* a card on the 960 x 540 page, inside its line */
#define GM_THUMB_H 136u
#define GM_THUMB_SLOTS 24u /* cards kept drawn, the least recently used given up first */
/* The title screen as 320 x 200 palette indices and its palette (768 bytes,
   6-bit VGA): 0, or -1 when the game has none. Uses the play core's .GAM
   reader (gp_open), so not while a game plays. */
int gm_title_picture(gp_read_fn read, void *user, const char *gam_name, uint8_t *px, uint8_t *pal);
/* A 320 x 200 picture boxed down to w x h RGB565 (rgb565.h): each pixel the
   average of the pixels it covers, so lettering stays readable. */
void gm_shrink(const uint8_t *px, const uint8_t *pal, uint16_t *out, uint32_t w, uint32_t h);
/* The card picture of game `id` if it is kept (0 if not), and keeping one. */
const uint16_t *gm_thumb_get(unsigned id);
void gm_thumb_put(unsigned id, const uint8_t *px, const uint8_t *pal);
void gm_thumb_clear(void);
<<<<<<< HEAD
/* Cooperative picture loading: present immediately, then at 50 ms checkpoints.
   The transport must checkpoint between bounded reads; a blocking syscall itself
   cannot be preempted here. The callbacks must not load another picture. */
struct gm_load_progress {
  uint64_t last;
  uint64_t (*now)(void *user);
  void (*present)(void *user);
  void *user;
};
void gm_load_start(struct gm_load_progress *p, uint64_t (*now)(void *), void (*present)(void *), void *user);
void gm_load_tick(struct gm_load_progress *p);
=======
/* True when game `id`'s card is kept, without counting as a use. */
int gm_thumb_has(unsigned id);
/* The game whose card to draw next, or -1 when the page and its neighbors are all drawn (or
   the details view shows): the selected card, the rest of its page (*on_page = 1), then the
   next page and the previous one (*on_page = 0). Neighbors are 16 cards of the 24 kept, so a
   page turn finds its cards drawn and the visible ones are never given up for them. */
int gm_thumb_next(const struct gm_menu *m, int *on_page);
>>>>>>> origin/feature/gmos-thumb-speed-1112
