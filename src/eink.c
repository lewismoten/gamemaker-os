/* SPDX-License-Identifier: MIT */
#include "eink.h"
#include "bootart.h"
#include "font.h"
#include <sandbox/syscall.h>
#include <string.h>
#define GAP_NS 2100000000ull
static struct {
  uint64_t fd, next;
  int enabled, boot, clear, library, playing, saving, sent;
  unsigned games;
  uint8_t mark[SBX_EINK_MARK_BYTES];
  char game[17], last[17];
} panel;

/* Reuse the boot badge's 96x96 RLE, sampled at pixel centers. The dark
 * background and black outlines are white on paper; colored castle is black. */
static void castle(void) {
  uint8_t row[96]; unsigned at = 0, left = 0, color = 0; int have = -1;
  for (unsigned y = 0; y < 40; y++) {
    int sy = (int)((2 * y + 1) * 96 / 80);
    while (have < sy) {
      for (unsigned x = 0; x < 96; x++, left--) {
        if (!left) {left = (ba_rle[at] & 15u) + 1; color = ba_rle[at++] >> 4;}
        row[x] = (uint8_t)color;
      }
      have++;
    }
    for (unsigned x = 0; x < 40; x++) {
      unsigned c = row[(2 * x + 1) * 96 / 80];
      if (c != 0 && ba_pal[c]) panel.mark[y * 5 + x / 8] |= (uint8_t)(0x80u >> (x % 8));
    }
  }
}
static void text(uint8_t *page, unsigned x, unsigned y, const char *s, unsigned scale) {
  for (; *s; s++, x += 6 * scale) {
    unsigned c = (unsigned char)*s;
    if (c < 32 || c > 126) c = '?';
    for (unsigned r = 0; r < 7; r++) for (unsigned col = 0; col < 5; col++)
      if (gm_font[c - 32][r] & (1u << (4 - col)))
        for (unsigned dy = 0; dy < scale; dy++) for (unsigned dx = 0; dx < scale; dx++) {
          unsigned px = x + col * scale + dx, py = y + r * scale + dy;
          if (px < 250 && py < 122) page[py * 32 + px / 8] |= (uint8_t)(0x80u >> (px % 8));
        }
  }
}
static int boot_page(void) {
  uint8_t page[SBX_EINK_PAGE_BYTES] = {0};
  for (unsigned y = 0; y < 40; y++) for (unsigned x = 0; x < 40; x++)
    if (panel.mark[y * 5 + x / 8] & (0x80u >> (x % 8))) {
      unsigned px = 105 + x;
      page[(12 + y) * 32 + px / 8] |= (uint8_t)(0x80u >> (px % 8));
    }
  text(page, 54, 65, "GAME-MAKER OS", 2);
  text(page, 80, 94, "LOADING LIBRARY", 1);
  /* x242..249,y0..7 stays white for the kernel's error indicator. */
  return sbx_eink_page(panel.fd, page);
}
void ge_init(unsigned major, unsigned minor) {
  memset(&panel, 0, sizeof panel);
  if (major != 1 || minor < 9) return;
  if (sbx_open("/dev/eink", 9, SBX_O_WRONLY, &panel.fd)) return;
  panel.enabled = panel.boot = 1;
  castle();
  ge_poll();
}
void ge_library(unsigned games) {panel.games = games; panel.library = 1; panel.playing = panel.saving = 0;}
void ge_play(const char *title) {
  unsigned i = 0;
  for (; i < 16 && title[i]; i++) {
    unsigned c = (unsigned char)title[i];
    if (c >= 'a' && c <= 'z') c -= 32;
    panel.game[i] = (char)((c >= 32 && c <= 126) ? c : '?');
  }
  panel.game[i] = 0; panel.playing = 1; panel.saving = 0;
}
void ge_saving(int saving) {panel.saving = saving;}
void ge_poll(void) {
  uint64_t now = 0;
  if (!panel.enabled || sbx_clock_ns(&now) || now < panel.next) return;
  /* TITLE does not remove a PAGE. Clear it on its own cadence slot before
   * installing our title; otherwise the boot bitmap would cover every status. */
  if (panel.clear) {
    if (!panel.library) return;
    int status = sbx_eink_clear(panel.fd);
    panel.next = now + GAP_NS;
    if (!status) panel.clear = 0;
    else if (status != -SBX_EAGAIN) {sbx_close(panel.fd); panel.enabled = 0;}
    return;
  }
  char line[17] = {0};
  if (!panel.boot) {
    if (!panel.library) return;
    if (panel.saving) memcpy(line, "SAVING", 7);
    else if (panel.playing) memcpy(line, panel.game, sizeof line);
    else {
      char digits[11]; unsigned n = panel.games, k = 0, i = 0;
      do {digits[k++] = (char)('0' + n % 10);} while (n /= 10);
      while (k) line[i++] = digits[--k];
      memcpy(line + i, " GAMES", 7);
    }
    if (panel.sent && !strcmp(line, panel.last)) return;
  }
  int status;
  if (panel.boot) status = boot_page();
  else {
    sbx_eink_title_rec title;
    status = sbx_eink_title(panel.fd, &title, "GAME-MAKER OS", 13, panel.mark, line);
  }
  panel.next = now + GAP_NS;
  if (!status) {
    if (panel.boot) {panel.boot = 0; panel.clear = 1;}
    else {memcpy(panel.last, line, sizeof line); panel.sent = 1;}
  } else if (status != -SBX_EAGAIN) {
    /* ENODEV (including a driver failure mid-run), ENOSYS, EBUSY, etc.
     * are optional-feature failures. Release the handle and stay silent. */
    sbx_close(panel.fd); panel.enabled = 0;
  }
}
