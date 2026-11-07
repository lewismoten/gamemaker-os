/* SPDX-License-Identifier: MIT; the title of a Game-Maker game from its .GAM
 * file: the start scene's description, as the Sandbox Edition web player
 * shows it (gameTitle in its engine's formats/game.ts). Version 3.00 files
 * start with "GM" and keep it at byte 285 (20 bytes); older files keep it at
 * byte 6 of the first 330-byte scene record (15 bytes). The text is CP437:
 * spaces and box or block characters around it are trimmed, accented letters
 * become their plain letters, and anything else outside ASCII becomes '?'. */
#pragma once
#include <stdint.h>
#define GM_TITLE_MAX 20u
#define GM_GAM_TITLE_BYTES 512u /* enough of the file for either version */
/* out gets at most GM_TITLE_MAX characters and a NUL. Returns the length
   (0 when the description is empty) or -1 when data is not a usable .GAM. */
int gm_gam_title(const uint8_t *data, uint32_t bytes, char out[GM_TITLE_MAX + 1]);
