/* SPDX-License-Identifier: MIT; the Game-Maker profile for Sandbox JS (docs/js-profile.md): the
 * <game> element and game-folder pictures over this repository's play core (game.cpp, session.c,
 * sound.c, thumb.c). It defines jsrt_profile, one hook per old `#if JS_GAME` block of src/js/jsrt.c;
 * the table is sandbox-kernel js/docs/profiles.md. Build: jsprof.mk. Headers are included by
 * relative path so this file does not need gamemaker-os's src/ on the include path (the core has
 * headers of the same names: gif.h, dom.h). */
#include <stdio.h>
#include <string.h>
#include <profile.h>
#include <jsrt.h>
#include <plat.h>
#include "../session.h"
#include "../thumb.h"

static struct gs_session session;
static char game_dir[96];
static uint32_t handle_id;
static int playing;

static void say(const char *s) {jsrt_log(s);}
static int32_t path_read(const char *dir, const char *name, uint8_t *buf, uint32_t cap) {
  char path[112]; int n = snprintf(path, sizeof path, "%s/", dir);
  for (; *name && n < (int)sizeof path - 1; name++) path[n++] = *name >= 'a' && *name <= 'z' ? (char)(*name - 32) : *name;
  path[n] = 0;
  return jsrt_fs_read(path, buf, cap);
}
static int32_t dir_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {return path_read(user, name, buf, cap);}
static int32_t picture_read(void *user, const char *name, uint8_t *buf, uint32_t cap) {return path_read(*(const char *const *)user, name, buf, cap);}
struct find {char gam[16];};
static void find_gam(void *user, const char *name, uint32_t n, int dir, uint32_t size) {
  (void)size; struct find *f = user;
  if (dir || n < 5 || n > 12 || f->gam[0]) return;
  const char *x = name + n - 4;
  if (x[0] == '.' && (x[1] | 32) == 'g' && (x[2] | 32) == 'a' && (x[3] | 32) == 'm') {memcpy(f->gam, name, n); f->gam[n] = 0;}
}

static void gm_init(void) {memset(&session, 0, sizeof session); playing = 0; handle_id = 0; game_dir[0] = 0;}
static void gm_release(void *user) {(void)user; gp_release_all();}
static int gm_play(const char *dir, void **handle, char *err, uint32_t cap) {
  struct find f = {{0}};
  if (jsrt_fs_list(dir, find_gam, &f) < 0 || !f.gam[0]) {snprintf(err, cap, "no .GAM in %s", dir); return -1;}
  snprintf(game_dir, sizeof game_dir, "%s", dir);
  memset(&session, 0, sizeof session);
  session.say = say;
  if (gs_start(&session, dir_read, game_dir, f.gam, plat_clock_ns())) {snprintf(err, cap, "cannot play %s", dir); return -1;}
  handle_id = jsrt_handle_open(JSRT_H_GAME, gm_release, &session);
  if (!handle_id) {gp_release_all(); snprintf(err, cap, "too many open handles"); return -1;}
  playing = 1;
  jsrt_stats.game_ticks = 0; jsrt_stats.game_play_ns = 0; jsrt_stats.game_late_max_us = jsrt_stats.game_step_max_us = 0;
  char b[160]; snprintf(b, sizeof b, "JS game %s/%s", dir, f.gam); jsrt_log(b);
  *handle = &session;
  return 0;
}
static void gm_end(void) {
  playing = 0;
  if (handle_id) {jsrt_handle_close(handle_id); handle_id = 0;} /* gm_release: gp_release_all */
  snd_end(&session.sound); /* the core calls end() before stop(), so an app stopped mid-game ends its sound here; a no-op when the game already did */
}
static int gm_key(uint16_t code, int32_t value) {return playing && (gs_event(&session, 1, code, value ? 1 : 0) & GS_EXIT) ? JSRT_END : 0;}
static uint32_t gm_step(uint64_t t, struct jsrt_direct *d) {
  if (!playing) return 0;
  uint64_t due = gs_wake(&session, t);
  if (session.phase == GS_PLAY && t > due) {uint32_t l = (uint32_t)((t - due) / 1000u); if (l > jsrt_stats.game_late_max_us) jsrt_stats.game_late_max_us = l;}
  static uint64_t last; if (session.phase == GS_PLAY && last && t > last) jsrt_stats.game_play_ns += t - last;
  last = t;
  int was = session.phase; uint32_t before = session.total_ticks;
  int r = gs_step(&session, t);
  uint32_t dt = (uint32_t)((plat_clock_ns() - t) / 1000u);
  if (session.ran && dt > jsrt_stats.game_step_max_us) jsrt_stats.game_step_max_us = dt;
  jsrt_stats.game_ticks += session.total_ticks - before;
  if (was != GS_PLAY && session.phase == GS_PLAY) {session.t0 = plat_clock_ns(); session.ticks = 0;}
  uint32_t out = 0;
  if (r & GS_DRAW) {out |= JSRT_DRAW; d->px = session.px; d->pal = session.pal;}
  if (r & GS_EXIT) out |= JSRT_END;
  return out;
}
static uint64_t gm_wake(uint64_t now) {return playing ? gs_wake(&session, now) : 0;}
static void gm_draw(void *h, uint32_t *fb, uint32_t stride) {gs_draw(h, fb, stride);}
static uint32_t gm_ticks(void) {return session.total_ticks;}
static int gm_picture(const char *src, uint8_t *px, uint8_t *pal) {
  struct find f = {{0}};
  if (jsrt_fs_list(src, find_gam, &f) < 0 || !f.gam[0]) return -1;
  static char picture_dir[96]; static const char *dirp; /* gp_open retains the reader context until the next game is opened */
  snprintf(picture_dir, sizeof picture_dir, "%s", src); dirp = picture_dir;
  return gm_title_picture(picture_read, &dirp, f.gam, px, pal);
}
static const struct jsrt_profile gm = {JSRT_PROFILE_ABI, "gamemaker", gm_init, 0, gm_play, gm_end, gm_key, gm_step, gm_wake, gm_draw, gm_ticks, gm_picture};
const struct jsrt_profile *const jsrt_profile = &gm;
