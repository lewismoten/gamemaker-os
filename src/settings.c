/* SPDX-License-Identifier: MIT */
#include "settings.h"
#include <string.h>
void gm_settings_defaults(struct gm_settings *s) {
  s->screen_off = 1; s->power_saving = 1; s->ui_sounds = 1;
}
/* "GMOS 1\nS P\n" (11 bytes, before UI sounds) or "GMOS 1\nS P U\n" (13 bytes). */
int gm_settings_load(struct gm_settings *s, const struct save_ops *o) {
  char raw[14]; uint32_t n = 0;
  gm_settings_defaults(s);
  int e = save_get(o, "GMOS", "SETTINGS.DAT", raw, sizeof raw, &n);
  if (e) return e == SAVE_ENOENT ? 0 : e;
  if ((n != 11 && n != 13) || memcmp(raw, "GMOS 1\n", 7) ||
      raw[7] < '0' || raw[7] > '3' || raw[8] != ' ' ||
      (raw[9] != '0' && raw[9] != '1')) return SAVE_EINVAL;
  if (n == 11 ? raw[10] != '\n' : (raw[10] != ' ' || (raw[11] != '0' && raw[11] != '1') || raw[12] != '\n')) return SAVE_EINVAL;
  s->screen_off = (unsigned)(raw[7] - '0');
  s->power_saving = (unsigned)(raw[9] - '0');
  if (n == 13) s->ui_sounds = (unsigned)(raw[11] - '0');
  return 0;
}
int gm_settings_save(const struct gm_settings *s, const struct save_ops *o) {
  char raw[] = "GMOS 1\n1 1 1\n";
  if (s->screen_off > 3 || s->power_saving > 1 || s->ui_sounds > 1) return SAVE_EINVAL;
  raw[7] = (char)('0' + s->screen_off); raw[9] = (char)('0' + s->power_saving); raw[11] = (char)('0' + s->ui_sounds);
  return save_put(o, "GMOS", "SETTINGS.DAT", raw, sizeof raw - 1);
}
uint32_t gm_settings_blank_seconds(const struct gm_settings *s) {
  static const uint32_t seconds[] = {300, 600, 1800, 0};
  return seconds[s->screen_off < 4 ? s->screen_off : 1];
}
const char *gm_settings_screen_label(const struct gm_settings *s) {
  static const char *const labels[] = {"Screen off after: 5 min", "Screen off after: 10 min",
    "Screen off after: 30 min", "Screen off after: Never"};
  return labels[s->screen_off < 4 ? s->screen_off : 1];
}
