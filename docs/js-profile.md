# Game-Maker profile for Sandbox JS (prototype, host only)

`src/js/profile_gm.c` implements the Sandbox JS `struct jsrt_profile` (sandbox-kernel `js/docs/profiles.md`) over
this repository's play core: `play` finds the `.GAM` and starts `gs_start`, `step`/`wake`/`key`/`draw`/`ticks` wrap
the session, `picture` reads a game folder's title screen (`gm_title_picture`), `end` releases the play core and ends
the sound. Files go through the core's `jsrt_fs_read`/`jsrt_fs_list` (the app's path policy).

`jsprof.mk` builds the Sandbox JS core with it. Sandbox JS is not vendored: point `SANDBOX_JS_DIR` at a sandbox-kernel
checkout's `js/`. Prototype pin: sandbox-kernel `2953656` (branch `feature/sandbox-js-followups-1113`, js 0.1.1).

```sh
make -f jsprof.mk host SANDBOX_JS_DIR=~/dev/sandbox-kernel/js   # build/jsprof/host/core/host/jsrun
make -f jsprof.mk size SANDBOX_JS_DIR=~/dev/sandbox-kernel/js   # build/jsprof/board/core/JS.ELF and its size report
tests/js/profile_cmp.sh build/host/jsrun build/jsprof/host/core/host/jsrun "$PWD" /tmp/out   # old JS copy vs profile
```

The play core's own `gif.c` and `font_data.c` are left out of the link: the Sandbox JS core has the same symbols.
Nothing here changes `GMOS.ELF` or the old `JS.ELF` (`make js`).

## Host checks (2026-11-13)

`tests/js/profile_snd.sh <old jsrun> <profile jsrun> "$PWD" <out dir> [game folder]` runs the apps in `tests/js/apps/`
(GPLAY, GESC, GPIC, GLEAK) on a virtual clock against the old JS copy and the profile build. It passes on `tests/games/DICE`
and on the biggest Sandbox game on the dev card (8.4 MB of files; the checks do not need it, it is just the large case).

- **Sound.** The board has no audio driver; a game's sound is the `SND` log lines (`src/sound.h`), and the host's audio ring
  and underrun counters stay at zero for both builds (`--devstat`: `audio_open 0`). Over 20 s with music and effects the
  lines are identical (11 lines: game, three music starts, voc effects). One difference, in the profile's favor: when the
  app ends the game with `stop()`, the old copy sent no `SND end`, so the music kept playing on the PC side; the profile's
  `end()` calls `snd_end`, which sends `SND stopall` and `SND end`. Esc (the game ends itself) is identical in both.
- **Picture hook.** An `<img src>` on a game folder renders the same framebuffer as the old copy (same CRC, 640x480x32 shot,
  not blank).
- **Screen.** The 8 bpp framebuffer mid-game and the page after the stop match the old copy byte for byte.
- **Leaks.** One app playing and stopping the game 50 times: `mallocSize` 217,746 bytes after the first stop, 218,058 after
  the 50th (flat after the first few), `heap_blocks 0`. The app started and stopped 50 times mid-game (`--cycles 50`):
  `leaks 0 open 0`, blocks 186 first and last, and 50 `SND end` lines. `JSRT_H_GAME` is released each time.
- **Heap cap.** The game's memory does not count against the app's 1.5 MiB QuickJS heap: `heap_peak` is 385,794 bytes
  whether the app plays 20 s of the large game or only starts it, and the same game plays with `--heap 400000`. The play
  core allocates nothing from the heap: its state is static (`.bss` in the board objects: `session.o` 1.24 MB, `game.o`
  308 KB including the world, `thumb.o` 1.56 MB for the thumbnail cache), plus the code in the ELF. That is about 3.1 MB
  the JS.ELF carries beside the 1.5 MiB heap and the 3 MiB malloc pool; whether the board's RAM holds all of it is a size
  question for the on-board test (`make -f jsprof.mk size`). The profile core's idle heap is 212,818 bytes against 142,912 for
  the old copy (the shared core's prelude and picker), well inside the cap.
- Not covered: a sound that is actually played (needs the board or the PC side), and the board's timing.
