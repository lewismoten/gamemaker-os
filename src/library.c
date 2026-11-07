/* SPDX-License-Identifier: MIT; original library index reader (library.h):
 * a small JSON reader for one flat object a line. */
#include "library.h"
struct cur {const char *p, *end; int bad;};
static void ws(struct cur *c) {while (c->p < c->end && (*c->p == ' ' || *c->p == '\t' || *c->p == '\r' || *c->p == '\n')) c->p++;}
static int eat(struct cur *c, char ch) {ws(c); if (c->p < c->end && *c->p == ch) {c->p++; return 1;} return 0;}
static int hex(int ch) {return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;}
/* A code point as printable ASCII ("" for nothing): quotes, dashes and Latin-1 letters. */
static const char *fold(uint32_t u) {
  static char one[2];
  static const char *latin = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
  if (u >= 0x20 && u < 0x7f) {one[0] = (char)u; one[1] = 0; return one;}
  if (u == '\t' || u == '\n' || u == '\r' || u == 0xa0) return " ";
  if (u == 0x2018 || u == 0x2019 || u == 0x201a || u == 0x2032) return "'";
  if (u == 0x201c || u == 0x201d || u == 0x201e || u == 0x2033) return "\"";
  if (u == 0x2010 || u == 0x2011 || u == 0x2012 || u == 0x2013 || u == 0x2014 || u == 0x2212) return "-";
  if (u == 0x2026) return "...";
  if (u == 0xb7 || u == 0x2022) return "*";
  if (u == 0xbd) return "1/2";
  if (u == 0x22ef || u == 0x200b || u == 0xfe0f) return "";
  if (u >= 0xc0 && u <= 0xff) {one[0] = latin[u - 0xc0]; one[1] = 0; return one;}
  return "?";
}
/* The next UTF-8 code point of a string's raw bytes (a bad byte is itself). */
static uint32_t utf8(struct cur *c) {
  uint8_t b = (uint8_t)*c->p++;
  int more = b >= 0xf0 ? 3 : b >= 0xe0 ? 2 : b >= 0xc0 ? 1 : 0;
  uint32_t u = more == 3 ? b & 7u : more == 2 ? b & 15u : more == 1 ? b & 31u : b;
  for (; more && c->p < c->end && ((uint8_t)*c->p & 0xc0) == 0x80; more--) u = u << 6 | ((uint8_t)*c->p++ & 63u);
  return more ? '?' : u;
}
/* A JSON string into out (cap characters and a NUL; out may be 0 to skip it). */
static int string(struct cur *c, char *out, uint32_t cap) {
  uint32_t k = 0;
  if (!eat(c, '"')) return c->bad = 1, 0;
  while (c->p < c->end && *c->p != '"') {
    uint32_t u;
    if (*c->p == '\\') {
      c->p++; if (c->p >= c->end) break;
      char e = *c->p++;
      if (e == 'u') {
        u = 0;
        for (int i = 0; i < 4; i++) {int h = c->p < c->end ? hex(*c->p) : -1; if (h < 0) return c->bad = 1, 0; u = u << 4 | (uint32_t)h; c->p++;}
        if (u >= 0xd800 && u < 0xdc00 && c->end - c->p >= 6 && c->p[0] == '\\' && c->p[1] == 'u') {u = '?'; c->p += 6;} /* a pair: outside Latin-1 */
      } else u = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e == 'b' || e == 'f' ? ' ' : (uint8_t)e;
    } else u = utf8(c);
    for (const char *f = fold(u); *f; f++) if (out && k < cap) out[k++] = *f;
  }
  if (out) out[k] = 0;
  if (!eat(c, '"')) return c->bad = 1, 0;
  return 1;
}
static int32_t number(struct cur *c) {
  ws(c); int neg = c->p < c->end && *c->p == '-'; if (neg) c->p++;
  int32_t v = 0; int digits = 0;
  while (c->p < c->end && *c->p >= '0' && *c->p <= '9') {if (v < 100000000) v = v * 10 + (*c->p - '0'); c->p++; digits = 1;}
  while (c->p < c->end && (*c->p == '.' || *c->p == 'e' || *c->p == 'E' || *c->p == '+' || *c->p == '-' || (*c->p >= '0' && *c->p <= '9'))) c->p++;
  if (!digits) c->bad = 1;
  return neg ? -v : v;
}
static void value(struct cur *c, int depth);
static void skip_word(struct cur *c) {ws(c); int n = 0; while (c->p < c->end && *c->p >= 'a' && *c->p <= 'z') {c->p++; n++;} if (!n) c->bad = 1;}
static void value(struct cur *c, int depth) {
  ws(c);
  if (c->p >= c->end || depth > 8) {c->bad = 1; return;}
  char ch = *c->p;
  if (ch == '"') {string(c, 0, 0); return;}
  if (ch == '{' || ch == '[') {
    char close = ch == '{' ? '}' : ']'; c->p++;
    if (eat(c, close)) return;
    do {if (ch == '{') {string(c, 0, 0); if (!eat(c, ':')) c->bad = 1;} value(c, depth + 1);} while (!c->bad && eat(c, ','));
    if (!eat(c, close)) c->bad = 1;
    return;
  }
  if (ch == '-' || (ch >= '0' && ch <= '9')) {number(c); return;}
  skip_word(c);
}
static int same(const char *a, const char *b) {while (*a && *a == *b) {a++; b++;} return *a == *b;}
static void zero(void *d, uint32_t n) {uint8_t *p = d; while (n--) *p++ = 0;}
int gm_record_parse(const char *line, uint32_t n, struct gm_record *r) {
  struct cur c = {line, line + n, 0};
  char key[24], schema[24] = "", crc[12] = "";
  zero(r, sizeof *r);
  if (!eat(&c, '{')) return 0;
  if (!eat(&c, '}')) do {
    if (!string(&c, key, sizeof key - 1) || !eat(&c, ':')) return 0;
    if (same(key, "schema")) string(&c, schema, sizeof schema - 1);
    else if (same(key, "title")) string(&c, r->title, GM_LIB_TITLE);
    else if (same(key, "company")) string(&c, r->company, GM_LIB_COMPANY);
    else if (same(key, "blurb")) string(&c, r->blurb, GM_LIB_BLURB);
    else if (same(key, "dosName")) string(&c, r->dos, 8);
    else if (same(key, "locator")) string(&c, r->locator, GM_LIB_WORD);
    else if (same(key, "gamCrc32")) string(&c, crc, 8);
    else if (same(key, "year")) r->year = number(&c);
    else if (same(key, "course")) r->course = number(&c);
    else if (same(key, "category")) {
      unsigned k = 0;
      if (!eat(&c, '[')) return 0;
      if (!eat(&c, ']')) {
        do {if (k < 2) string(&c, r->category[k++], GM_LIB_WORD); else value(&c, 1);} while (!c.bad && eat(&c, ','));
        if (!eat(&c, ']')) return 0;
      }
    } else value(&c, 1);
    if (c.bad) return 0;
  } while (eat(&c, ','));
  if (c.bad || !eat(&c, '}') || !same(schema, "gmp-library") || !r->title[0]) return 0;
  uint32_t v = 0; int digits = 0;
  for (const char *p = crc; *p; p++) {int h = hex(*p); if (h < 0) {digits = 0; break;} v = v << 4 | (uint32_t)h; digits++;}
  r->crc = v; r->has_crc = digits == 8;
  return 1;
}
uint32_t gm_crc32(uint32_t crc, const uint8_t *d, uint32_t n) {
  crc = ~crc;
  while (n--) {crc ^= *d++; for (int k = 0; k < 8; k++) crc = crc >> 1 ^ (0xedb88320u & (0u - (crc & 1)));}
  return ~crc;
}
