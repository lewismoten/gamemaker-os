/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * Digitized .voc effects for /dev/audio: a streaming Creative Voice File player that follows the Sandbox
 * Edition's web engine (formats/voc.ts decodeVoc, audio/engine.ts playVoc, MIT, copyright (c) 2014 gandrewstone;
 * (c) 2026 Lewis Moten modifications), which stands in for the Creative driver RSD's PlaySbVocFile() handed the
 * file to. The file is read through a 384-byte window, never loaded whole when the
 * source can seek. One voice: a new clip cuts the old one, as PlaySbVocFile does. Output is mono, mixed into
 * au_render's accumulator, resampled to 48 kHz by linear interpolation at the web engine's 0.8 level.
 * Nothing here knows about the board: the source is a callback. */
#pragma once
#include <stdint.h>
#define VP_WIN 384u
#define VP_MAX_SAMPLES 16000000u  /* the web engine's VOC_MAX_SAMPLES: more is refused */
#define VP_MAX_VISITS 10000u      /* its guard on block visits (a repeat loop); more blocks end the sound */
#define VP_GAP_VISITS 256u        /* more empty block visits in a row than this: refused (so a pump never stalls) */
#define VP_RATE 48000u
#define VP_OK 0
#define VP_PLAYING 2              /* the state of a clip that is playing (VP_OK is what vp_prepare returns then) */
#define VP_PENDING 1
#define VP_INVALID (-22)
#define VP_LIMIT (-7)
#define VP_EMPTY (-61)            /* no samples (the web engine plays nothing) */
struct vp_io {
  void *user;
  /* Up to n bytes at offset off; fewer (or 0) at the end of the file, negative on an error. */
  int32_t (*pread)(void *user, uint32_t off, void *buf, uint32_t n);
  void (*close)(void *user);  /* optional; called once when the clip stops */
  uint32_t size;              /* the file's length */
};
struct vp_cur {               /* a walk over the blocks (the prescan runs one, playback another) */
  uint32_t p, visits, gap, rstart, rcount, rate;
  uint8_t have_rstart, codec, ad_started;
};
struct vp_seg {uint32_t off, len; uint8_t kind, init;}; /* len in bytes of source data */
struct vp {
  struct vp_io io;
  int state;                  /* 0 idle, VP_PENDING scanning, VP_PLAYING, <0 refused */
  struct vp_cur scan, play;
  uint32_t total, rate, phase, frame, frames, ad_pos, seg_pos;
  struct vp_seg seg; uint8_t ad_ref, ad_scale, ad_sub, seg_open;
  int32_t s0, s1; uint8_t primed, ended;
  uint8_t win[VP_WIN]; uint32_t win_off, win_len;
};
void vp_init(struct vp *v);
void vp_stop(struct vp *v);                              /* silence; closes the source */
int vp_begin(struct vp *v, const struct vp_io *io);      /* stops the old clip, starts scanning this one; VP_PENDING or an error */
int vp_prepare(struct vp *v, unsigned budget);           /* scan up to budget block visits: VP_PENDING, VP_OK (plays now) or an error (stopped) */
void vp_mix(struct vp *v, int32_t *acc, unsigned n);     /* adds n mono samples at 48 kHz (silence unless playing); the end stops it */
