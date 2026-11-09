/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * Top ten scores in DOS PLAYGAME's own <game>.HIS file (code/GM/HISCOREL.C):
 * ten records of a signed 32-bit little-endian score then 30 name bytes,
 * 340 bytes, no header. Pure code; the board and the host tests share it.
 * Claude Sonnet 5.5 wrote this file. */
#pragma once
#include <stdint.h>
#define HIS_ROWS 10
#define HIS_NAME 30
#define HIS_NAME_MAX 22 /* what the DOS name prompt accepts */
#define HIS_BYTES 340u
struct his_row {int32_t score; uint8_t name[HIS_NAME];};
struct his_table {struct his_row row[HIS_ROWS];};
/* Like DOS fread on a short or missing file: whatever bytes exist fill the
   table from the start, the rest stay zero. Never fails; bytes past 340 are ignored. */
void his_parse(struct his_table *t, const uint8_t *data, uint32_t bytes);
/* Exactly HIS_BYTES bytes. */
void his_serialize(const struct his_table *t, uint8_t out[HIS_BYTES]);
/* The row a score would take (0 to 9): the first row it strictly beats, so a
   tie goes below the equal score. -1 if it makes no place (scores of 0 or
   less never do, as in DOS). */
int his_place(const struct his_table *t, int32_t score);
/* Insert at his_place(), shifting lower rows down and dropping the last.
   name is cut at HIS_NAME_MAX and padded with zeros; NULL leaves it empty.
   Returns the 1-based position, or 0 if the score didn't place (t unchanged). */
int his_insert(struct his_table *t, int32_t score, const char *name);
