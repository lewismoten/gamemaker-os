# Building and installing GMOS.ELF

`GMOS.ELF` is the first Game-Maker OS program. It runs in ARM User mode on
[sandbox-kernel](https://github.com/lewismoten/sandbox-kernel) and uses only the
kernel's published SDK.

## The SDK

`sdk/` is a copy of the kernel repository's `sdk/` folder. `sdk/PINNED` names the
kernel commit it came from. To move to a newer SDK, run
`scripts/vendor_sdk.sh <kernel checkout> [commit]`, then build and test. Nothing
else from the kernel is used, and the kernel knows nothing about Game-Maker
files: to it, `/sd/GAMES` is an ordinary folder.

The program needs the kernel's M1 slice 3 or later: `/dev/input/event0`,
`getdents` and the `health` heartbeat. Playing a game needs ABI 1.1 (M1 slice 4):
`clock_gettime` and `sleep_until`. On an older kernel the list works and Enter
logs `GMOS playing needs the kernel's clock (ABI 1.1)`.

`third_party/gmcore/` is the Game-Maker play core from Game-Maker: Sandbox
Edition, copied unchanged with that repository's license;
`scripts/vendor_gmcore.sh <checkout> [commit]` refreshes it and its `PINNED`.

## Build

```
make            # build/GMOS.ELF (Arm GNU toolchain; set TOOLCHAIN to its prefix)
make test       # host tests; renders build/host/games-*.png and play-*.png
make test-asan  # the host tests and a fuzz pass under AddressSanitizer and UBSan
```

The host tests build the title reader, library index, games page and drawing code
(`src/gam.c`, `src/library.c`, `src/menu.c`, `src/thumb.c`, `src/view.c`,
`src/font_data.c`) and the game player (`src/game.cpp`, `src/gif.c`,
`src/session.c`) as a host library and render frames to PNG; see
[play.md](play.md) for the game tests. `tests/gmos_page.py` builds the games page
on the host as the board does. Set `GMOS_GAMES` to a folder of game folders to
title real games as well, and `GMOS_CARD` to a card folder written by the Sandbox
Edition's `web/scripts/gmosCard.ts` to check the frames the board logged
(`tests/data/board-list.txt`).

`make test-asan` builds the same library, `gmplay` and a fuzz driver
(`tests/host/fuzz.c`) in `build/asan/` with AddressSanitizer and
UndefinedBehaviorSanitizer, runs the fuzz driver (library index lines that are long,
broken or invalid UTF-8; more games than the list holds; truncated and mutated GIFs,
FLIs, LSTs and .GAMs through the title-screen picker; the page drawn in every
state), then the whole host suite with the sanitizer runtime preloaded into
Python. Any overflow or undefined behavior stops it with a report. On macOS the
python.org Python can't load a sanitizer (hardened runtime), so
`tools/asan_python.sh` picks an unhardened one such as Homebrew's. Options:
`FUZZ_ROUNDS=5000` for a longer pass, and `FUZZ_GAMES="<folder> ..."` to list,
title-picture and draw every game in folders of game folders as well.

`tools/render_list.py --card <folder> [--keys tab,right,enter] --out page.png`
renders the games page for any card folder and prints the frame's CRC-32, the
number the board logs as `GMOS list frame crc` for the same frame. The font is edited in
`tools/font.txt`; `python3 tools/make_font.py` regenerates `src/font_data.c`.

## Install and run

With the board on USB (the kernel's `scripts/usb_kernel.py`, from a kernel
checkout; replace `<port>` with the board's serial port):

```
usb_kernel.py <port> put build/GMOS.ELF                 # /OS/GMOS.ELF
usb_kernel.py <port> putgame <game folder> DICE         # every file, as /GAMES/DICE/...
usb_kernel.py <port> put LIBRARY.JSL                    # the library index, as /OS/LIBRARY.JSL
usb_kernel.py <port> run GMOS.ELF                       # start it and show the log
usb_kernel.py <port> key down down enter                # press keys
usb_kernel.py <port> key esc                            # quit
```

A packed game is one file: `put DICE.GMP --name POCKET/DICE.GMP` (or
`GAMES/DICE.GMP`), up to 1 MiB; see [gmp.md](gmp.md).

USB `put`/`putgame` retains its 8.3 uppercase installation contract. Desktop
copying is different: with the paired ABI 1.5 kernel, copy loose game folders,
stored/deflated ZIPs or plain-ZIP SFX EXEs into `GAMES`, keeping long names.
See [drag-drop.md](drag-drop.md) for layouts, bounds and qualification status.

## What it does

1. Reports readiness to the kernel (`health`), then keeps a heartbeat on every
   pass of its loop.
2. Lists `/sd/GAMES`: each folder holding a `.GAM` file is a game, titled from
   the start scene's description in that file (an empty one shows the folder
   name). Then the packed games, `.GMP` files in `/sd/GAMES` and
   `/sd/POCKET` (one file a game, shared with the Pocket Player; see
   [gmp.md](gmp.md)), titled the same way from the `.GAM` inside.
3. Reads the library index, `/sd/GAMES/LIBRARY.JSL` (or `/sd/OS/LIBRARY.JSL`,
   where `put` can install it), if there is one: the Pocket Player's
   `library.jsonl` format from Game-Maker: Sandbox Edition, one JSON object a
   line, written there by its `web/scripts/gmosCard.ts`. A line names a game by
   its `.GAM`'s CRC-32 (else its folder, `locator`, or `.GAM` name, `dosName`)
   and gives its title, category, company, year and blurb. Games it does not
   list go in an Other tab.
4. Switches the screen to 960x540 at 16 bits a pixel (RGB565; ABI 1.2), which
   the GPU scales to the HDMI mode: exactly twice in each direction on a 1080p
   monitor, so the page fills it with square pixels. Draws the games page after
   the Sandbox Edition web site's Games page in its
   dark theme: the header, category tabs with counts (Starters, Sandbox,
   Bizarre, CD, Other; only those with games), the tab's line, eight game cards a
   page (four across) with each game's title screen (218 by 136), title and company, the
   selected card outlined in blue, and the key help. Each card's title screen is
   read from the game's own files between key presses (about 100 ms a game on
   the Pi Zero W, the last 24 kept), the way the web page picks it: scene 0's
   title graphic (the picture a `.LST` holds longest, an animation's last
   frame, a backdrop's window), else its menu picture. Once every card on the
   page is drawn it logs `GMOS list frame crc <n>`.
5. Reads `/dev/input/event0` without blocking. On the grid the arrows move
   (repeats too), PgUp and PgDn turn the page, Home and End go to the first and
   last card, Tab and Shift+Tab (or `]` and `[`) change the tab, and Enter opens
   the details view: the title screen at twice its size (640 by 400), the category, title,
   company and year, the blurb, and Play and Back buttons (Left, Right or Tab
   pick one; Enter or Space presses it; Esc goes back). Play logs
   `GMOS chose <DIR> "<title>"` and plays the game ([play.md](play.md)); the
   game's end or Esc comes back to the details view. Esc or Q on the grid logs
   `GMOS quit` and exits 0, leaving the last frame on the screen. After
   `SYN_DROPPED` it reads the current state and goes on. If the device goes away
   it logs `GMOS input removed` and opens it again once it is back.

Between input reads the list sleeps 10 ms (`sleep_until`); it will block on
input once the kernel has `poll`.

## Optional e-ink status (ABI 1.9)

GMOS uses `/dev/eink` only when the startup record reports major 1, minor 9
or newer. It opens write-only and first submits a 250x122 boot page with
`GAME-MAKER OS`, the same castle as the HDMI splash, and `LOADING LIBRARY`.
The top-right error indicator area remains white.

Once scanning finishes it clears the page, then installs the title
`GAME-MAKER OS` and a 40x40 castle mark. The API's free-text build line shows
`N GAMES` in the library, the current game's title while playing (capitalized,
clipped to 16 bytes), or `SAVING` while the F5 save screen is open. Leaving
F5 restores the game title; returning to the library restores the count.
The kernel's uptime, USB and error lines remain visible in title mode.

Page, clear, title and retry attempts share a minimum gap of 2.1 seconds,
which stays below both the one-second and 30-updates-per-minute limits.
The clear is required because a full page covers a title; the kernel's status
screen can appear briefly between clear and title. Changes are coalesced to
the latest state, so a screen dismissed within the cadence gap may never
appear on paper. No e-ink operation sleeps or waits for a refresh. `EAGAIN`
defers a retry to a later heartbeat; other device errors (including `ENODEV`)
close the optional handle and disable updates silently for this run. Older
kernels and failed opens leave play available without e-ink.

`tests/test_eink.py` runs the production client with a host fake for the
write-only device and monotonic clock. The SDK remains an unmodified copy
from the commit in `sdk/PINNED`; the host transport lives under `tests/host/`.
