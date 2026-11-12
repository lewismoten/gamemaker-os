/* SPDX-License-Identifier: MIT; which card's title screen to draw next (thumb.h):
 * the selected card first, then the rest of its page, then the next page and
 * the previous one (so turning a page finds its cards kept), one per call. */
#include "thumb.h"
#include "menu.h"
static int wanted(const struct gm_menu *m, const struct gm_game *g) {
  return g && g->pic >= 0 && (g->pic == 0 || !gm_thumb_has((unsigned)(g - m->games)));
}
int gm_thumb_next(const struct gm_menu *m, int *on_page) {
  if (m->view != GM_GRID) return -1;
  unsigned t = m->tab, size = gm_tab_size(m, t), pos = m->pos[t], first = pos / GM_PAGE * GM_PAGE;
  if (wanted(m, gm_tab_game(m, t, pos))) {*on_page = 1; return (int)(gm_tab_game(m, t, pos) - m->games);}
  for (unsigned i = first; i < first + GM_PAGE && i < size; i++) {
    const struct gm_game *g = gm_tab_game(m, t, i);
    if (wanted(m, g)) {*on_page = 1; return (int)(g - m->games);}
  }
  for (int side = 0; side < 2; side++) { /* ahead first: paging forward is the common move */
    if (side ? first < GM_PAGE : first + GM_PAGE >= size) continue;
    unsigned a = side ? first - GM_PAGE : first + GM_PAGE;
    for (unsigned i = a; i < a + GM_PAGE && i < size; i++) {
      const struct gm_game *g = gm_tab_game(m, t, i);
      if (wanted(m, g)) {*on_page = 0; return (int)(g - m->games);}
    }
  }
  return -1;
}
