/* SPDX-License-Identifier: MIT; title screens for the games list (thumb.h):
 * a C port of the Sandbox Edition's titlePicture.ts choices, drawn with this
 * program's GIF and FLI decoders. */
#include "thumb.h"
#include "fli.h"
#include "gif.h"
#include "rgb565.h"
#define W 320u
#define H 200u
extern uint8_t gp_file_buf[1024 * 1024]; /* session.c: one 1 MiB read buffer for both */
#define file gp_file_buf
static uint8_t list[4096], frame[W * H], frame_pal[768];
static uint16_t cache[GM_THUMB_SLOTS][GM_THUMB_W * GM_THUMB_H];
static uint32_t used[GM_THUMB_SLOTS], clock;
static int16_t owner[GM_THUMB_SLOTS];
static int owners_set;
static int lower(int c) {return c >= 'A' && c <= 'Z' ? c + 32 : c;}
static const char *ext(const char *name) {const char *d = ""; for (; *name; name++) if (*name == '.') d = name; return d;}
static int is(const char *a, const char *b) {while (*a && lower(*a) == *b) {a++; b++;} return !*a && !*b;}
static void copy(uint8_t *d, const uint8_t *s, uint32_t n) {while (n--) *d++ = *s++;}
static void zero(uint8_t *d, uint32_t n) {while (n--) *d++ = 0;}
/* GMTITLE.GIF and its kin, AJGAMES, SANDLOGO and BARD (.GIF or .FLI): the same on many games. */
static int splash(const char *name) {
  char base[16]; unsigned k = 0;
  while (name[k] && name[k] != '.' && k < 15) {base[k] = (char)lower(name[k]); k++;}
  base[k] = 0;
  const char *e = ext(name);
  if (!is(e, ".gif") && !is(e, ".fli")) return 0;
  if (is(base, "ajgames") || is(base, "sandlogo") || is(base, "bard")) return 1;
  if (k < 7 || !(base[0] == 'g' && base[1] == 'm' && base[2] == 't' && base[3] == 'i' && base[4] == 't' && base[5] == 'l' && base[6] == 'e')) return 0;
  for (unsigned i = 7; i < k; i++) if (base[i] < '0' || base[i] > '9') return 0;
  return 1;
}
/* Nearly every pixel one color (more than 97 %). */
static int blank(const uint8_t *px) {
  static uint32_t counts[256];
  for (unsigned i = 0; i < 256; i++) counts[i] = 0;
  uint32_t top = 0;
  for (uint32_t i = 0; i < W * H; i++) if (++counts[px[i]] > top) top = counts[px[i]];
  return top * 100u > W * H * 97u;
}
/* levelPicture.ts busyness: distinct colors (up to 40) times the share not of the commonest, on every 4th pixel; x1000. */
static uint32_t busyness(const uint8_t *px, const uint8_t *pal) {
  static uint32_t keys[256], counts[256]; /* an indexed picture has at most 256 colors */
  uint32_t n = 0, top = 0, total = 0;
  for (uint32_t i = 0; i < W * H; i += 4, total++) {
    const uint8_t *c = pal + px[i] * 3u; uint32_t k = (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2], j = 0;
    while (j < n && keys[j] != k) j++;
    if (j == n) {keys[n] = k; counts[n++] = 0;}
    if (++counts[j] > top) top = counts[j];
  }
  return (n < 40 ? n : 40) * (1000u - top * 1000u / total);
}
static int gif_at(int32_t n, int32_t x, int32_t y, uint8_t *px, uint8_t *pal) {
  zero(px, W * H);
  return n > 0 && !gp_gif_draw(file, (uint32_t)n, x, y, pal, px) ? 0 : -1;
}
/* An animation's last frame, or its most colorful when the last is under 80 % as busy. */
static int fli_pic(int32_t n, uint8_t *px, uint8_t *pal) {
  struct gp_fli f;
  if (n <= 0 || gp_fli_open(&f, file, (uint32_t)n)) return -1;
  zero(frame, sizeof frame); zero(frame_pal, sizeof frame_pal);
  uint32_t best_busy = 0; int have = 0;
  for (uint32_t k = 0; k < f.frames; k++) {
    static uint8_t canvas[GP_FLI_PIXELS];
    if (!k) zero(canvas, sizeof canvas);
    gp_fli_frame(&f, canvas, frame_pal);
    zero(frame, sizeof frame);
    for (uint32_t y = 0; y < H && y < f.h; y++) for (uint32_t x = 0; x < W && x < f.w; x++) frame[y * W + x] = canvas[y * f.w + x];
    uint32_t b = busyness(frame, frame_pal);
    if (!have || b > best_busy) {best_busy = b; copy(px, frame, W * H); copy(pal, frame_pal, 768); have = 1;} /* the caller's px, pal hold the best so far */
  }
  if (!have) return -1;
  int last = busyness(frame, frame_pal) * 10u >= best_busy * 8u;
  if (last) {copy(px, frame, W * H); copy(pal, frame_pal, 768);}
  return 0;
}
static int graphic(gp_read_fn read, void *user, const char *name, int depth, uint8_t *px, uint8_t *pal);
/* The slide held longest (no pause: until a key, the longest of all), the later one on a tie. */
static int list_pic(gp_read_fn read, void *user, const char *name, int depth, uint8_t *px, uint8_t *pal) {
  if (depth > 2) return -1;
  int32_t n = read(user, name, list, sizeof list - 1);
  if (n <= 0) return -1;
  char names[32][16]; uint32_t waits[32]; unsigned count = 0;
  for (int32_t at = 0; at < n && count < 32;) {
    int32_t end = at; while (end < n && list[end] != '\n') end++;
    int32_t a = at, b; while (a < end && (list[a] == ' ' || list[a] == '\t')) a++;
    b = a; unsigned k = 0;
    while (b < end && list[b] != ' ' && list[b] != '\t' && list[b] != '\r' && list[b] != 0x1a) {if (k < 15) names[count][k++] = (char)list[b]; b++;}
    names[count][k] = 0;
    while (b < end && (list[b] == ' ' || list[b] == '\t')) b++;
    uint32_t wait = 0xffff, v = 0; int digits = 0;
    while (b < end && list[b] >= '0' && list[b] <= '9') {v = v * 10 + (uint32_t)(list[b++] - '0'); digits = 1; if (v > 0xffff) v = 0xffff;}
    if (digits) wait = v;
    if (k) waits[count++] = wait;
    at = end + 1;
  }
  uint8_t done[32] = {0};
  for (unsigned tries = 0; tries < count; tries++) {
    int pick = -1;
    for (unsigned i = 0; i < count; i++) if (!done[i] && (pick < 0 || waits[i] >= waits[pick])) pick = (int)i;
    done[pick] = 1;
    if (!graphic(read, user, names[pick], depth + 1, px, pal)) return 0;
  }
  return -1;
}
static int graphic(gp_read_fn read, void *user, const char *name, int depth, uint8_t *px, uint8_t *pal) {
  if (!name[0] || splash(name)) return -1;
  const char *e = ext(name); int r = -1;
  if (is(e, ".gif")) r = gif_at(read(user, name, file, sizeof file), 0, 0, px, pal);
  else if (is(e, ".bkd")) { /* { i16 x, i16 y } and a GIF name */
    int32_t n = read(user, name, file, 64);
    if (n < 5) return -1;
    int32_t x = (int16_t)(file[0] | file[1] << 8), y = (int16_t)(file[2] | file[3] << 8);
    char gif[GP_NAME_MAX + 1]; int32_t a = 4, end = 4, k = 0;
    while (end < n && end < 4 + 39 && file[end] != '\r' && file[end] != '\n' && file[end]) end++;
    while (a < end && file[a] == ' ') a++;
    while (end > a && file[end - 1] == ' ') end--;
    for (; a < end && k < (int32_t)GP_NAME_MAX; a++) gif[k++] = (char)file[a];
    gif[k] = 0;
    if (!k || splash(gif)) return -1;
    r = gif_at(read(user, gif, file, sizeof file), x, y, px, pal);
  } else if (is(e, ".fli") || is(e, ".flc")) r = fli_pic(read(user, name, file, sizeof file), px, pal);
  else if (is(e, ".lst")) return list_pic(read, user, name, depth, px, pal);
  return r || blank(px) ? -1 : 0;
}
int gm_title_picture(gp_read_fn read, void *user, const char *gam_name, uint8_t *px, uint8_t *pal) {
  if (gp_open(read, user, gam_name) < 0) return -1;
  char title[GP_NAME_MAX + 1], menu[GP_NAME_MAX + 1]; unsigned k;
  const char *t = gp_graphic(GP_TITLE, 0), *m = gp_graphic(GP_MENU, 0);
  for (k = 0; t[k] && k < GP_NAME_MAX; k++) title[k] = t[k];
  title[k] = 0;
  for (k = 0; m[k] && k < GP_NAME_MAX; k++) menu[k] = m[k];
  menu[k] = 0;
  return !graphic(read, user, title, 0, px, pal) || !graphic(read, user, menu, 0, px, pal) ? 0 : -1;
}
void gm_shrink(const uint8_t *px, const uint8_t *pal, uint16_t *out, uint32_t w, uint32_t h) {
  uint32_t rgb[256];
  for (unsigned i = 0; i < 256; i++) {
    const uint8_t *c = pal + i * 3; uint32_t v = 0;
    for (int k = 0; k < 3; k++) v = v << 8 | ((c[k] & 63u) << 2 | (c[k] & 63u) >> 4);
    rgb[i] = v;
  }
  for (uint32_t y = 0; y < h; y++) {
    uint32_t y0 = y * H / h, y1 = (y + 1) * H / h; if (y1 <= y0) y1 = y0 + 1;
    for (uint32_t x = 0; x < w; x++) {
      uint32_t x0 = x * W / w, x1 = (x + 1) * W / w; if (x1 <= x0) x1 = x0 + 1;
      uint32_t r = 0, g = 0, b = 0, n = (x1 - x0) * (y1 - y0);
      for (uint32_t yy = y0; yy < y1; yy++) for (uint32_t xx = x0; xx < x1; xx++) {
        uint32_t c = rgb[px[yy * W + xx]]; r += c >> 16; g += c >> 8 & 255u; b += c & 255u;
      }
      out[y * w + x] = gm_565((r + n / 2) / n << 16 | (g + n / 2) / n << 8 | (b + n / 2) / n);
    }
  }
}
static void init(void) {if (!owners_set) {for (unsigned i = 0; i < GM_THUMB_SLOTS; i++) owner[i] = -1; owners_set = 1;}}
void gm_thumb_clear(void) {owners_set = 0; init();}
const uint16_t *gm_thumb_get(unsigned id) {
  init();
  for (unsigned i = 0; i < GM_THUMB_SLOTS; i++) if (owner[i] == (int16_t)id) {used[i] = ++clock; return cache[i];}
  return 0;
}
int gm_thumb_has(unsigned id) {
  init();
  for (unsigned i = 0; i < GM_THUMB_SLOTS; i++) if (owner[i] == (int16_t)id) return 1;
  return 0;
}
void gm_thumb_put(unsigned id, const uint8_t *px, const uint8_t *pal) {
  init();
  unsigned s = 0;
  for (unsigned i = 0; i < GM_THUMB_SLOTS; i++) {
    if (owner[i] == (int16_t)id) {s = i; break;}
    if (owner[i] < 0 || used[i] < used[s]) s = i;
    if (owner[i] < 0) break;
  }
  owner[s] = (int16_t)id; used[s] = ++clock;
  gm_shrink(px, pal, cache[s], GM_THUMB_W, GM_THUMB_H);
}
