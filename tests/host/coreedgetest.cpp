// SPDX-License-Identifier: MIT
// Relevant regressions ported from gamemaker-pocket host/coreedgetest.cpp at
// 33c2d7724126ab9ca2307e8fbd63c4cb7e5f9fcb. Synthetic assets exercise the
// vendored core directly, including coordinates the DOS loader normalizes.
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <initializer_list>
#include "gmcore.h"

using namespace gm;
static int failures = 0, checks = 0;
static void check(bool ok, const char* name) {
  ++checks;
  if (!ok) { ++failures; printf("FAIL %s\n", name); }
}
static void put16(uint8_t* p, int v) {
  p[0] = uint8_t(v); p[1] = uint8_t(unsigned(v) >> 8);
}
struct Fixture {
  std::unique_ptr<World> w = std::make_unique<World>();
  uint8_t bbl[BLOCK_BYTES] = {}, cbl[CHARBL * BLOCK_BYTES], chr[3784] = {};
  uint8_t mon[LASTMON * MONSTER_BYTES] = {}, mbl[MONBL * BLOCK_BYTES];
  Level level = {};
  Fixture() {
    memset(cbl, 255, sizeof cbl); memset(mbl, 255, sizeof mbl);
    for (int s = 0; s < MAXSEQ; ++s) {
      for (int f = 0; f < MAXFRAME; ++f) {
        auto p = chr + s * SEQ_BYTES + f * FRAME_BYTES;
        put16(p + 6, CHARBL); put16(p + 8, CHARBL); put16(p + 10, 1);
      }
      chr[s * SEQ_BYTES + 172] = 255;
      chr[s * SEQ_BYTES + 174] = BACKBL;
    }
    for (int i = 0; i < MAXINV; ++i) chr[3714 + i] = BACKBL;
    for (int i = 0; i < 30; ++i) put16(chr + 3724 + 2 * i, 100);
    for (int i = 0; i < LASTMON; ++i) mon[i * MONSTER_BYTES + 98] = LASTMON;
    w->bbl = bbl; w->cbl = cbl; w->chr = chr; w->nbl = 1;
    w->mon = mon; w->mbl = mbl; w->nmon = LASTMON; w->nmbl = MONBL;
    w->nlevels = 1; w->infoUser = this;
    w->loadInfo = [](void* user, int, Level& out) {
      out = static_cast<Fixture*>(user)->level; return true;
    };
    w->loadMap = [](void*, int, uint8_t* blocks, uint8_t*, uint8_t*, bool* hasMons) {
      memset(blocks, 0, MLEN * MWID); *hasMons = false; return true;
    };
  }
  bool begin() { return w->begin(); }
};

static int wrap(int n, int size) { return (n % size + size) % size; }
static void entries() {
  Fixture f;
  auto& w = *f.w;
  // Damaged entries must be normalized before scrolling or direct map writes.
  for (int v : {INT_MIN, -32768, -101, -1, 100, 200, 32767, INT_MAX}) {
    f.level.startX = v; f.level.startY = -1;
    w.lvlIdx = -1;
    check(f.begin() && w.cx[0] == wrap(v, MLEN) && w.cy[0] == MWID - 1,
          "damaged configured start wraps before use");
    check(w.enterLevel(0, v, v), "damaged coordinate fixture enters");
    check(w.cx[0] == wrap(v, MLEN) && w.cy[0] == wrap(v, MWID), "damaged entry immediately wraps");
    check(w.mx == wrap(w.cx[0] - SCRNBLEN / 2, MLEN) &&
          w.my == wrap(w.cy[0] - SCRNBWID / 2, MWID), "damaged entry centers bounded viewport");
    Touch t = w.getTouch();
    check(t.bx[2] == wrap(v, MLEN) && t.by[2] == wrap(v, MWID), "damaged coordinates have bounded touch indices");
    w.dropSlot = 0; w.inv[0] = 42;
    check(w.dropBlock() && w.map[wrap(v, MWID) * MLEN + wrap(v, MLEN)] == 42, "damaged entry permits direct map write");
    w.cx[0] = w.cy[0] = v;
    w.chkMapBounds();
    check(w.cx[0] == wrap(v, MLEN) && w.cy[0] == wrap(v, MWID), "damaged coordinates wrap by full map size");
  }
}

static void links() {
  for (int v : {-32768, -101, -1, 100, 200, 32767}) {
    Fixture f;
    f.level.startX = f.level.startY = 50;
    f.level.nlinks = 1;
    f.level.links[0] = {50, 50, 0, v, v};
    check(f.begin(), "damaged link fixture loads");
    f.w->hold = true;
    check(f.w->tick() == 'L' && f.w->finish(), "damaged link transition reports loaded");
    check(f.w->cx[0] == wrap(v, MLEN) && f.w->cy[0] == wrap(v, MWID),
          "damaged link destination wraps by map size");
  }
}
static void blockScores() {
  Fixture f; check(f.begin(), "block score fixture loads");
  auto& w = *f.w;
  Attr a = {};
  for (int delta : {-32768, -1, 0, 1, 32767}) {
    for (int start : {INT_MIN, INT_MIN + 1, -32768, 0, 32767, INT_MAX - 1, INT_MAX}) {
      w.score = start; a.scorePts = delta;
      int64_t sum = int64_t(start) + delta;
      int expected = sum < INT_MIN ? INT_MIN : sum > INT_MAX ? INT_MAX : int(sum);
      check(w.doBlockTouchChar(a) == 1 && w.score == expected, "block score clamps only outside int32 range");
    }
  }
}
static void monsterScores() {
  Fixture f; check(f.begin(), "monster score fixture loads");
  auto& w = *f.w;
  memset(f.mbl, 0, BLOCK_BYTES);  // Opaque, overlapping pictures ensure collision.
  for (int stronger : {0, 1}) for (int delta : {-32768, -1, 0, 1, 32767}) {
    for (int start : {INT_MIN, INT_MIN + 32768, 0, INT_MAX - 32767, INT_MAX}) {
      for (int m = 0; m < 2; ++m) {
        uint8_t* mo = f.mon + m * MONSTER_BYTES;
        mo[8] = m == stronger ? 2 : 1;
        mo[99] = uint8_t(delta); mo[100] = uint8_t(unsigned(delta) >> 8);
        w.mm[m] = {uint8_t(m), 1, 1};
        w.resetMonInfo(m); w.mi[m].curPic = 0;
        w.mi[m].fromchar = m == stronger ? 1 : 0;
      }
      w.score = start;
      int64_t sum = int64_t(start) + delta;
      int expected = sum < INT_MIN ? INT_MIN : sum > INT_MAX ? INT_MAX : int(sum);
      check(w.doCollision(0, 1, 0) == LASTMM && w.score == expected &&
            w.mm[0].monnum == LASTMON && w.mm[1].monnum == LASTMON,
            "both monster score paths clamp only outside int32 range");
    }
  }
}

int main() {
  entries(); links(); blockScores(); monsterScores();
  printf("coreedgetest: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
