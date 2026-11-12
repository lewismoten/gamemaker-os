# SPDX-License-Identifier: MIT
"""Which card's title screen the board draws next while the list is idle (src/thumbq.c): the
selected card and its page first, the next and previous pages after (16 of the 24 cards kept),
nothing again once they are drawn, and a page turned back to is found kept. One card per call,
so one decode is the most a pass of the main loop waits on."""
import ctypes as C
import unittest
from gmos_page import lib, PAGE
from test_gmos import many

def setup():
    c = lib()
    c.gm_thumb_next.argtypes = [C.c_void_p, C.POINTER(C.c_int)]; c.gm_thumb_next.restype = C.c_int
    c.gm_thumb_has.argtypes = [C.c_uint]; c.gm_thumb_has.restype = C.c_int
    return c
PX, PAL = (C.c_uint8 * 64000)(*([1] * 64000)), (C.c_uint8 * 768)()

def next_card(p):
    on = C.c_int(0); i = setup().gm_thumb_next(p.m, C.byref(on)); return i, on.value
def draw(p, i): setup().gmos_menu_pic(p.m, i, 1); setup().gm_thumb_put(i, PX, PAL)
def drain(p):
    """What the idle loop does: card after card until none is wanted. Returns [(game, on_page)]."""
    out = []
    while True:
        i, on = next_card(p)
        if i < 0: return out
        draw(p, i); out.append((i, on))

class Queue(unittest.TestCase):
    def test_selected_card_then_its_page_then_neighbors(self):
        p = many(40); p.keys('pgdn', 'pgdn', 'right', 'right')   # page 3 of 5, the third card selected
        self.assertEqual(p.pos, 18)
        got = drain(p); ids = [i for i, _ in got]
        self.assertEqual(ids[0], p.selected)
        self.assertEqual({i for i in ids[:PAGE]}, set(range(16, 24)))            # the visible page, all of it, before anything else
        self.assertTrue(all(on for _, on in got[:PAGE]) and not any(on for _, on in got[PAGE:]))
        self.assertEqual(ids[PAGE:2 * PAGE], list(range(24, 32)))                # then the next page, then the previous
        self.assertEqual(ids[2 * PAGE:], list(range(8, 16)))
    def test_bounded_memory_and_nothing_left_to_do(self):
        p = many(40); got = drain(p)
        self.assertEqual(len(got), 2 * PAGE)                                      # page 1 and the next: no earlier page to draw
        p.keys('pgdn', 'pgdn'); more = drain(p)
        self.assertLessEqual(sum(setup().gm_thumb_has(i) for i in range(40)), 24)  # the cap: 24 cards of 218 x 136 x 2 bytes
        self.assertEqual(next_card(p)[0], -1)
        self.assertLessEqual(len(more), 2 * PAGE)                                 # page 3's own, and page 4's; page 2 was kept
    def test_scroll_back_finds_cards_kept(self):
        p = many(40); drain(p); p.keys('pgdn'); drain(p)
        p.keys('pgup')
        self.assertEqual(next_card(p)[0], -1)                                     # nothing to read again
    def test_visible_cards_never_given_up_for_neighbors(self):
        p = many(60); p.keys('pgdn', 'pgdn', 'pgdn'); drain(p)
        for i in range(24, 32): self.assertTrue(setup().gm_thumb_has(i))
    def test_details_view_reads_nothing(self):
        p = many(10); p.keys('enter')
        self.assertEqual(next_card(p)[0], -1)
    def test_cards_without_a_picture_are_not_asked_twice(self):
        p = many(10); setup().gmos_menu_pic(p.m, 0, -1)
        self.assertNotIn(0, [i for i, _ in drain(p)])
if __name__ == '__main__': unittest.main()
