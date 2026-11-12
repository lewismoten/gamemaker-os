/* SPDX-License-Identifier: MIT */
#include "awake.h"
#include "session.h"
#include "settings.h"
#include <sandbox/syscall.h>
static int supported;
static uint64_t token;

void awake_init(uint32_t major, uint32_t minor) {
  supported = major == 1 && minor >= 11;
  token = 0;
}
int32_t awake_configure(const struct gm_settings *settings) {
  if (!supported) return 0;
  return sbx_power_timeouts(settings->power_saving ? 60 : 0,
                            gm_settings_blank_seconds(settings));
}
int32_t awake_set(int watching) {
  if (!supported) return 0;
  if (watching) {
    if (token) return 0;
    return sbx_power_acquire(SBX_POWER_NO_BLANK | SBX_POWER_NO_SLOWDOWN, &token);
  }
  if (token) {
    int32_t r = sbx_power_release(token);
    /* EBADF means the kernel no longer owns this token. Other errors may
     * be transient: retain it, never acquire a second hold over it. */
    if (!r || r == -SBX_EBADF) token = 0;
    return r;
  }
  return 0;
}
int32_t awake_session(const struct gs_session *s, int overlay) {
  int watching = !overlay && (s->phase == GS_PLAY ||
    (s->phase == GS_CARDS && !(s->stage == GS_PAUSE && s->pause == GS_KEY_WAIT)));
  return awake_set(watching);
}
int32_t awake_shutdown(void) {return awake_set(0);}
