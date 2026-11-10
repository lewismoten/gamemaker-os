/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * PLAYGAME's Save Game (F5) and Load Game (F6) screens during a level: SaveGame, LoadGame, SelectGameToSave and
 * SelectGameToLoad (code/GM/PLAYGAME.C 1659-1892), drawn in place on the session's 320 x 200 picture. Ten slots,
 * named in SAVNAMES.DAT (10 x 21 bytes), each GAME<n>.SAV (savefile.h's DOS records), all under /OS/SAVES/<key>/ as
 * the Top Ten is. Pure: time and input come in as arguments, so the host tests drive it as the board does.
 * Claude Sonnet 5.5 wrote this file. */
#pragma once
#include <stdint.h>
#include "savefile.h"
#include "session.h"
#define SP_SLOTS 10u
#define SP_NAME_MAX 20u        /* GGet's maxlen */
#define SP_PAUSE_NS 2000000000ull /* PauseTimeKey(2) */
enum {SP_SAVE, SP_LOAD};
enum {SP_PICK, SP_NAME, SP_MSG};
enum {SP_DRAW = 1, SP_DONE = 2, SP_LOADED = 4, SP_FATAL = 8};
struct sp_menu {
  int mode, stage, sel, shift, ctrl, cur_on, cur_drawn;
  int names_error; /* unread names must never be replaced with blank records */
  const struct save_ops *ops; char key[9];
  struct gs_session *s;
  char names[SP_SLOTS][SP_NAME_MAX + 1];
  uint8_t name_col, mark_col, msg_col, prev, back;
  uint32_t len;
  uint64_t t0, blink;
};
/* Opens the screen over the level: the game's Load or Save picture (File(0,9) or (0,10)), else black with the title
   (and, to save, the ">" marks) in the level's palette. */
void sp_start(struct sp_menu *m, int mode, const struct save_ops *ops, const char *key, struct gs_session *s, uint64_t now);
/* One key event (Linux codes). SP_DONE ends the screen; SP_LOADED when a load replaced the play (the caller gs_resumes
   either way); SP_FATAL when a load failed half way and the state before it could not be put back either. */
int sp_event(struct sp_menu *m, uint16_t type, uint16_t code, int32_t value, uint64_t now);
/* Time moves on: the name cursor blinks, the message's two seconds end. */
int sp_step(struct sp_menu *m, uint64_t now);
uint64_t sp_wake(const struct sp_menu *m, uint64_t now);
