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
