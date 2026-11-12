/* SPDX-License-Identifier: MIT; production remote controls, storage and SDK. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sandbox/syscall.h>
#include "menu.h"
#include "awake.h"

static struct gm_menu menu;
static char stored[32], staged[32];
static uint32_t stored_n, staged_n, read_at;
static unsigned calls;
static uint64_t slow, blank;
static int fail_rename;
int32_t sbx_syscall(uint32_t call, void *record, uint32_t bytes) {
  assert(call == SBX_CALL_POWER_TIMEOUTS && bytes == sizeof(sbx_power_timeouts_rec));
  const sbx_power_timeouts_rec *r = record;
  assert(r->h.size == bytes && !r->h.flags && !r->h.reserved0 && !r->h.reserved1);
  calls++; slow = r->slowdown_seconds; blank = r->blank_seconds;
  return 0;
}
static int open_write(void *ctx, const char *path, uint64_t *fd) {
  (void)ctx; assert(!strcmp(path, "/sd/OS/SAVES/GMOS/SETTINGS.TMP"));
  staged_n = 0; *fd = 1; return 0;
}
static int write_data(void *ctx, uint64_t fd, const void *p, uint32_t n, uint32_t *done) {
  (void)ctx; assert(fd == 1); if (n > 3) n = 3;
  assert(staged_n + n <= sizeof staged);
  memcpy(staged + staged_n, p, n); staged_n += n; *done = n; return 0;
}
static int finish(void *ctx, uint64_t fd) {(void)ctx; assert(fd == 1); return 0;}
static int rename_data(void *ctx, const char *from, const char *to) {
  (void)ctx; assert(!strcmp(from, "/sd/OS/SAVES/GMOS/SETTINGS.TMP"));
  assert(!strcmp(to, "/sd/OS/SAVES/GMOS/SETTINGS.DAT"));
  if (fail_rename) return -SBX_EIO;
  memcpy(stored, staged, staged_n); stored_n = staged_n; return 0;
}
static int open_read(void *ctx, const char *path, uint64_t *fd) {
  (void)ctx; assert(!strcmp(path, "/sd/OS/SAVES/GMOS/SETTINGS.DAT"));
  read_at = 0; *fd = 1; return stored_n ? 0 : SAVE_ENOENT;
}
static int read_data(void *ctx, uint64_t fd, void *p, uint32_t n, uint32_t *got) {
  (void)ctx; assert(fd == 1); if (n > 2) n = 2;
  if (n > stored_n - read_at) n = stored_n - read_at;
  memcpy(p, stored + read_at, n); read_at += n; *got = n; return 0;
}
static const struct save_ops ops = {0, open_write, write_data, finish, finish,
  rename_data, open_read, read_data};
static int key(uint16_t code) {return gm_event(&menu, 1, code, 1);}
static void boot(unsigned major, unsigned minor) {
  gm_finish(&menu);
  assert(!gm_settings_load(&menu.settings, &ops));
  awake_init(major, minor);
  assert(!awake_configure(&menu.settings));
}
int main(void) {
  boot(1, 11); assert(calls == 1 && slow == 60 && blank == 600);
  /* Empty library too: Back, Up wraps to Power, Left reaches Screen. */
  assert(key(1) == GM_MOVED); key(103); key(105);
  assert(menu.option == GM_OPT_SCREEN);
  const unsigned expected[] = {1800, 0, 300, 600};
  for (unsigned saving = 0; saving < 2; saving++) {
    for (unsigned i = 0; i < 4; i++) {
      assert(key(28) == GM_SETTINGS && menu.options_active);
      assert(gm_event(&menu, 1, 28, 2) == GM_NONE); /* no repeat toggles */
      assert(!gm_settings_save(&menu.settings, &ops));
      struct gm_settings restored;
      assert(!gm_settings_load(&restored, &ops));
      assert(restored.screen_off == menu.settings.screen_off);
      assert(restored.power_saving == menu.settings.power_saving);
      assert(!awake_configure(&restored));
      assert(blank == expected[i] && slow == (saving ? 0 : 60));
    }
    key(106); assert(key(28) == GM_SETTINGS); key(105);
  }
  puts("remote controls, persistence, all timeout pairs and Never PASS");
  /* Persist Never + Off and reload into a fresh menu as at boot. */
  key(28); key(28); key(106); key(28);
  assert(!gm_settings_save(&menu.settings, &ops));
  boot(1, 11); assert(!slow && !blank);
  for (unsigned minor = 0; minor < 11; minor++) {
    unsigned before = calls; boot(1, minor); assert(calls == before);
  }
  unsigned before = calls; boot(2, 11); assert(calls == before);
  puts("boot reapplication and quiet ABI gating PASS");
  fail_rename = 1; gm_settings_defaults(&menu.settings);
  assert(gm_settings_save(&menu.settings, &ops) == -SBX_EIO);
  fail_rename = 0; boot(1, 11); assert(!slow && !blank);
  stored[7] = '9';
  assert(gm_settings_load(&menu.settings, &ops) == SAVE_EINVAL);
  assert(menu.settings.screen_off == 1 && menu.settings.power_saving == 1);
  stored_n = 3;
  assert(gm_settings_load(&menu.settings, &ops) == SAVE_EINVAL);
  /* UI sounds: saved with the others; a file from before the setting (11 bytes) loads with sounds on. */
  memcpy(stored, "GMOS 1\n1 1\n", 11); stored_n = 11;
  boot(1, 11); menu.settings.ui_sounds = 0; assert(!gm_settings_save(&menu.settings, &ops) && stored_n == 13);
  boot(1, 11); assert(menu.settings.ui_sounds == 0);
  memcpy(stored, "GMOS 1\n2 0\n", 11); stored_n = 11;
  assert(!gm_settings_load(&menu.settings, &ops) && menu.settings.ui_sounds == 1 && menu.settings.screen_off == 2);
  puts("UI sounds setting persists; an older file loads with sounds on PASS");
  puts("atomic save failure and invalid settings defaults PASS");
  return 0;
}
