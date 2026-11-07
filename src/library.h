/* SPDX-License-Identifier: MIT; the library index on the card,
 * /sd/GAMES/LIBRARY.JSL: the Pocket Player's library.jsonl format from
 * Game-Maker: Sandbox Edition (web/src/export/pocketLibrary.ts), one JSON
 * object a line. The first line is {"schema":"gmp-library-index",...}; each
 * game's line has "schema":"gmp-library", its title, category, the .GAM's
 * CRC-32 and DOS name, and for Game-Maker OS also its company, year and blurb
 * (what the web Games page shows). Pure: no system calls. */
#pragma once
#include <stdint.h>
#define GM_LIB_TITLE 63u
#define GM_LIB_COMPANY 31u
#define GM_LIB_BLURB 639u
#define GM_LIB_WORD 31u
struct gm_record {
  char title[GM_LIB_TITLE + 1], company[GM_LIB_COMPANY + 1], blurb[GM_LIB_BLURB + 1];
  char category[2][GM_LIB_WORD + 1], dos[9], locator[GM_LIB_WORD + 1];
  uint32_t crc; int has_crc;
  int32_t year, course; /* 0 when absent */
};
/* Reads the line [line, line + n) into r: 1 when it is a game's record, 0 when
   it is something else (the index header, a blank or broken line). Text is
   folded to printable ASCII: curly quotes and dashes to their plain kind,
   accented Latin letters to their letters, anything else to '?'. */
int gm_record_parse(const char *line, uint32_t n, struct gm_record *r);
/* CRC-32/ISO-HDLC, as the index's gamCrc32 (start with crc 0). */
uint32_t gm_crc32(uint32_t crc, const uint8_t *d, uint32_t n);
