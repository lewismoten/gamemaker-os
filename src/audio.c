/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten; see audio.h. */
#include "audio.h"
static void copy(char *to, const char *from) {unsigned i = 0; while (from && from[i] && i < AU_NAME_MAX) {to[i] = from[i]; i++;} to[i] = 0;}
static int same(const char *a, const char *b) {unsigned i = 0; while (a[i] && a[i] == b[i]) i++; return a[i] == b[i];}
void au_init(struct au *a, uint8_t *music, uint32_t music_cap) {
  uint8_t *b = (uint8_t *)a;
  for (unsigned i = 0; i < sizeof *a; i++) b[i] = 0;
  a->music = music; a->music_cap = music_cap; a->vol = a->vol_target = AU_VOL_ONE;
  cmf_init(&a->cmf, &a->opl); pc_init(&a->pc); vp_init(&a->vp);
}
void au_music(struct au *a, const char *cmf) {copy(a->want_music, cmf); a->music_pending = 1;}
void au_beep(struct au *a, const char *snd, int tune) {copy(a->want_snd, snd); a->want_tune = tune; a->beep_pending = 1;}
void au_voc(struct au *a, const char *voc) {vp_stop(&a->vp); copy(a->want_voc, voc); a->voc_pending = a->want_voc[0] != 0;}
int au_ui(struct au *a, int cue) {return ui_start(&a->ui, cue);}
void au_stop(struct au *a) {a->want_music[0] = 0; a->music_pending = 1; a->beep_pending = 0; pc_stop(&a->pc); vp_stop(&a->vp); a->voc_pending = 0;}
void au_volume(struct au *a, uint32_t target) {a->vol_target = target > AU_VOL_MAX ? AU_VOL_MAX : target;}
void au_silence(struct au *a) {
  a->music_pending = 0; a->beep_pending = 0; a->voc_pending = 0; a->hold_len = 0; vp_stop(&a->vp);
  cmf_stop(&a->cmf); pc_stop(&a->pc); a->hush_left = 0; a->vol = a->vol_target = 0;
}
void au_hush(struct au *a) {
  for (unsigned i = 0; i < 9; i++) opl_write(&a->opl, 0xb0 + i, opl_read(&a->opl, 0xb0 + i) & ~0x20u);
  for (unsigned r = 0x80; r < 0x96; r++) opl_write(&a->opl, r, 0xff); /* sustain level 15, release rate 15, every operator */
  opl_write(&a->opl, 0xbd, 0xc0);
  a->cmf.playing = 0; a->cmf.off = 0; a->hush_left = AU_HUSH_FRAMES;
}
int au_play_loaded(struct au *a, uint32_t n) {
  if (cmf_load(&a->cmf, a->music, n)) {cmf_stop(&a->cmf); return -1;}
  a->until_q16 = 0; a->hush_left = 0; a->music_loads++; a->vol_target = AU_VOL_ONE; return 0;
}
void au_set_playing(struct au *a, int playing) {a->playing = playing != 0;}
int au_service(struct au *a, int32_t (*read)(void *, const char *, uint8_t *, uint32_t), void *user) {
  int started = 0;
  if (a->music_pending) {
    a->music_pending = 0;
    if (a->cmf.playing && a->vol && a->want_music[0]) {a->vol_target = 0; a->music_pending = 1; return 0;} /* fade out first; the switch is at silence */
    if (!a->want_music[0] && a->hush_left) return 0; /* a second stop keeps the release running */
    if (!a->want_music[0] && a->cmf.playing) {au_hush(a); return 0;} /* a stop: keyed off at once, the chip reset in 200 ms */
    cmf_stop(&a->cmf); /* the buffer is about to change under the pump (the read calls the heartbeat) */
    if (a->want_music[0]) {
      int32_t n = read ? read(user, a->want_music, a->music, a->music_cap) : -1;
      if (n > 0 && !au_play_loaded(a, (uint32_t)n)) started = 1;
      else cmf_stop(&a->cmf);
    }
  }
  if (a->beep_pending) {
    a->beep_pending = 0;
    if (!same(a->snd_name, a->want_snd)) {
      int32_t n = read && a->want_snd[0] ? read(user, a->want_snd, a->snd, sizeof a->snd) : -1;
      a->snd_len = n > 0 ? (uint32_t)n : 0; copy(a->snd_name, n > 0 ? a->want_snd : "");
    }
    if (a->snd_len && a->want_tune >= 0 && !pc_start(&a->pc, a->snd, a->snd_len, (unsigned)a->want_tune)) {a->beeps++; started = 1;}
  }
  if (a->voc_pending) {
    a->voc_pending = 0;
    struct vp_io io; int st = VP_INVALID;
    if (a->voc_open && !a->voc_open(a->want_voc, &io)) {
      st = vp_begin(&a->vp, &io); /* the scan takes a few block reads; a clip of thousands of blocks is stopped by its budget */
    }
    if (st != VP_PENDING) a->voc_refused++;
  }
  if (a->vp.state == VP_PENDING) {
    int st = vp_prepare(&a->vp, 64); /* scan incrementally, between ring refills */
    if (st == VP_OK) {a->vocs++; started = 1;}
    else if (st != VP_PENDING) a->voc_refused++;
  }
  return started;
}
void au_render(struct au *a, int16_t *out, unsigned n) {
  int32_t acc[AU_CHUNK];
  int16_t sp[AU_CHUNK * 2];
  unsigned i = 0;
  for (unsigned k = 0; k < n; k++) acc[k] = 0;
  if (a->cmf.playing) while (i < n) { /* the worklet's loop: tick when due, then render up to the next one */
    if ((int32_t)a->until_q16 <= 0) {cmf_tick(&a->cmf); a->until_q16 += CMF_TICK_Q16;}
    unsigned run = (a->until_q16 + 65535u) >> 16;
    if (run > n - i) run = n - i;
    opl_render(&a->opl, acc + i, run);
    a->until_q16 -= run << 16; i += run;
  } else {
    opl_render(&a->opl, acc, n); /* a stopped song's last notes ring out their release */
    if (a->hush_left) {if (a->hush_left <= n) {a->hush_left = 0; cmf_stop(&a->cmf);} else a->hush_left -= n;}
  }
  for (unsigned k = 0; k < n; k++) {int32_t m = (int32_t)((int64_t)acc[k] * a->vol >> 15); if (m < 0) m = -m; if (!a->music_muted && (uint32_t)m > a->music_peak) a->music_peak = (uint32_t)m;} /* the song alone, after the master volume */
  if (a->music_muted) for (unsigned k = 0; k < n; k++) acc[k] = 0;
  if (a->pc.active) {
    pc_render(&a->pc, sp, n);
    for (unsigned k = 0; k < n; k++) acc[k] += sp[k * 2];
  }
  vp_mix(&a->vp, acc, n);
  int32_t ub[AU_CHUNK];
  for (unsigned k = 0; k < n; k++) ub[k] = 0;
  ui_mix(&a->ui, ub, n);
  for (unsigned k = 0; k < n; k++) {
    int32_t v = acc[k];
    if (a->vol != a->vol_target) a->vol = a->vol < a->vol_target ? a->vol + 1 : a->vol - 1;
    v = (int32_t)((int64_t)v * a->vol >> 15) + ub[k];
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    out[k * 2] = out[k * 2 + 1] = (int16_t)v;
    if ((uint32_t)(v < 0 ? -v : v) > a->peak) a->peak = (uint32_t)(v < 0 ? -v : v);
  }
  a->frames += n;
}
int au_quiet(const struct au *a) {
  return !a->playing && !(a->cmf.playing && !a->music_muted) && !a->hush_left && !a->music_pending && !a->voc_pending && !a->beep_pending &&
         !a->pc.active && a->vp.state != VP_PLAYING && a->vp.state != VP_PENDING && a->vol == a->vol_target;
}
int au_pump(struct au *a, const struct au_sink *s) {
  uint32_t queued = 0, cap = 0, sent = 0;
  if (s->query(s->user, &queued, &cap)) return -1;
  int quiet = au_quiet(a);
  uint32_t low = quiet ? AU_QUIET_LOW_FRAMES : a->playing ? AU_PLAY_LOW_FRAMES : AU_LOW_FRAMES, fill = quiet ? AU_QUIET_FILL_FRAMES : a->playing ? AU_PLAY_FILL_FRAMES : AU_FILL_FRAMES;
  uint64_t t = s->now(s->user);
  if (a->last_pump_ns) {
    uint64_t gap = t - a->last_pump_ns;
    if (gap > AU_GAP_CLAMP_NS) gap = AU_GAP_CLAMP_NS; /* a long pause: more than this is never useful, and the frame math must not wrap */
    a->pump_gap_ns = gap;
    if (gap > a->gap_max_ns) a->gap_max_ns = gap;
    if (gap > a->gap_peak_ns) a->gap_peak_ns = gap;
  }
  a->last_pump_ns = t;
  /* Round up observed worst gap + worst slice + two 10 ms scheduling slices.
     Keep a conservative floor before observations, and never exceed the target. */
  uint64_t observed = ((a->gap_peak_ns + a->slice_peak_ns + 2u * AU_SLEEP_NS) * 48000u + 999999999u) / 1000000000u;
  if (!quiet && observed > low) low = observed < fill ? (uint32_t)observed : fill;
  if (!cap || queued > cap) return -1;
  if (low > cap) low = cap;
  if (fill > cap) fill = cap;
  a->low_frames = low;
  if (queued >= low) return 0;
  /* Frames can be slow: render by elapsed time (what the ring played since the last pump, plus a quarter), and more when it is low. */
  uint32_t budget = AU_BATCH; uint64_t io_ns = 0, render_ns = 0;
  if (a->pump_gap_ns) {uint64_t ef = a->pump_gap_ns * 48000u / 1000000000ull; ef += ef / 4; if (ef > budget) budget = ef < AU_BATCH_MAX ? (uint32_t)ef : AU_BATCH_MAX;}
  if (queued < low / 2 && budget < AU_BATCH_MAX) budget = AU_BATCH_MAX; /* time in the device's write and query: the render's own time is bounded by the budget */
  while (queued < fill) {
    if (io_ns > AU_PUMP_IO_NS) break; /* a flapping device: give up for this frame, the rest is kept for the next pump */
    uint32_t want = fill - queued;
    if (!a->hold_len) {
      if (!budget) break;
      uint32_t n = want < AU_CHUNK ? want : AU_CHUNK;
      if (n > budget) n = budget;
      uint64_t t0 = s->now(s->user);
      au_render(a, a->hold, n);
      uint64_t elapsed = s->now(s->user) - t0;
      a->total_ns += elapsed; render_ns += elapsed;
      if (elapsed > a->slice_peak_ns) a->slice_peak_ns = elapsed;
      a->hold_len = n * 4u; a->hold_off = 0;
      budget = render_ns >= AU_PUMP_RENDER_NS ? 0 : budget - n;
    }
    uint32_t bytes = a->hold_len;
    if (bytes > want * 4u) bytes = want * 4u;
    uint64_t io0 = s->now(s->user);
    int32_t w = s->write(s->user, (const uint8_t *)a->hold + a->hold_off, bytes);
    io_ns += s->now(s->user) - io0;
    if (w == -11 || w == 0) {a->eagain++; break;} /* keep the unsent samples for the next pump */
    if (w < 0 || (uint32_t)w > bytes || w % 4) return -1;
    a->writes++;
    a->hold_off += (uint32_t)w; a->hold_len -= (uint32_t)w; sent += (uint32_t)w / 4u;
    io0 = s->now(s->user); int32_t qrc = s->query(s->user, &queued, &cap); io_ns += s->now(s->user) - io0;
    if (qrc || !cap || queued > cap) return -1;
    fill = quiet ? AU_QUIET_FILL_FRAMES : a->playing ? AU_PLAY_FILL_FRAMES : AU_FILL_FRAMES;
    if (fill > cap) fill = cap;
    if ((!a->hold_len && queued < fill && fill - queued <= AU_REFILL_MARGIN)) break;
  }
  return (int)sent;
}
void au_wait_until(const struct au_sink *sink, uint64_t deadline,
                   void (*service)(void *), void (*sleep_until)(void *, uint64_t)) {
  /* Bounded and cooperative: one call waits at most AU_WAIT_MAX_NS (the caller loops, presents and reads input), and a sleep that
     returns at once cannot spin it past AU_WAIT_SPINS rounds. */
  uint64_t limit = sink->now(sink->user) + AU_WAIT_MAX_NS;
  if (deadline > limit) deadline = limit;
  for (unsigned i = 0; i < AU_WAIT_SPINS; i++) {
    service(sink->user);
    uint64_t now = sink->now(sink->user);
    if (now >= deadline) return;
    uint64_t slice = deadline - now < AU_SLEEP_NS ? deadline : now + AU_SLEEP_NS;
    sleep_until(sink->user, slice);
  }
}
