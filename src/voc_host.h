/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * UNLINKED host qualification. Hermes GPT-6.1 Sol / openai-codex.
 * RSD SOUNDC.C and Sandbox formats/voc.ts + audio/engine.ts (MIT) are
 * behavior references; see docs/voc-host.md. No Creative driver code.
 */
#pragma once
#include "pcspeaker.h"
#define VOC_SOURCE_MAX (2u * 1024u * 1024u)
#define VOC_VISITS_MAX 10000u
#define VOC_PREPARE_MAX 64u
#define VOC_SECONDS_MAX 120u
#define VOC_SILENCE UINT32_MAX
/* Explicit results, not a live syscall ABI. */
#define VOC_PENDING 1
#define VOC_INVALID (-22)
#define VOC_UNSUPPORTED (-95)
#define VOC_LIMIT (-7)
#define VOC_NOT_READY (-11)
struct voc_span { uint32_t end, offset; };
struct voc_host {
    uint8_t source[VOC_SOURCE_MAX];
    struct voc_span spans[VOC_VISITS_MAX];
    uint32_t bytes, pos, visits, count, samples, rate;
    uint32_t repeat_start, repeat_left, repeating, have_data;
    uint32_t frame, frames;
    int status;
};
/* Caller owns this fixed object and serializes access. All input/output spans
 * are trusted mapped and nonaliasing, not user pointers. begin copies source;
 * caller must supply the authoritative COMPLETE file length, never a read cap.
 * Every begin (even rejected) stops the previous voice. No app pointer retained.
 */
void voc_stop(struct voc_host *v);
int voc_begin(struct voc_host *v, const void *source, size_t bytes);
/* 1..64 block visits per call; repeat work <=10000 visits INCLUDING terminator.
 * No PCM until complete validation; 0 ready, 1 pending, negative explicit error.
 */
int voc_prepare(struct voc_host *v, unsigned budget);
/* <=1920 stereo S16_LE frames at48000Hz; silence after stop/end. Before ready,
 * returns NOT_READY (or stored error), without output or playback mutation.
 */
int voc_render(struct voc_host *v, void *output, size_t frames);
/* Bounded signed32 accumulation and s16 saturation of two stereo S16_LE spans.
 * dst may equal either input, but partial overlap is forbidden. */
int pcm_mix(void *dst, const void *a, const void *b, size_t frames);
/* Existing speaker is already master-scaled. VOC applies master4/5 exactly once.
 * Caller owns <=7680-byte scratch here; no live ring/device/session dispatch. */
int voc_speaker_render(struct voc_host *v, struct pcspeaker *p,
                       void *output, size_t frames);
