/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten. Claude Sonnet 5.5. */
#include "hiscore.h"
#include <string.h>
void his_parse(struct his_table *t, const uint8_t *data, uint32_t bytes) {
  uint8_t raw[HIS_BYTES]; memset(raw, 0, sizeof raw);
  if (data && bytes) memcpy(raw, data, bytes < HIS_BYTES ? bytes : HIS_BYTES);
  for (unsigned i = 0; i < HIS_ROWS; i++) {
    const uint8_t *p = raw + i * 34u;
    t->row[i].score = (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
    memcpy(t->row[i].name, p + 4, HIS_NAME);
  }
}
void his_serialize(const struct his_table *t, uint8_t out[HIS_BYTES]) {
  for (unsigned i = 0; i < HIS_ROWS; i++) {
    uint8_t *p = out + i * 34u; uint32_t s = (uint32_t)t->row[i].score;
    p[0] = (uint8_t)s; p[1] = (uint8_t)(s >> 8); p[2] = (uint8_t)(s >> 16); p[3] = (uint8_t)(s >> 24);
    memcpy(p + 4, t->row[i].name, HIS_NAME);
  }
}
int his_place(const struct his_table *t, int32_t score) {
  for (int i = 0; i < HIS_ROWS; i++) if (score > t->row[i].score) return i;
  return -1;
}
int his_insert(struct his_table *t, int32_t score, const char *name) {
  int at = his_place(t, score); if (at < 0) return 0;
  for (int k = HIS_ROWS - 1; k > at; k--) t->row[k] = t->row[k - 1];
  /* DOS sets only yourname[0] = 0 before the prompt (HISCOREL.C), so the shifted-in
     row's old name bytes stay behind the new name; match it byte for byte. */
  t->row[at].score = score; t->row[at].name[0] = 0;
  /* DOS's GGet writes the typed text and its terminator. */
  unsigned i = 0; for (; name && name[i] && i < HIS_NAME_MAX; i++) t->row[at].name[i] = (uint8_t)name[i];
  t->row[at].name[i] = 0;
  return at + 1;
}
