/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * A game's sound for /dev/audio (ABI 1.8): the CMF song on the OPL2 synth (cmf.h, opl.h) and the PC
 * speaker effects (pcspeaker.h), mixed to 16-bit stereo at 48 kHz and kept fed from the main loop.
 * Nothing here blocks: au_pump asks the ring's level and writes only when it is low, a little at a
 * time, so the 18.2 Hz tick and the heartbeat go on. The mix and gains are the web engine's (audio/engine.ts):
 * the music is 0.12 * 0.8, the speaker's square 0.12 * 0.8. VOC clips are not played (they stay SND log lines). */
#pragma once
#include "cmf.h"
#include "pcspeaker.h"
#define AU_LOW_FRAMES 2400u  /* write when fewer frames than this are queued (50 ms) */
#define AU_FILL_FRAMES 7200u /* and top up to this many (150 ms) */
#define AU_CHUNK 480u        /* frames rendered per write (10 ms); pcspeaker.h's limit is 1920 */
#define AU_NAME_MAX 16u
struct au_sink {
  void *user;
  /* Frames queued and the ring's capacity; 0 when it was read. */
  int32_t (*query)(void *user, uint32_t *queued, uint32_t *capacity);
  /* Bytes taken, or a negative error (-11, EAGAIN: none fit). */
  int32_t (*write)(void *user, const void *pcm, uint32_t bytes);
  uint64_t (*now)(void *user); /* monotonic ns, for the CPU figures */
};
struct au {
  struct opl opl; struct cmf cmf; struct pcspeaker pc;
  uint32_t until_q16;          /* samples to the next sequencer tick, 16 fractional bits */
  int16_t hold[AU_CHUNK * 2]; uint32_t hold_len, hold_off; /* a rendered chunk the ring has not taken yet */
  uint8_t *music; uint32_t music_cap; /* the caller's scratch for a song (the biggest in the games is 19.8 KB) */
  uint8_t snd[2430];
  char snd_name[AU_NAME_MAX + 1]; uint32_t snd_len;
  /* Requests from the session, served by au_service (which reads files). */
  char want_music[AU_NAME_MAX + 1]; uint8_t music_pending;
  char want_snd[AU_NAME_MAX + 1]; int want_tune; uint8_t beep_pending;
  /* Counters. */
  uint64_t frames, total_ns; uint32_t writes, eagain, music_loads, beeps;
};
/* music: scratch for the song file while it plays (it must stay untouched until the next song or au_stop). */
void au_init(struct au *a, uint8_t *music, uint32_t music_cap);
/* The session's events (sound.h): a song (name "" stops it), a beep (tune n of the .SND), everything stops. */
void au_music(struct au *a, const char *cmf);
void au_beep(struct au *a, const char *snd, int tune);
void au_stop(struct au *a);
/* Reads the files the requests name through read (game.h's gp_read_fn); returns 1 when something started. */
int au_service(struct au *a, int32_t (*read)(void *, const char *, uint8_t *, uint32_t), void *user);
/* Renders n frames (at most AU_CHUNK) of stereo s16 into out: the sequencer ticks, the synth, the speaker. */
void au_render(struct au *a, int16_t *out, unsigned n);
/* Feeds the ring if it is low; the frames written, or -1 when the query failed. */
int au_pump(struct au *a, const struct au_sink *sink);
