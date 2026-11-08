/* SPDX-License-Identifier: MIT; gmp.h: a .GMP's DOS files, read through the
 * Pocket Player's bundle index (gmsmall.h) and inflated by puff. */
#include "gmp.h"
#include "gmsmall.h"
#include "asset_scratch.h"
#define packed ga_packed
#define whole ga_whole
extern "C" {
#include "puff.h"
}
namespace {

bool pak_read(void *user, uint32_t off, void *buf, uint32_t n) {return gmp_read_at((const struct gmp *)user, off, buf, n) == 0;}
gm::SmallPak pak_of(const struct gmp *g) {
  gm::SmallPak p; p.user = (void *)g; p.readAt = pak_read; p.size = g->total; p.count = g->entries; return p;
}
uint32_t le32(const uint8_t *b) {return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24;}
bool is_gam(const char *n) { /* "dos/" then 1 to 8 characters and ".GAM" */
  uint32_t k = 0; while (n[k]) k++;
  return k >= 9 && k <= 16 && n[0] == 'd' && n[1] == 'o' && n[2] == 's' && n[3] == '/' &&
         n[k - 4] == '.' && n[k - 3] == 'G' && n[k - 2] == 'A' && n[k - 1] == 'M';
}
}  // namespace
extern "C" int gmp_read_at(const struct gmp *g, uint32_t off, void *buf, uint32_t n) {
  uint8_t *out = (uint8_t *)buf; uint32_t base = 0;
  if (off > g->total || n > g->total - off) return -1;
  for (unsigned v = 0; v < g->volumes && n; v++) {
    uint32_t sz = g->size[v];
    if (off < base + sz) { /* this volume holds the next bytes */
      uint32_t at = off - base, take = sz - at < n ? sz - at : n;
      if (g->read_at(g->user, v, at, out, take)) return -1;
      out += take; off += take; n -= take;
    }
    base += sz;
  }
  return n ? -1 : 0;
}
extern "C" int gmp_open(struct gmp *g) {
  g->total = 0; g->entries = 0; g->gam[0] = 0; g->pocket = 0;
  if (!g->read_at || !g->volumes || g->volumes > GMP_VOLUMES_MAX) return -1;
  for (unsigned v = 0; v < g->volumes; v++) {
    if (g->size[v] > 0xffffffffu - g->total) return -1;
    g->total += g->size[v];
  }
  gm::SmallPak p = pak_of(g);
  if (!p.begin() || p.count > GMP_ENTRIES_MAX) return -1;
  g->entries = p.count;
  uint32_t off, len;
  g->pocket = p.find("game.txt", &off, &len);
  for (uint32_t i = 0; i < g->entries && !g->gam[0]; i++) { /* entries are sorted: the first dos/ *.GAM */
    gm::SmallEntry e;
    if (gmp_read_at(g, 8 + i * 40, &e, sizeof e)) return -1;
    e.name[31] = 0;
    if (!is_gam(e.name) || !p.find(e.name, &off, &len)) continue;
    for (int k = 0; k < 13; k++) g->gam[k] = e.name[4 + k];
  }
  return g->gam[0] ? 0 : -1;
}
extern "C" int32_t gmp_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {
  const struct gmp *g = (const struct gmp *)user;
  char key[32] = "dos/"; uint32_t k = 4;
  for (; name[k - 4]; k++) {
    if (k >= sizeof key - 1) return -1;
    char c = name[k - 4]; key[k] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
  }
  key[k] = 0;
  gm::SmallPak p = pak_of(g);
  uint32_t off, len;
  if (!p.find(key, &off, &len) || len < 4 || len > sizeof packed) return -1;
  if (gmp_read_at(g, off, packed, len)) return -1;
  uint32_t raw = le32(packed);
  if (raw > sizeof whole) return -1;
  uint8_t *dest = cap >= raw ? buf : whole;
  unsigned long dn = raw, sn = len - 4;
  if (len == 4 ? raw != 0 : puff(dest, &dn, packed + 4, &sn) || dn != raw) return -1;
  uint32_t n = raw < cap ? raw : cap;
  if (dest == whole) for (uint32_t i = 0; i < n; i++) buf[i] = whole[i];
  return (int32_t)n;
}
extern "C" int32_t gmp_entry(const struct gmp *g, const char *name, uint8_t *buf, uint32_t cap) {
  gm::SmallPak p = pak_of(g);
  uint32_t off, len;
  if (!p.find(name, &off, &len)) return -1;
  uint32_t n = len < cap ? len : cap;
  return gmp_read_at(g, off, buf, n) ? -1 : (int32_t)n;
}
extern "C" int gmp_volume_name(const char *base, unsigned volume, char out[13]) {
  unsigned k = 0;
  while (base[k] && base[k] != '.') {if (k == 8) return -1; out[k] = base[k]; k++;}
  if (!k || volume >= GMP_VOLUMES_MAX || volume > 99) return -1;
  out[k++] = '.'; out[k++] = 'G';
  if (!volume) {out[k++] = 'M'; out[k++] = 'P';}
  else {out[k++] = (char)('0' + volume / 10); out[k++] = (char)('0' + volume % 10);}
  out[k] = 0;
  return 0;
}
