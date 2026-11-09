/* SPDX-License-Identifier: MIT; original: SND log lines for a game's sounds (sound.h). */
#include "sound.h"
#include "game.h"
#define LINE_MAX 80u
static char *put(char *p, char *end, const char *s) {
  while (*s && p < end) {
    char c = *s++;
    if (c >= 'a' && c <= 'z') c = (char)(c - 32); /* the card's 8.3 names */
    *p++ = c > ' ' && c < 127 ? c : '_';         /* one field, printable */
  }
  return p;
}
static char *word(char *p, char *end, const char *s) {while (*s && p < end) *p++ = *s++; return p;}
static char *num(char *p, char *end, uint32_t n) {
  char t[12]; int k = 0;
  do t[k++] = (char)('0' + n % 10); while (n /= 10);
  while (k && p < end) *p++ = t[--k];
  return p;
}
static char *path(char *p, char *end, const struct gs_sound *s, const char *file) {
  p = s->dir[0] ? put(p, end, s->dir) : word(p, end, "-");
  p = word(p, end, "/");
  return put(p, end, file);
}
static void say(const struct gs_sound *s, char *line, char *p) {
  *p = 0;
  if (s->say) s->say(line);
}
void snd_dir(struct gs_sound *s, const char *dir) {
  unsigned k = 0;
  while (dir && dir[k] && k < SND_DIR_MAX) {s->dir[k] = dir[k]; k++;}
  s->dir[k] = 0;
}
void snd_game(struct gs_sound *s, const char *gam) {
  char line[LINE_MAX], *end = line + LINE_MAX - 1, *p = word(line, end, "SND game ");
  p = s->dir[0] ? put(p, end, s->dir) : word(p, end, "-");
  p = word(p, end, " "); p = put(p, end, gam);
  s->active = 1; s->last_fx = -1; s->last_tick = 0;
  gp_take_sound(); /* nothing from before the game */
  say(s, line, p);
}
void snd_music(struct gs_sound *s, const char *cmf) {
  if (!s->active) return;
  char line[LINE_MAX], *end = line + LINE_MAX - 1, *p;
  if (!cmf || !cmf[0]) p = word(line, end, "SND music stop");
  else p = path(word(line, end, "SND music start "), end, s, cmf);
  if (s->event) s->event(s->event_user, SND_EV_MUSIC, cmf ? cmf : "", 0);
  say(s, line, p);
}
void snd_effect(struct gs_sound *s, uint32_t tick) {
  int n = gp_take_sound();
  if (n < 0 || !s->active) return;
  if (n == s->last_fx && tick - s->last_tick < SND_REPEAT_TICKS) return;
  s->last_fx = n; s->last_tick = tick;
  char line[LINE_MAX], *end = line + LINE_MAX - 1, *p;
  const char *voc = gp_voc_name(n);
  if (voc[0]) {p = path(word(line, end, "SND voc "), end, s, voc); p = word(p, end, ".VOC");}
  else {
    p = path(word(line, end, "SND beep "), end, s, gp_sound_file()); p = word(p, end, " "); p = num(p, end, (uint32_t)n);
    if (s->event) s->event(s->event_user, SND_EV_BEEP, gp_sound_file(), n);
  }
  say(s, line, p);
}
void snd_end(struct gs_sound *s) {
  if (!s->active) return;
  s->active = 0;
  if (s->event) s->event(s->event_user, SND_EV_STOP, "", 0);
  char line[LINE_MAX], *end = line + LINE_MAX - 1;
  say(s, line, word(line, end, "SND stopall"));
  say(s, line, word(line, end, "SND end"));
}
