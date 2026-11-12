/* SPDX-License-Identifier: MIT; one OS-owned ABI 1.11 playback hold. */
#pragma once
#include <stdint.h>
struct gs_session;
struct gm_settings;
/* Initialize once at startup, before any playback. */
void awake_init(uint32_t major, uint32_t minor);
/* Apply user idle timeouts; quietly succeeds without a syscall on older ABI. */
int32_t awake_configure(const struct gm_settings *settings);
/* Idempotent; errors retain ownership for a later retry. */
int32_t awake_set(int watching);
int32_t awake_session(const struct gs_session *s, int overlay);
/* ABI 1.12: 1 while the kernel has the screen blanked (power saving), else 0 (also on older kernels). One cheap call:
 * ask once a loop pass; any key, CEC key or USB input makes it 0 again. Never true while a hold from awake_set is held. */
int awake_screen_dark(void);
/* Call on every normal/error return from main; kernel cleanup also covers faults. */
int32_t awake_shutdown(void);
