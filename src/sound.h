/* SPDX-License-Identifier: MIT; a game's sounds as SND log lines. The board
 * has no audio driver, so a page on the computer at the other end of the USB
 * log (the Sandbox Edition's Pi remote) can play them from its own copy of the
 * game's files:
 *   SND game <DIR> <GAM>          a game starts (its /sd/GAMES folder and .GAM)
 *   SND music start <DIR>/<CMF>   PlaySong (a level, the title, won or lost)
 *   SND music stop                a slot with no song: the song stops
 *   SND voc <DIR>/<NAME>.VOC      an effect with a Sound Blaster clip
 *   SND beep <DIR>/<SND> <n>      an effect played on the PC speaker: tune n of that .SND
 *   SND stopall                   everything stops (the game ends or Esc)
 *   SND end                       the game is over; back to the list
 * File names are upper case, as on the card. A write to the log never waits
 * for the USB side; effects are one line a tick at most (the last one started,
 * as heard), and the same effect again within SND_REPEAT_TICKS is left out. */
#pragma once
#include <stdint.h>
#define SND_REPEAT_TICKS 3u
enum {SND_EV_MUSIC, SND_EV_BEEP, SND_EV_STOP};
#define SND_DIR_MAX 8u
struct gs_sound {
  void (*say)(const char *line);
  /* Optional, for a driver that plays the sounds (audio.h): kind SND_EV_MUSIC (name: the .CMF, "" stops),
     SND_EV_BEEP (name: the .SND, n: the tune) or SND_EV_STOP. Called where the SND line is logged. */
  void (*event)(void *user, int kind, const char *name, int n);
  void *event_user;
  char dir[SND_DIR_MAX + 1];
  int active;          /* between "game" and "end" */
  int last_fx;         /* the last effect line's effect (-1: none) */
  uint32_t last_tick;  /* the tick it was logged on */
};
/* The game's folder for the lines (call before gs_start; "" logs "-"). */
void snd_dir(struct gs_sound *s, const char *dir);
void snd_game(struct gs_sound *s, const char *gam);
/* PlaySong of a slot: "" stops the music. */
void snd_music(struct gs_sound *s, const char *cmf);
/* After a game tick: the effect the core started, if any. */
void snd_effect(struct gs_sound *s, uint32_t tick);
/* The game is over: stopall and end, once. */
void snd_end(struct gs_sound *s);
