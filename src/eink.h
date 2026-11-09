/* SPDX-License-Identifier: MIT; the OS's use of the e-ink panel (kernel ABI 1.9, docs/eink.md in the kernel): its title and a
 * status page on the games list. Pure code (the host tests run it); main.c does the /dev/eink calls. The kernel limits
 * updates (1 s apart, 30 a minute); this asks far less: a page only after the selection has been still for EI_SETTLE_MS. */
#pragma once
#include <stdint.h>
#define EI_PAGE_BYTES 3904u
#define EI_SETTLE_MS 2000u
#define EI_MIN_GAP_MS 5000u    /* between two pages we send, whatever the kernel allows */
#define EI_RETRY_MS 10000u     /* after the kernel said too soon */
#define EI_NAME "GAME-MAKER OS"
extern const uint8_t ei_status_frame[EI_PAGE_BYTES];
extern const uint8_t ei_mark[200];
/* The status page: the frame plus MODE, GAMES and GAME values (upper case, clipped to EI_VALUE_CHARS). */
#define EI_VALUE_CHARS 14u
void ei_page(uint8_t *out, const char *mode, unsigned games, const char *game);
struct ei_state {
  uint8_t page[EI_PAGE_BYTES], sent[EI_PAGE_BYTES];
  uint8_t have_sent;
  uint64_t changed_ns, sent_ns, retry_ns;
};
void ei_init(struct ei_state *s);
/* Call often with the current list state. Returns 1 when st->page should be written to /dev/eink now (then call ei_sent). */
int ei_update(struct ei_state *s, uint64_t now_ns, unsigned games, const char *game);
void ei_sent(struct ei_state *s, uint64_t now_ns, int ok);
