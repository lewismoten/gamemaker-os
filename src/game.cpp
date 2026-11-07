// SPDX-License-Identifier: MIT; original loader of a game's DOS files for the
// shared play core (game.h). The .GAM layout is version 3.00's (GRATOR.H:
// a 256-byte header, NumScenes, 283-byte scenes, a pool of file names), as
// Game-Maker: Sandbox Edition's formats/game.ts reads it; a level is a scene
// of type 10, and its links, start and scroll limits are read the way that
// project's Pocket Player packer (web/src/export/pocketPack.ts) turns them
// into the core's level records. Every buffer is static: no allocator.
#include <new>
#include <stdlib.h>
#include "game.h"
// A freestanding C++ build declares no abs(); the board gets it from src/libc.c.
extern "C" int abs(int);
#include "gmcore.h"

namespace {
using gm::BLOCK_BYTES;
constexpr uint32_t GAM_CAP = 65536, MAP_BYTES = gm::MLEN * gm::MWID * 2 + gm::LASTMM * 10;
constexpr int SCENE = 283, HEAD = 262, NOSCENE = 0xffff, ADVTYPE = 10;
constexpr int F_PAL = 0, F_MAP = 1, F_BBL = 2, F_MON = 3, F_MBL = 4, F_CHR = 5, F_CBL = 6, F_SND = 7, F_INTRO = 9, F_EXIT = 10;

gp_read_fn reader;
void *readerUser;
uint8_t gam[GAM_CAP];
uint32_t gamN;
int nscenes, nplay, play[gm::MAXLEVELS];
alignas(gm::World) uint8_t worldMem[sizeof(gm::World)];
gm::World *w;
uint8_t mapBuf[MAP_BYTES];
uint8_t bbl[gm::BACKBL * BLOCK_BYTES], cbl[gm::CHARBL * BLOCK_BYTES], chr[8192], mon[gm::LASTMON * gm::MONSTER_BYTES], mbl[gm::MONBL * BLOCK_BYTES];
uint8_t snd[4096], pal[768];
char bblName[16], cblName[16], chrName[16], monName[16], mblName[16], sndName[16], palName[16];
int nbl, nmon, nmbl;
bool haveHero;

int u16(uint32_t at) { return at + 1 < gamN ? gam[at] | gam[at + 1] << 8 : 0; }
int i16(uint32_t at) { return (int16_t)u16(at); }
uint32_t sceneAt(int s) { return HEAD + (uint32_t)s * SCENE; }
// File slot `f` of scene `s`: a name in the pool, "" when empty.
const char *fileOf(int s, int f) {
  uint32_t pool = sceneAt(nscenes), len = (uint32_t)u16(pool);
  uint32_t o = (uint32_t)u16(sceneAt(s) + 43 + 2 * f);
  if (!o || o >= len || pool + 2 + o >= gamN) return "";
  return (const char *)gam + pool + 2 + o;
}
bool same(const char *a, const char *b) {
  while (*a && *a == *b) a++, b++;
  return *a == *b;
}
void copyName(char *d, const char *s) {
  unsigned i = 0;
  for (; s[i] && i < 15; i++) d[i] = s[i];
  d[i] = 0;
}
// Reads `name` into buf unless it is already there (kept when the next level uses the same file). Bytes, or -1.
int32_t load(const char *name, uint8_t *buf, uint32_t cap, char *have, int32_t *size) {
  if (have[0] && same(have, name)) return *size;
  have[0] = 0;
  int32_t n = reader(readerUser, name, buf, cap);
  if (n < 0) return -1;
  copyName(have, name);
  *size = n;
  return n;
}
int32_t bblN, cblN, chrN, monN, mblN, sndN, palN;
int levelOf(int scene) {
  if (scene == 1) return -1;
  for (int i = 0; i < nplay; i++)
    if (play[i] == scene) return i;
  return -2;
}

// The core's LevelInfo: start (the first level only: scene 0's first link), scroll limits and links.
bool info(void *, int level, gm::Level &out) {
  if (level < 0 || level >= nplay) return false;
  uint32_t s = sceneAt(play[level]);
  out.startX = level == 0 ? i16(sceneAt(0) + 83 + 6) : 0;
  out.startY = level == 0 ? i16(sceneAt(0) + 83 + 8) : 0;
  out.scrollUp = gam[s + 6]; out.scrollDown = gam[s + 7]; out.scrollLeft = gam[s + 8]; out.scrollRight = gam[s + 9];
  out.nlinks = 0;
  for (int l = 0; l < 20 && out.nlinks < gm::MAXLINKS; l++) {
    uint32_t k = s + 83 + 10 * (uint32_t)l;
    int to = u16(k);
    if (to == NOSCENE) continue;
    int lv = levelOf(to);
    if (lv == -2) continue;  // a link to a scene that is not a level (the Pocket Player leaves such a game out)
    out.links[out.nlinks++] = {i16(k + 2), i16(k + 4), lv, i16(k + 6), i16(k + 8)};
  }
  return true;
}

// The core's MapLoader: the level's map, blocks, hero, monsters, palette and sounds.
bool loadMap(void *, int level, uint8_t *blocks, uint8_t *mons, uint8_t *mm, bool *hasMons) {
  if (level < 0 || level >= nplay) return false;
  int s = play[level];
  if (reader(readerUser, fileOf(s, F_MAP), mapBuf, MAP_BYTES) != (int32_t)MAP_BYTES) return false;
  const int cells = gm::MLEN * gm::MWID;
  for (int i = 0; i < cells; i++) blocks[i] = mapBuf[2 * i];
  if (load(fileOf(s, F_BBL), bbl, sizeof bbl, bblName, &bblN) < BLOCK_BYTES) return false;
  nbl = bblN / BLOCK_BYTES;
  // A level with no character files keeps the previous level's hero, as the engine does.
  const char *c = fileOf(s, F_CHR), *cb = fileOf(s, F_CBL);
  if (c[0] && cb[0]) {
    if (load(c, chr, sizeof chr, chrName, &chrN) < 3784 || load(cb, cbl, sizeof cbl, cblName, &cblN) < BLOCK_BYTES) return false;
    haveHero = true;
  } else if (!haveHero)
    return false;
  nmon = nmbl = 0;
  const char *mo = fileOf(s, F_MON), *mb = fileOf(s, F_MBL);
  if (mo[0] && mb[0] && load(mo, mon, sizeof mon, monName, &monN) >= gm::MONSTER_BYTES && load(mb, mbl, sizeof mbl, mblName, &mblN) >= BLOCK_BYTES) {
    nmon = monN / gm::MONSTER_BYTES;
    nmbl = mblN / BLOCK_BYTES;
  }
  *hasMons = nmon > 0 && nmbl > 0;
  if (*hasMons) {
    for (int i = 0; i < cells; i++) mons[i] = mapBuf[2 * i + 1];
    for (int l = 0; l < gm::LASTMM; l++) {
      const uint8_t *p = mapBuf + 2 * cells + 10 * l;  // monnum u8, status i16, xtra u8, curx i16, cury i16, extra i16
      mm[3 * l] = p[0]; mm[3 * l + 1] = p[4]; mm[3 * l + 2] = p[6];
    }
  }
  if (load(fileOf(s, F_PAL), pal, sizeof pal, palName, &palN) != 768) return false;
  const char *sn = fileOf(s, F_SND);
  w->snd = sn[0] && load(sn, snd, sizeof snd, sndName, &sndN) >= gm::LASTSND * 72 ? snd : nullptr;
  w->bbl = bbl; w->cbl = cbl; w->chr = chr; w->nbl = nbl;
  w->mon = *hasMons ? mon : nullptr; w->mbl = *hasMons ? mbl : nullptr;
  w->nmon = *hasMons ? nmon : 0; w->nmbl = *hasMons ? nmbl : 0;
  return true;
}
}  // namespace

extern "C" int gp_open(gp_read_fn read, void *user, const char *gam_name) {
  reader = read; readerUser = user;
  bblName[0] = cblName[0] = chrName[0] = monName[0] = mblName[0] = sndName[0] = palName[0] = 0;
  haveHero = false;
  int32_t n = read(user, gam_name, gam, GAM_CAP);
  if (n < HEAD || gam[0] != 'G' || gam[1] != 'M') return -1;  // version 3.00 files only, for now
  gamN = (uint32_t)n;
  nscenes = i16(256);
  if (nscenes < 2 || sceneAt(nscenes) + 2 > gamN) return -1;
  nplay = 0;
  for (int s = 0; s < nscenes && nplay < gm::MAXLEVELS; s++)
    if (u16(sceneAt(s)) == ADVTYPE) play[nplay++] = s;
  w = new (worldMem) gm::World();
  w->loadMap = loadMap; w->loadInfo = info;
  w->nlevels = nplay;
  w->hold = true;
  return nplay ? nplay : -1;
}
extern "C" const char *gp_graphic(int slot, int level) {
  switch (slot) {
    case GP_TITLE: return fileOf(0, 0);
    case GP_WON: return fileOf(1, 0);
    case GP_LOST: return fileOf(1, 2);
    case GP_INTRO: return level >= 0 && level < nplay ? fileOf(play[level], F_INTRO) : "";
    case GP_EXIT: return level >= 0 && level < nplay ? fileOf(play[level], F_EXIT) : "";
  }
  return "";
}
extern "C" int gp_begin(void) { return w && w->begin() ? 0 : -1; }
extern "C" void gp_key(int scan, int down) { w->key(scan, down != 0); }
extern "C" void gp_release_all(void) { w->releaseAll(); }
extern "C" char gp_tick(void) { return w->tick(); }
extern "C" int gp_finish(void) {
  bool ok = w->finish();
  w->buildDrawList();
  return ok ? 0 : -1;
}
extern "C" int gp_level(void) { return w->level; }
extern "C" int gp_lives(void) { return w->lives; }
extern "C" int gp_pending_level(void) { return w->pendLevel; }
extern "C" void gp_render(uint8_t *px) {
  for (uint32_t y = 0; y < GP_H; y++) {
    uint8_t *row = px + y * GP_W;
    if (y < GP_VIEW_H) {
      w->renderRow((int)y, row);
      for (uint32_t x = GP_VIEW_W; x < GP_W; x++) row[x] = 0;
    } else
      for (uint32_t x = 0; x < GP_W; x++) row[x] = 0;
  }
}
extern "C" const uint8_t *gp_palette(void) { return pal; }
extern "C" uint32_t gp_frame_hash(void) {
  uint32_t h = 2166136261u;
  uint8_t row[GP_W];
  for (uint32_t y = 0; y < GP_VIEW_H; y++) {
    w->renderRow((int)y, row);
    for (uint32_t x = 0; x < GP_VIEW_W; x++) h = (h ^ row[x]) * 16777619u;
  }
  return h;
}
// The world itself, for the host tests' state lines (game_host.h).
gm::World &gp_world() { return *w; }
