// SPDX-License-Identifier: MIT; real session/core end -> Top Ten -> menu.
#include <cassert>
#include <cstring>
#include "gmcore.h"
#include "hostio.h"
extern "C" {
#include "session.h"
#include "hiscreen.h"
}

static gs_session session;
static uint64_t now;
static void pages() {
  for (int i = 0; i < 300 && session.phase == GS_CARDS; i++) {
    gs_event(&session, 1, 57, 1);
    now += 1000000000ull;
    gs_step(&session, now);
  }
}
int main() {
  for (int win = 0; win < 2; win++) {
    now = 0;
    assert(!gs_launch(&session, host_read, (void *)"tests/games/DICE", "DICE.GAM", now));
    pages(); assert(session.phase == GS_MENU && !session.begun);
    gs_event(&session, 1, 28, 1); pages();
    assert(session.phase == GS_PLAY && session.begun);
    gm::World &world = gp_world();
    world.score = 10001;
    if (win) world.won = true;
    else {
      world.lives = 0;
      world.cseq = gm::DIESEQ;
      world.cframe = gm::MAXFRAME - 1;
      world.nextAni = 0;
    }
    now += 1000000000ull; gs_step(&session, now); pages();
    assert(session.phase == GS_SCORES && session.score_check);
    assert(gp_score() == 10001);
    his_table table = {};
    assert(his_insert(&table, 10000, "OLD") == 1);
    hs_screen hs;
    hs_start(&hs, &table, gp_score(), now);
    assert(hs.stage == HS_NAME && hs.place == 1);
    static uint8_t pixels[HS_W * HS_H], palette[768];
    hs_draw_colors(&hs, pixels, palette, nullptr, nullptr, session.colors);
    assert(!memcmp(palette, session.colors, 768));
    hs_event(&hs, 1, 42, 1, now); // Shift
    hs_event(&hs, 1, 30, 1, now); // A
    hs_event(&hs, 1, 48, 1, now); // B
    hs_event(&hs, 1, 14, 1, now); // Backspace
    hs_event(&hs, 1, 46, 1, now); // C
    int result = hs_event(&hs, 1, 28, 1, now);
    assert((result & HS_COMMIT) && hs.stage == HS_TABLE);
    assert(hs.t.row[0].score == 10001 && !strcmp((const char *)hs.t.row[0].name, "AC"));
    assert(hs.t.row[1].score == 10000);
    assert(hs_event(&hs, 1, 57, 1, now) & HS_DONE);
    gs_menu_return(&session, now); pages();
    assert(session.phase == GS_MENU && !session.begun && !session.score_check);
    // Play again resets the score, then menu's Scores choice asks for no name.
    gs_event(&session, 1, 28, 1); pages();
    assert(session.phase == GS_PLAY && gp_score() == 0);
    gs_event(&session, 1, 1, 1); assert(session.phase == GS_SCORES);
    gs_menu_return(&session, now); pages();
    for (int i = 0; i < 4; i++) gs_event(&session, 1, 108, 1);
    gs_event(&session, 1, 28, 1);
    assert(session.phase == GS_SCORES && !session.score_check);
    hs_start(&hs, &hs.t, -1, now); assert(hs.stage == HS_TABLE);
    gs_menu_return(&session, now); pages();
    assert(gs_event(&session, 1, 1, 1) & GS_EXIT);
  }
}
