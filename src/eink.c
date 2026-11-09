/* SPDX-License-Identifier: MIT; see eink.h. Draws the values with the OS's 5x7 capitals at 2x (14 px tall, as the labels). */
#include "eink.h"
#include "font.h"
static void put_px(uint8_t *p, int x, int y) {if (x >= 0 && x < 250 && y >= 0 && y < 122) p[y * 32 + x / 8] |= (uint8_t)(0x80u >> (x & 7));}
static void draw(uint8_t *p, int x, int y, const char *s, unsigned n, int sc) {
  for (unsigned i = 0; i < n && s[i]; i++) {
    unsigned c = (unsigned char)s[i];
    if (c >= 'a' && c <= 'z') c -= 32u;
    if (c < 32 || c > 126) c = '?';
    for (int r = 0; r < 7; r++) for (int col = 0; col < 5; col++)
      if (gm_font[c - 32][r] >> (4 - col) & 1) for (int dy = 0; dy < sc; dy++) for (int dx = 0; dx < sc; dx++) put_px(p, x + col * sc + dx, y + r * sc + dy);
    x += 6 * sc;
  }
}
/* Length of the first line of s (at most EI_LINE_CHARS), breaking at a space when the text goes on; *skip is the characters to step over. */
static unsigned line_of(const char *s, unsigned *skip) {
  unsigned n = 0; while (s[n] && n < EI_LINE_CHARS) n++;
  *skip = n;
  if (!s[n]) return n;
  if (s[n] == ' ') {*skip = n + 1; return n;}
  unsigned k = n; while (k && s[k - 1] != ' ') k--;
  if (!k) return n;               /* one long word: break it */
  *skip = k; return k - 1;
}
void ei_page(uint8_t *out, const char *mode, unsigned games, const char *game) {
  for (unsigned i = 0; i < EI_PAGE_BYTES; i++) out[i] = ei_status_frame[i];
  for (unsigned y = 20; y < 122; y++) for (unsigned x = y < 44 ? 6u : 0u; x < 32; x++) out[y * 32 + x] = 0; /* the frame's own labels go; the mark (x < 48) stays */
  draw(out, 50, 26, mode, 24, 1);
  char head[24] = "GAMES: "; unsigned h = 7, v = games; char t[11]; int k = 0;
  do {t[k++] = (char)('0' + v % 10);} while (v /= 10);
  while (k) head[h++] = t[--k];
  head[h] = 0;
  draw(out, 4, 46, head, h, 2);
  unsigned skip, n = line_of(game, &skip); draw(out, 4, 66, game, n, 2);
  const char *rest = game + skip; while (*rest == ' ') rest++;
  if (!*rest) return;
  char l2[EI_LINE_CHARS + 1]; unsigned n2 = line_of(rest, &skip); const char *after = rest + skip; while (*after == ' ') after++;
  if (*after) {  /* more than two lines: the second ends in "..." */
    if (n2 > EI_LINE_CHARS - 3) {n2 = EI_LINE_CHARS - 3; while (n2 && rest[n2 - 1] == ' ') n2--;}
    for (unsigned i = 0; i < n2; i++) l2[i] = rest[i];
    l2[n2++] = '.'; l2[n2++] = '.'; l2[n2++] = '.'; l2[n2] = 0;
  } else {for (unsigned i = 0; i < n2; i++) l2[i] = rest[i]; l2[n2] = 0;}
  draw(out, 4, 86, l2, n2, 2);
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
