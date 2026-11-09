/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten.
 * Hermes OpenAI Codex GPT-6.1 Sol. Synthetic public-layout fixtures only. */
#include "savefile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void minimum(void) {
  uint8_t *wire = calloc(37755, 1), *out = malloc(37755);
  struct save_game *g = malloc(sizeof *g);
  uint32_t n = 0;
  assert(wire && out && g);
  wire[1] = 0xfe; wire[2] = 0xff; /* previous scene = -2 */
  wire[7] = 0x78; wire[8] = 0x56; wire[9] = 0x34; wire[10] = 0x12;
  assert(save_game_decode(g, wire, 37755) == 0);
  assert(g->prevscene == -2 && g->sound_clock == 0x12345678);
  assert(save_game_size(g) == 37755);
  assert(save_game_encode(g, out, 37755, &n) == 0 && n == 37755);
  assert(!memcmp(wire, out, n));
  assert(g->blocks[149].next_time == 0 && g->records[999].key == 0);
  free(g); free(out); free(wire);
}
static void patterned(const void *data, size_t n, unsigned seed) {
  const uint8_t *p = data;
  for (size_t i = 0; i < n; i++) assert(p[i] == (uint8_t)(i * 37 + seed));
}
static void decoded(const struct save_game *g, unsigned m, unsigned r) {
  assert(g->block_count == m && g->record_count == (int)r);
  assert(g->prevscene == -32768 && g->doscene == 32767 && g->linkin == -2);
  assert(g->sound_clock == 0xfedcba98);
  for (unsigned i = 0; i < 150; i++) assert(g->next_change[i] == (0x12345678u ^ i));
  patterned(g->block_map, 150, 1);
  patterned(g->character_info, 135, 2);
  patterned(g->character, 3784, 3);
  assert(g->map_x == -12345 && g->map_y == 23456);
  patterned(g->monster_info, 200 * 55, 4);
  patterned(g->monster_map, 200 * 10, 5);
  patterned(g->map, 100 * 100 * 2, 6);
  assert(g->monster_count == 200);
  for (unsigned i = 0; i < m; i++) {
    assert(g->blocks[i].block == i && g->blocks[i].next_time == (0x87654321u ^ i));
  }
  for (unsigned i = m; i < 150; i++) assert(!g->blocks[i].block && !g->blocks[i].next_time);
  assert(g->last_recorded_key == -123);
  patterned(g->record_file, 51, 7);
  assert(g->record_pointer == 0x1234fedc && g->record_timer == 0xabcdef01);
  assert(g->record_flag == 1 && g->last_key_press == 0x80000000 && g->record_paused == 0x8001);
  for (unsigned i = 0; i < r; i++) {
    assert(g->records[i].key == (uint8_t)i && g->records[i].delta_time == (uint16_t)(i * 97));
  }
  for (unsigned i = r; i < 1000; i++) assert(!g->records[i].key && !g->records[i].delta_time);
}
static void rejected(const uint8_t *wire, uint32_t n) {
  struct save_game *g = malloc(sizeof *g), *before = malloc(sizeof *before);
  assert(g && before); memset(g, 0x5a, sizeof *g); memcpy(before, g, sizeof *g);
  assert(save_game_decode(g, wire, n) == SAVE_EINVAL);
  assert(!memcmp(g, before, sizeof *g)); free(before); free(g);
}
static void malformed(const uint8_t *wire, uint32_t n) {
  /* Exact allocations, including the short Recording header cases. */
  for (uint32_t len = 0; len < n; len++) {
    uint8_t *shorter = malloc(len ? len : 1); assert(shorter);
    memcpy(shorter, wire, len); rejected(shorter, len); free(shorter);
  }
  uint8_t *bad = malloc(n + 1); assert(bad); memcpy(bad, wire, n); bad[n] = 0;
  rejected(bad, n + 1); /* no silently ignored trailers */
  bad[0] = 151; rejected(bad, n); bad[0] = 255; rejected(bad, n);
  memcpy(bad, wire, n);
  unsigned rec = 37685 + 5 * wire[0];
  bad[rec] = 0xff; bad[rec+1] = 0xff; rejected(bad, n); /* signed -1 */
  bad[rec] = 0xe9; bad[rec+1] = 3; rejected(bad, n); /* 1001 */
  bad[rec] = 0; bad[rec+1] = 0x80; rejected(bad, n); /* -32768 */
  free(bad);
  /* Repair framing rather than letting the length guard shield count guards.
   * R=1001 with M=0 is below the global byte ceiling. M=151/255 with R=0
   * also fits that ceiling but would overflow the owned block array. */
  uint8_t *framed = calloc(41505, 1); assert(framed);
  framed[37685] = 0xe9; framed[37686] = 3;
  rejected(framed, 37755 + 3 * 1001);
  memset(framed, 0, 41505);
  framed[0] = 151; rejected(framed, 37755 + 5 * 151);
  framed[0] = 255; rejected(framed, 37755 + 5 * 255);
  free(framed);
}
static void capacities(const struct save_game *g, uint32_t size) {
  /* Every insufficient cap is an exact allocation; no partial output or n. */
  for (uint32_t cap = 0; cap < size; cap++) {
    uint8_t *out = malloc(cap ? cap : 1); assert(out); memset(out, 0x5a, cap);
    uint32_t n = 0xdeadbeef;
    assert(save_game_encode(g, out, cap, &n) == SAVE_EFBIG);
    assert(n == 0xdeadbeef);
    for (uint32_t i = 0; i < cap; i++) assert(out[i] == 0x5a);
    free(out);
  }
}
static void arguments(struct save_game *g, uint8_t *wire, uint32_t n) {
  uint8_t *out = malloc(n); struct save_game *before = malloc(sizeof *before);
  assert(out && before); memset(out, 0x5a, n); memcpy(before, g, sizeof *g);
  uint32_t size = 123;
  assert(!save_game_size(NULL));
  assert(save_game_decode(NULL, wire, n) == SAVE_EINVAL);
  assert(save_game_decode(g, NULL, n) == SAVE_EINVAL);
  assert(save_game_encode(NULL, out, n, &size) == SAVE_EINVAL);
  assert(save_game_encode(g, NULL, n, &size) == SAVE_EINVAL);
  assert(save_game_encode(g, out, n, NULL) == SAVE_EINVAL);
  assert(save_game_decode(g, g, n) == SAVE_EINVAL);
  assert(save_game_decode(g, (uint8_t *)g + 1, n) == SAVE_EINVAL);
  assert(save_game_encode(g, g, n, &size) == SAVE_EINVAL);
  assert(save_game_encode(g, (uint8_t *)g + 1, n, &size) == SAVE_EINVAL);
  assert(save_game_encode(g, out, n, (uint32_t *)out) == SAVE_EINVAL);
  assert(save_game_encode(g, out, n, &g->sound_clock) == SAVE_EINVAL);
  assert(!memcmp(g, before, sizeof *g));
  g->record_count = -1; assert(!save_game_size(g));
  assert(save_game_encode(g, out, n, &size) == SAVE_EINVAL);
  g->record_count = 1001; assert(!save_game_size(g));
  assert(save_game_encode(g, out, n, &size) == SAVE_EINVAL);
  memcpy(g, before, sizeof *g); g->block_count = 151; assert(!save_game_size(g));
  assert(save_game_encode(g, out, n, &size) == SAVE_EINVAL);
  memcpy(g, before, sizeof *g);
  assert(size == 123);
  for (uint32_t i = 0; i < n; i++) assert(out[i] == 0x5a);
  free(before); free(out);
}
int main(int argc, char **argv) {
  minimum();
  if (argc == 5) {
    unsigned m = (unsigned)strtoul(argv[3], NULL, 10), r = (unsigned)strtoul(argv[4], NULL, 10);
    uint32_t n = 37755 + 5 * m + 3 * r, written = 0;
    /* Wire starts at an unaligned byte, surrounded by sentinels. */
    uint8_t *storage = malloc(n + 2), *outstore = malloc(n + 3);
    struct save_game *g = malloc(sizeof *g);
    assert(storage && outstore && g);
    uint8_t *wire = storage + 1, *out = outstore + 1;
    memset(storage, 0xa5, n + 2); memset(outstore, 0xa5, n + 3);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(fread(wire, 1, n, f) == n && fgetc(f) == EOF); assert(!fclose(f));
    memset(g, 0x5a, sizeof *g);
    assert(save_game_decode(g, wire, n) == 0); decoded(g, m, r);
    assert(save_game_size(g) == n);
    assert(save_game_encode(g, out, n + 1, &written) == 0 && written == n);
    assert(!memcmp(out, wire, n));
    assert(storage[0] == 0xa5 && storage[n+1] == 0xa5);
    assert(outstore[0] == 0xa5 && outstore[n+1] == 0xa5 && outstore[n+2] == 0xa5);
    f = fopen(argv[2], "wb"); assert(f); assert(fwrite(out, 1, n, f) == n); assert(!fclose(f));
    arguments(g, wire, n);
    if (n < 41505) rejected(wire, n + 1); /* in-range trailer, not global cap */
    if (m == 150 && r == 1000) { malformed(wire, n); capacities(g, n); }
    printf("fixture M=%u R=%u bytes=%u state=%zu PASS\n", m, r, n, sizeof *g);
    free(g); free(outstore); free(storage);
  } else assert(argc == 1);
  puts("GAME0 ALL PASS"); return 0;
}
