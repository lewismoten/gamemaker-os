// SPDX-License-Identifier: MIT; original loader of a game's DOS files for the
// shared play core (game.h). The .GAM layout is version 3.00's (GRATOR.H:
// a 256-byte header, NumScenes, 283-byte scenes, a pool of file names), as
// Game-Maker: Sandbox Edition's formats/game.ts reads it; a level is a scene
// of type 10, and its links, start and scroll limits are read the way that
// project's Pocket Player packer (web/src/export/pocketPack.ts) turns them
// into the core's level records. An older .GAM (20 fixed 330-byte scenes,
// names without extensions) is first converted to version 3.00 bytes the way
// that project's parseGame and serializeGame do it, after GameClass::Load.
// Every buffer is static: no allocator.
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
constexpr int F_PAL = 0, F_MAP = 1, F_BBL = 2, F_MON = 3, F_MBL = 4, F_CHR = 5, F_CBL = 6, F_SND = 7, F_SONG = 8, F_INTRO = 9, F_EXIT = 10;

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
// A hero position wraps by the map size, as PLAYGAME does (PLAYBACK.C).
int wrapX(int v) { return ((v % gm::MLEN) + gm::MLEN) % gm::MLEN; }
int wrapY(int v) { return ((v % gm::MWID) + gm::MWID) % gm::MWID; }
uint32_t sceneAt(int s) { return HEAD + (uint32_t)s * SCENE; }
// File slot `f` of scene `s`: a name in the pool, "" when empty.
const char *fileOf(int s, int f) {
  uint32_t pool = sceneAt(nscenes), len = (uint32_t)u16(pool);
  uint32_t o = (uint32_t)u16(sceneAt(s) + 43 + 2 * f);
  if (!o || o >= len || pool + 2 + o >= gamN) return "";
  const char *name = (const char *)gam + pool + 2 + o;
  for (uint32_t i = pool + 2 + o; i < gamN; i++)
    if (!gam[i]) return name;
  return "";  // an unterminated name is no name
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

// Pre-3.00 files (OLDintegratorstruct in GRATOR.H, 330 bytes a scene): gametype u16, x, y i16,
// desc[15], fnames[9][10] (no extensions), links[20] (fx, fy, tnum, tx, ty; tnum 20 = unused),
// red, green, blue, ssup, ssdown, ssleft, ssright, PinChar, extra[11].
constexpr int OLD_SCENE = 330, OLD_MAX = 20;
uint8_t oldGam[OLD_SCENE * OLD_MAX];
// JavaScript's toLowerCase on a Latin-1 byte, as parseOld lowers the names.
uint8_t lowerLatin1(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7) ? c + 32 : c; }
struct Out {
  uint32_t n = 0;
  void u8(int v) { if (n < GAM_CAP) gam[n++] = (uint8_t)v; }
  void u16(int v) { u8(v & 255); u8(v >> 8 & 255); }
};
// Builds the version 3.00 bytes of an older file into gam (buildPool: offset 0 is "", names are shared).
uint32_t convertOld(const uint8_t *d, uint32_t n) {
  int count = (int)(n / OLD_SCENE);
  if (count > OLD_MAX) count = OLD_MAX;
  static char names[OLD_MAX][20][16];
  static const char *const ext[9] = {".pal", ".map", ".bbl", ".mon", ".mbl", ".chr", ".cbl", ".snd", ".cmf"};
  for (int s = 0; s < count; s++) {
    const uint8_t *r = d + s * OLD_SCENE;
    for (int f = 0; f < 20; f++) names[s][f][0] = 0;
    auto file = [&](int oldIdx, int newIdx, const char *e) {
      const uint8_t *src = r + 21 + 10 * oldIdx;
      int k = 0;
      while (k < 10 && src[k]) { names[s][newIdx][k] = (char)lowerLatin1(src[k]); k++; }
      if (!k) return;
      for (int i = 0; e[i]; i++) names[s][newIdx][k++] = e[i];
      names[s][newIdx][k] = 0;
    };
    if (s == 0) { file(0, 2, ".bkd"); file(1, 4, ".txt"); file(2, 6, ".txt"); file(3, 7, ".txt"); file(4, 3, ".cmf"); }
    else if (s == 1) { file(0, 0, ".bkd"); file(3, 1, ".cmf"); }
    else for (int j = 0; j < 9; j++) file(j, j, ext[j]);
  }
  // The pool, as SceneFiles::Add fills it.
  static uint8_t pool[OLD_MAX * 20 * 16 + 1];
  static uint16_t offs[OLD_MAX][20];
  uint32_t plen = 1; pool[0] = 0;
  for (int s = 0; s < count; s++)
    for (int f = 0; f < 20; f++) {
      const char *nm = names[s][f];
      offs[s][f] = 0;
      if (!nm[0]) continue;
      for (int t = 0; t <= s && !offs[s][f]; t++)
        for (int g = 0; g < (t == s ? f : 20) && !offs[s][f]; g++)
          if (names[t][g][0] && same(names[t][g], nm)) offs[s][f] = offs[t][g];
      if (offs[s][f]) continue;
      offs[s][f] = (uint16_t)plen;
      for (int i = 0; nm[i]; i++) pool[plen++] = (uint8_t)nm[i];
      pool[plen++] = 0;
    }
  Out o;
  o.u8('G'); o.u8('M'); o.u16(300); o.u8(0); o.u8(8);  // DataFileHeader: version 300, FileType GamFile
  for (int i = 0; i < 250; i++) o.u8("RSD"[i % 3]);
  o.u16(count); o.u16(20); o.u16(20);
  for (int s = 0; s < count; s++) {
    const uint8_t *r = d + s * OLD_SCENE;
    int type = r[0] | r[1] << 8;
    o.u16(type == 1 || type == 2 || type == ADVTYPE ? type : NOSCENE);
    o.u16(r[2] | r[3] << 8); o.u16(r[4] | r[5] << 8);
    const uint8_t *ss = r + 21 + 90 + 200 + 3;  // ssup, ssdown, ssleft, ssright
    o.u8(ss[0]); o.u8(ss[1]); o.u8(ss[2]); o.u8(ss[3]); o.u8(0);  // Style FALSE
    for (int i = 0; i < 11; i++) o.u8(0);
    o.u8(20);  // NumLinks
    int dl = 0;
    while (dl < 15 && r[6 + dl]) dl++;
    for (int i = 0; i < 20; i++) o.u8(i < dl && i < 19 ? r[6 + i] : 0);
    for (int f = 0; f < 20; f++) o.u16(offs[s][f]);
    for (int l = 0; l < 20; l++) {
      const uint8_t *k = r + 111 + 10 * l;
      int fx = k[0] | k[1] << 8, fy = k[2] | k[3] << 8, tnum = (int16_t)(k[4] | k[5] << 8), tx = k[6] | k[7] << 8, ty = k[8] | k[9] << 8;
      o.u16(tnum == OLD_MAX ? NOSCENE : tnum & 0xffff); o.u16(fx); o.u16(fy); o.u16(tx); o.u16(ty);
    }
  }
  o.u16((int)plen);
  for (uint32_t i = 0; i < plen; i++) o.u8(pool[i]);
  return o.n;
}

// The core's LevelInfo: start (the first level only: scene 0's first link), scroll limits and links.
bool info(void *, int level, gm::Level &out) {
  if (level < 0 || level >= nplay) return false;
  uint32_t s = sceneAt(play[level]);
  out.startX = level == 0 ? wrapX(i16(sceneAt(0) + 83 + 6)) : 0;
  out.startY = level == 0 ? wrapY(i16(sceneAt(0) + 83 + 8)) : 0;
  out.scrollUp = gam[s + 6]; out.scrollDown = gam[s + 7]; out.scrollLeft = gam[s + 8]; out.scrollRight = gam[s + 9];
  out.nlinks = 0;
  for (int l = 0; l < 20 && out.nlinks < gm::MAXLINKS; l++) {
    uint32_t k = s + 83 + 10 * (uint32_t)l;
    int to = u16(k);
    if (to == NOSCENE) continue;
    int lv = levelOf(to);
    if (lv == -2) continue;  // a link to a scene that is not a level (the Pocket Player leaves such a game out)
    out.links[out.nlinks++] = {i16(k + 2), i16(k + 4), lv, wrapX(i16(k + 6)), wrapY(i16(k + 8))};
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
  if (n >= 2 && !(gam[0] == 'G' && gam[1] == 'M')) {  // an older file: GameClass::Load converts it
    uint32_t k = (uint32_t)n < sizeof oldGam ? (uint32_t)n : sizeof oldGam;
    for (uint32_t i = 0; i < k; i++) oldGam[i] = gam[i];
    if (k < (uint32_t)OLD_SCENE) return -1;
    n = (int32_t)convertOld(oldGam, k);
  }
  if (n < HEAD) return -1;
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
    case GP_MENU: return fileOf(0, 2);
    case GP_SCORES: return fileOf(0, 8); // the high score screen's backdrop
  }
  return "";
}
extern "C" int gp_take_sound(void) {
  if (!w || !w->nsnd) return -1;
  int n = w->sndQ[w->nsnd - 1];
  w->nsnd = 0;
  return n;
}
extern "C" const char *gp_sound_file(void) { return w && w->snd ? sndName : ""; }
// The .snd's 30 clip names (9 bytes each) follow its 30 x 72 bytes of PC speaker tunes (code/GM/SOUND.H).
extern "C" const char *gp_voc_name(int n) {
  static char v[10];
  v[0] = 0;
  if (!w || !w->snd || n < 0 || n >= gm::LASTSND || sndN < gm::LASTSND * 72 + (n + 1) * 9) return v;
  const uint8_t *at = snd + gm::LASTSND * 72 + n * 9;
  int k = 0;
  while (k < 9 && at[k]) { v[k] = (char)at[k]; k++; }
  v[k] = 0;
  if (v[0] == ' ') v[0] = 0;
  return v;
}
extern "C" const char *gp_song(int slot, int level) {
  switch (slot) {
    case GP_SONG_TITLE: return fileOf(0, 1);
    case GP_SONG_WON: return fileOf(1, 1);
    case GP_SONG_LOST: return fileOf(1, 3);
    case GP_SONG_LEVEL: return level >= 0 && level < nplay ? fileOf(play[level], F_SONG) : "";
  }
  return "";
}
extern "C" const char *gp_demo(void) { return fileOf(0, 11); }
extern "C" const uint8_t *gp_gam_bytes(uint32_t *n) {
  *n = gamN;
  return gam;
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
extern "C" int32_t gp_score(void) { return w ? w->score : 0; }
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
