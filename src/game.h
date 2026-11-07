/* SPDX-License-Identifier: MIT; one Game-Maker game read from its original
 * DOS files (.GAM, .MAP, .BBL, .CBL, .CHR, .PAL, .MON, .MBL, .SND) and played
 * by the shared Game-Maker play core (third_party/gmcore, the same files the
 * Pocket Player firmware builds). Pure: files come through a reader the
 * platform gives (the kernel's /sd on the board, stdio in the host tests). */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GP_W 320u
#define GP_H 200u
#define GP_VIEW_W 312u /* CropScreen: PLAYGAME shows 312 x 196 while playing */
#define GP_VIEW_H 196u
#define GP_NAME_MAX 15u
/* Reads up to cap bytes of the game folder's file `name` (as the .GAM spells
   it); returns the bytes read, or a negative value if there is no such file. */
typedef int32_t (*gp_read_fn)(void *user, const char *name, uint8_t *buf, uint32_t cap);
/* Scene 0's title graphic and scene 1's won and lost graphics, a level's
   intro and exit graphics (ShowGraphic's file slots), and scene 0's menu picture. */
enum {GP_TITLE, GP_WON, GP_LOST, GP_INTRO, GP_EXIT, GP_MENU};
/* Reads the .GAM (version 3.00, or an older one converted as GameClass::Load
   converts it); returns the number of levels (scenes of type 10), or -1. */
int gp_open(gp_read_fn read, void *user, const char *gam_name);
/* The .GAM as version 3.00 bytes, as gp_open read it (an older file converted):
   the Sandbox Edition's serializeGame(parseGame(file)). */
const uint8_t *gp_gam_bytes(uint32_t *n);
/* The file name in a graphic slot ("" when empty); level is for GP_INTRO/GP_EXIT. */
const char *gp_graphic(int slot, int level);
int gp_begin(void); /* the first level, from the start of the game: 0, or -1 if its files are missing */
void gp_key(int scan, int down); /* a DOS scan code (1..127) */
void gp_release_all(void);
/* One game tick at 18.2 Hz: 0, 'D' (died), 'L' (took a link) or 'W' (won).
   A 'D' or 'L' holds the level change until gp_finish (the board shows the
   cards between levels first). */
char gp_tick(void);
int gp_finish(void); /* enters the level the last 'D' or 'L' named: 0 or -1 */
int gp_level(void);
int gp_lives(void);
int gp_pending_level(void); /* the level a held 'L' goes to */
/* The 320 x 200 picture of the game as palette indices (rows outside the
   312 x 196 view are 0), and the level's palette (768 bytes, 6-bit VGA). */
void gp_render(uint8_t *px);
const uint8_t *gp_palette(void);
/* FNV-1a of the 312 x 196 view, row by row: the Pocket Player host test's frameHash. */
uint32_t gp_frame_hash(void);
#ifdef __cplusplus
}
#endif
