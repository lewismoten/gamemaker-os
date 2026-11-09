/* SPDX-License-Identifier: MIT; see eink.h. Draws the values with the OS's 5x7 capitals at 2x (14 px tall, as the labels). */
#include "eink.h"
#include "font.h"
static void put_px(uint8_t *p, int x, int y) {if (x >= 0 && x < 250 && y >= 0 && y < 122) p[y * 32 + x / 8] |= (uint8_t)(0x80u >> (x & 7));}
static unsigned draw(uint8_t *p, int x, int y, const char *s, unsigned max) {
  unsigned n = 0;
  for (; s[n] && n < max; n++) {
    unsigned c = (unsigned char)s[n];
    if (c >= 'a' && c <= 'z') c -= 32u;
    if (c < 32 || c > 126) c = '?';
    for (int r = 0; r < 7; r++) for (int col = 0; col < 5; col++)
      if (gm_font[c - 32][r] >> (4 - col) & 1) for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) put_px(p, x + col * 2 + dx, y + r * 2 + dy);
    x += 12;
  }
  return n;
}
void ei_page(uint8_t *out, const char *mode, unsigned games, const char *game) {
  for (unsigned i = 0; i < EI_PAGE_BYTES; i++) out[i] = ei_status_frame[i];
  char num[11]; int k = 0, j = 0; unsigned v = games; do num[k++] = (char)('0' + v % 10); while (v /= 10);
  char buf[11]; while (k) buf[j++] = num[--k]; buf[j] = 0;
  draw(out, 76, 48, mode, EI_VALUE_CHARS); draw(out, 76, 70, buf, EI_VALUE_CHARS); draw(out, 76, 92, game, EI_VALUE_CHARS);
}
void ei_init(struct ei_state *s) {
  s->have_sent = 0; s->changed_ns = s->sent_ns = s->retry_ns = 0;
  for (unsigned i = 0; i < EI_PAGE_BYTES; i++) s->page[i] = s->sent[i] = 0;
}
int ei_update(struct ei_state *s, uint64_t now_ns, unsigned games, const char *game) {
  uint8_t next[EI_PAGE_BYTES]; ei_page(next, "LIBRARY", games, game);
  int differs = 0; for (unsigned i = 0; i < EI_PAGE_BYTES; i++) if (next[i] != s->page[i]) {differs = 1; break;}
  if (differs || (!s->changed_ns && !s->have_sent)) {for (unsigned i = 0; i < EI_PAGE_BYTES; i++) s->page[i] = next[i]; s->changed_ns = now_ns ? now_ns : 1;}
  int pending = !s->have_sent; if (!pending) for (unsigned i = 0; i < EI_PAGE_BYTES; i++) if (s->page[i] != s->sent[i]) {pending = 1; break;}
  if (!pending) return 0;
  if (now_ns - s->changed_ns < (uint64_t)EI_SETTLE_MS * 1000000ull) return 0;
  if (s->have_sent && now_ns - s->sent_ns < (uint64_t)EI_MIN_GAP_MS * 1000000ull) return 0;
  if (now_ns < s->retry_ns) return 0;
  return 1;
}
void ei_sent(struct ei_state *s, uint64_t now_ns, int ok) {
  if (!ok) {s->retry_ns = now_ns + (uint64_t)EI_RETRY_MS * 1000000ull; return;}
  for (unsigned i = 0; i < EI_PAGE_BYTES; i++) s->sent[i] = s->page[i];
  s->have_sent = 1; s->sent_ns = now_ns;
}
