/* SPDX-License-Identifier: MIT; the board's packed games: .GMP files in
 * /sd/GAMES and /sd/POCKET (the Pocket Player's folder, so one card serves
 * both players), each with its further volumes (NAME.G01 ...) found in the
 * same listing, read through the kernel's files for gmp.h. One pack is open
 * at a time; ABI 1.3 uses bounded absolute seek on its open volume. Older
 * kernels retain the forward-read/reopen fallback. */
#pragma once
#include <stdint.h>
#include "gmp.h"
#define SP_FOLDER_GAMES 1u
#define SP_FOLDER_POCKET 2u
struct sp_pack {uint8_t folder, volumes; char base[9]; uint32_t size[GMP_VOLUMES_MAX];};
/* Finds the packs of folder (SP_FOLDER_*) into packs[0..max): their count.
   say logs the ones it skips. */
unsigned sp_list(uint8_t folder, struct sp_pack *packs, unsigned max, void (*say)(const char *));
/* Call once at startup; unknown majors and minors below 3 never issue call 9.
   Defaults to the safe legacy path until configured. */
void sp_set_abi(unsigned major, unsigned minor);
/* Opens a pack for gmp_read (the open pack is replaced): the struct gmp, or 0. */
struct gmp *sp_open(const struct sp_pack *p);
/* Called while long reads go on (the kernel's heartbeat). */
extern void (*sp_beat)(void);
/* "/sd/GAMES/DICE.GMP", or a further volume's name. */
void sp_path(const struct sp_pack *p, unsigned volume, char out[40]);
