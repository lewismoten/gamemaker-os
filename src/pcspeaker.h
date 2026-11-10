/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * PC-speaker PCM producer, linked into GMOS audio and host qualification.
 * RSD SOUND.H/PLAYGAME.C and Sandbox audio/engine.ts define format/clock/level;
 * new integer rational-clock/phase implementation, no third-party synth code.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#define PC_RATE 48000u
#define PC_PIT 1193180u
#define PC_DIVISOR 8191u
#define PC_STEPS 36u
#define PC_MAX_FRAMES 1920u
#define PC_AMPLITUDE 3146 /* round(32767 * 0.12 * 0.8) */
struct pcspeaker {
    uint16_t hz[PC_STEPS];
    uint32_t step,clock,phase,active;
};
void pc_init(struct pcspeaker *p);
/* Full SND length must be 2160 or 2430; select tune 0..29, copy no pointers. */
int pc_start(struct pcspeaker *p,const void *snd,size_t bytes,unsigned tune);
void pc_stop(struct pcspeaker *p);
/* Exactly frames stereo s16 LE; out is trusted, nonaliasing private scratch.
 * <=1920 frames; zero frames is a no-op. Retrigger resets phase, step and clock.
 */
int pc_render(struct pcspeaker *p,void *out,size_t frames);
/* Accounting helper only, not a scheduler: preserved session.h literal ns10.
 * Initialize remainder to zero; caller owns serial lifetime. */
struct pc_tick_clock { uint64_t remainder; };
uint32_t pc_tick_frames(struct pc_tick_clock *clock);
