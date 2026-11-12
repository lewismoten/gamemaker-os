/* SPDX-License-Identifier: MIT; save-menu names read-error regression probes. */
#include <assert.h>
#include <stdlib.h>
#include "hostio.h"
#include "saveplay.h"
#include "text.h"

static uint8_t names[210], original[210], pending[210];
static unsigned at, names_writes, slot_writes;
static int scenario;
static const char *last_message;
static struct gs_session session;
static struct sp_menu menu;

static void say(const char *s) { last_message = s; }
static int open_read(void *ctx, const char *path, uint64_t *fd) {
  (void)ctx; assert(strstr(path, "SAVNAMES.DAT"));
  if (scenario == 0) return -5; /* transient open error */
  if (scenario == 2) return SAVE_ENOENT;
  *fd = 1; at = 0; return 0;
}
static int read_names(void *ctx, uint64_t fd, void *buf, uint32_t cap, uint32_t *got) {
  (void)ctx; assert(fd == 1);
  if (scenario == 1 && at) return -5; /* a partial read, then EIO */
  unsigned size = scenario == 3 ? 42 : sizeof names;
  unsigned n = size - at;
  if (scenario == 1) n = 21;
  if (n > cap) n = cap;
  if (!n && scenario == 4) {*(uint8_t *)buf = 1; *got = 1; return 0;} /* DOS ignores a trailer */
  memcpy(buf, names + at, n); at += n; *got = n; return 0;
}
static int open_write(void *ctx, const char *path, uint64_t *fd) {
  (void)ctx; *fd = strstr(path, "SAVNAMES.TMP") ? 1 : 2; return 0;
}
static int write_data(void *ctx, uint64_t fd, const void *buf, uint32_t n, uint32_t *done) {
  (void)ctx;
  if (fd == 1) {
    names_writes++;
    if (scenario == 6) return -5;
    assert(n == sizeof pending); memcpy(pending, buf, n);
  } else slot_writes++;
  *done = n; return 0;
}
static int finish(void *ctx, uint64_t fd) {(void)ctx; (void)fd; return 0;}
static int replace(void *ctx, const char *src, const char *dst) {
  (void)ctx; (void)src;
  if (strstr(dst, "SAVNAMES.DAT")) memcpy(names, pending, sizeof names);
  return 0;
}

int main(int argc, char **argv) {
  assert(argc == 2); scenario = atoi(argv[1]);
  assert(gp_open(host_read, "tests/games/DICE", "DICE.GAM") > 0);
  assert(gp_begin() == 0);
  session.read = host_read; session.user = "tests/games/DICE"; session.say = say;
  struct save_ops ops = {NULL, open_write, write_data, finish, finish, replace, open_read, read_names};
  for (unsigned i = 0; i < 10; i++) snprintf((char *)names + i * 21, 21, "Existing slot %u", i);
  memcpy(original, names, sizeof names);
  sp_start(&menu, SP_SAVE, &ops, "DICE1234", &session, 0);
  sp_event(&menu, 1, 28, 1, 0);
  if(scenario==7) {
    /* Remote-only: Clear old name, enter A, Done saves. */
    for(unsigned i=0;i<4;i++)sp_event(&menu,1,108,1,0);
    sp_event(&menu,1,106,1,0);sp_event(&menu,1,106,1,0);sp_event(&menu,1,28,1,0);
    for(unsigned i=0;i<4;i++)sp_event(&menu,1,103,1,0);
    for(unsigned i=0;i<6;i++)sp_event(&menu,1,105,1,0);
    sp_event(&menu,1,28,1,0);
    assert(!strcmp(menu.names[0],"A"));
    for(unsigned i=0;i<4;i++)sp_event(&menu,1,108,1,0);
    for(unsigned i=0;i<3;i++)sp_event(&menu,1,106,1,0);
    sp_event(&menu,1,28,1,0);
    assert(menu.stage==SP_MSG && slot_writes==1 && !strcmp((char *)names,"A"));
    for(unsigned i=1;i<10;i++)assert(!memcmp(names+i*21,original+i*21,21));
    puts("ALL PASS");return 0;
  }
  /* Replace the first slot name with 'a' through the production menu. */
  for (unsigned i = 0; i < 20; i++) sp_event(&menu, 1, 14, 1, 0);
  sp_event(&menu, 1, 30, 1, 0);
  sp_event(&menu, 1, 28, 1, 0);
  assert(menu.stage == SP_MSG && slot_writes == 1);
  if (scenario <= 1 || scenario == 6) {
    assert(names_writes == (scenario == 6 ? 1u : 0u));
    assert(memcmp(names, original, sizeof names) == 0);
    assert(strcmp(last_message, "GMOS game saved; name not saved") == 0);
    /* Check the player-visible message too, independently of the log callback. */
    uint8_t expected[GP_W * GP_H] = {0};
    const char *msg = "Game saved; name not saved!";
    gp_write(expected, 40, 185, menu.msg_col, msg, (unsigned)strlen(msg));
    assert(memcmp(session.px + 185 * GP_W, expected + 185 * GP_W, 8 * GP_W) == 0);
  } else {
    assert(names_writes == 1 && strcmp((char *)names, "a") == 0);
    for (unsigned i = 1; i < 10; i++) {
      if (scenario == 2 || (scenario == 3 && i >= 2)) assert(names[i * 21] == 0);
      else assert(memcmp(names + i * 21, original + i * 21, 21) == 0);
    }
    assert(strcmp(last_message, "GMOS game saved") == 0);
  }
  puts("ALL PASS"); return 0;
}
