/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * A game's sound for /dev/audio (ABI 1.8): the CMF song on the OPL2 synth (cmf.h, opl.h) and the PC
 * speaker effects (pcspeaker.h), mixed to 16-bit stereo at 48 kHz and kept fed from the main loop.
 * The sink never blocks: au_pump tops up from live ring levels, with bounded synthesis work per
 * call, so the 18.2 Hz tick and the heartbeat go on. The mix and gains are the web engine's (audio/engine.ts):
 * the music is 0.12 * 0.8, the speaker's square 0.12 * 0.8. A VOC clip (vocplay.h) is one more voice in the same sum: a new clip cuts the old one, the speaker and the song go on. */
#pragma once
#include "cmf.h"
#include "pcspeaker.h"
#include "vocplay.h"
#define AU_LOW_FRAMES 12000u  /* 250 ms floor: 150 ms stall + one 55 ms tick + 45 ms synth/scheduling margin */
#define AU_FILL_FRAMES 15000u /* and top up to this many (312 ms; the kernel ring holds 16384, an older one 4096) */
#define AU_VOL_MAX 65535u     /* full uint16 gain range, Q15; saturated after mixing */
#define AU_VOL_ONE 32768u     /* master volume 1.0 (Q15); it ramps one step a sample: 0.68 s from silence to full */
#define AU_PLAY_FILL_FRAMES AU_FILL_FRAMES /* 312.5 ms, clamped to the kernel ring */
#define AU_PLAY_LOW_FRAMES AU_LOW_FRAMES
#define AU_CHUNK 480u        /* frames per synthesis slice (10 ms); pcspeaker.h's limit is 1920 */
#define AU_BATCH (AU_CHUNK * 4u)
#define AU_REFILL_MARGIN 48u
#define AU_SLEEP_NS 10000000ull
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
  struct opl opl; struct cmf cmf; struct pcspeaker pc; struct vp vp;
  /* Opens a clip of the game that is playing: fills io (a pread over its file, or its bytes in memory) and returns 0, or -1. Set by the host. */
  int (*voc_open)(const char *name, struct vp_io *io);
  char want_voc[AU_NAME_MAX + 1]; uint8_t voc_pending;
  uint32_t vol, vol_target;    /* master volume, Q15, ramped per sample so nothing clicks */
  uint32_t until_q16;          /* samples to the next sequencer tick, 16 fractional bits */
  int16_t hold[AU_CHUNK * 2]; uint32_t hold_len, hold_off; /* rendered frames the ring has not taken yet */
  uint8_t *music; uint32_t music_cap; /* the caller's scratch for a song (the biggest in the games is 19.8 KB) */
  uint8_t snd[2430];
  char snd_name[AU_NAME_MAX + 1]; uint32_t snd_len;
  /* Requests from the session, served by au_service (which reads files). */
  char want_music[AU_NAME_MAX + 1]; uint8_t music_pending;
  char want_snd[AU_NAME_MAX + 1]; int want_tune; uint8_t beep_pending;
  uint8_t playing;             /* gameplay mode; both modes retain stall headroom */
  uint64_t last_pump_ns, gap_max_ns, svc_max_ns; /* interval maxima, in every mode */
  uint64_t gap_peak_ns, slice_peak_ns; /* lifetime observations for low-water sizing */
  uint32_t low_frames; /* last effective low-water mark, capacity clamped */
  /* Counters. */
  uint64_t frames, total_ns; uint32_t writes, eagain, music_loads, beeps, vocs, voc_refused;
};
/* music: scratch for the song file while it plays (it must stay untouched until the next song or au_stop). */
void au_init(struct au *a, uint8_t *music, uint32_t music_cap);
/* The session's events (sound.h): a song (name "" stops it), a beep (tune n of the .SND), everything stops. */
void au_music(struct au *a, const char *cmf);
void au_beep(struct au *a, const char *snd, int tune);
/* A digitized clip (name "NAME.VOC"): the clip playing is cut at once, as PlaySbVocFile does; au_service opens the new one. */
void au_voc(struct au *a, const char *voc);
/* A song change or stop fades the playing song out first (au_service does the switch at silence), then fades the new one in. */
void au_stop(struct au *a);
/* At once, no fade: the song, the speaker and any request stop, the chip is reset and the volume is 0 (what the ring already holds still plays). */
void au_silence(struct au *a);
/* Starts the song already read into the music buffer (n bytes) and fades it in; 0, or -1 when it is refused. */
int au_play_loaded(struct au *a, uint32_t n);
/* A fade to this volume (0..AU_VOL_MAX; AU_VOL_ONE = unity) begins. */
void au_volume(struct au *a, uint32_t target);
/* Reads the files the requests name through read (game.h's gp_read_fn); returns 1 when something started. */
int au_service(struct au *a, int32_t (*read)(void *, const char *, uint8_t *, uint32_t), void *user);
/* Renders n frames (at most AU_CHUNK) of stereo s16 into out: the sequencer ticks, the synth, the speaker. */
void au_render(struct au *a, int16_t *out, unsigned n);
/* Select gameplay/library mode without discarding queued PCM or gap history. */
void au_set_playing(struct au *a, int playing);
/* Feeds the ring if low, rechecking after writes, with at most AU_BATCH frames rendered per call;
   clamped to capacity. Returns frames written, or -1 on a fatal sink error. */
int au_pump(struct au *a, const struct au_sink *sink);
void au_wait_until(const struct au_sink *sink, uint64_t deadline,
                   void (*service)(void *), void (*sleep_until)(void *, uint64_t));
