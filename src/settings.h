/* SPDX-License-Identifier: MIT; native user settings, stored with GMOS lists. */
#pragma once
#include "savefile.h"
struct gm_settings {
  unsigned screen_off; /* 0: 5 min, 1: 10 min, 2: 30 min, 3: Never */
  unsigned power_saving;
};
void gm_settings_defaults(struct gm_settings *s);
int gm_settings_load(struct gm_settings *s, const struct save_ops *o);
int gm_settings_save(const struct gm_settings *s, const struct save_ops *o);
uint32_t gm_settings_blank_seconds(const struct gm_settings *s);
const char *gm_settings_screen_label(const struct gm_settings *s);
