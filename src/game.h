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
enum {GP_TITLE, GP_WON, GP_LOST, GP_INTRO, GP_EXIT, GP_MENU, GP_SCORES, GP_LOADPIC, GP_SAVEPIC, GP_INSTRUCTIONS, GP_STORY, GP_CREDITS};
/* Reads the .GAM (version 3.00, or an older one converted as GameClass::Load
   converts it); returns the number of levels (scenes of type 10), or -1. */
int gp_open(gp_read_fn read, void *user, const char *gam_name);
/* The .GAM as version 3.00 bytes, as gp_open read it (an older file converted):
   the Sandbox Edition's serializeGame(parseGame(file)). */
const uint8_t *gp_gam_bytes(uint32_t *n);
/* The file name in a graphic slot ("" when empty); level is for GP_INTRO/GP_EXIT. */
const char *gp_graphic(int slot, int level);
const char *gp_title(void); /* scene 0 description */
int gp_begin(void); /* the first level, from the start of the game: 0, or -1 if its files are missing */
void gp_key(int scan, int down); /* a DOS scan code (1..127) */
void gp_release_all(void);
/* One game tick at 18.2 Hz: 0, 'D' (died), 'L' (took a link) or 'W' (won).
   A 'D' or 'L' holds the level change until gp_finish (the board shows the
   cards between levels first). */
char gp_tick(void);
int gp_finish(void); /* enters the level the last 'D' or 'L' named: 0 or -1 */
int gp_level(void);
int32_t gp_score(void); /* the score so far (PLAYGAME ci.score) */
int gp_lives(void);
int gp_pending_level(void); /* the level a held 'L' goes to */
/* The 320 x 200 picture of the game as palette indices (rows outside the
   312 x 196 view are 0), and the level's palette (768 bytes, 6-bit VGA). */
void gp_render(uint8_t *px);
const uint8_t *gp_palette(void);
/* Sound, as events (the board has no audio: SND log lines, src/sound.h).
   The effect the last ticks started (the last one is the one heard, as on the
   Pocket Player), or -1; it empties the core's queue. */
int gp_take_sound(void);
/* The level's .SND file name ("" when it has none) and effect n's Sound
   Blaster clip name from its table (no extension; "" when the effect has
   none, or its name starts with a space: PLAYGAME's PC speaker path). */
const char *gp_sound_file(void);
const char *gp_voc_name(int n);
/* The demo recording's name: scene 0's file slot 11 ("" when empty). */
const char *gp_demo(void);
/* The .CMF song slots PLAYGAME plays: the title's, the won and lost pages',
   and a level's (level is for GP_SONG_LEVEL); "" when empty. */
enum {GP_SONG_TITLE, GP_SONG_WON, GP_SONG_LOST, GP_SONG_LEVEL, GP_SONG_MENU, GP_SONG_STORY};
const char *gp_song(int slot, int level);
/* FNV-1a of the 312 x 196 view, row by row: the Pocket Player host test's frameHash. */
uint32_t gp_frame_hash(void);
/* Same hash from gp_render's existing pixels, without rendering a second time. */
uint32_t gp_pixels_hash(const uint8_t *px);
/* Save and load a game as PLAYGAME does (SaveGame and LoadGame, PLAYGAME.C 1836-1892, 1737-1834),
   through the DOS wire records of savefile.h. gp_state_save fills g from the play in progress (0, or -1
   with no game running). gp_state_load checks g first and changes nothing when it refuses (-1); when
   the level's files cannot be read after the checks it returns -2 with the play half replaced, and the
   caller loads the state it saved before. */
struct save_game;
int gp_state_save(struct save_game *g);
int gp_state_load(const struct save_game *g);
#ifdef __cplusplus
}
namespace gm {struct World;}
/* The host state-line renderer inspects the shared world. */
gm::World &gp_world();
#endif
