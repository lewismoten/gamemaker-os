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

Folder and file names must be 8.3 and are sent in upper case. Games can also be
copied onto the card from any computer into a `GAMES` folder; long folder names
need the kernel's long-name support, which comes later.

## What it does

1. Reports readiness to the kernel (`health`), then keeps a heartbeat on every
   pass of its loop.
2. Lists `/sd/GAMES`: each folder holding a `.GAM` file is a game, titled from
   the start scene's description in that file (an empty one shows the folder
   name).
3. Reads the library index, `/sd/GAMES/LIBRARY.JSL` (or `/sd/OS/LIBRARY.JSL`,
   where `put` can install it), if there is one: the Pocket Player's
   `library.jsonl` format from Game-Maker: Sandbox Edition, one JSON object a
   line, written there by its `web/scripts/gmosCard.ts`. A line names a game by
   its `.GAM`'s CRC-32 (else its folder, `locator`, or `.GAM` name, `dosName`)
   and gives its title, category, company, year and blurb. Games it does not
   list go in an Other tab.
4. Draws the games page after the Sandbox Edition web site's Games page in its
   dark theme: the header, category tabs with counts (Starters, Sandbox,
   Bizarre, CD, Other; only those with games), the tab's line, six game cards a
   page (three across) with each game's title screen, title and company, the
   selected card outlined in blue, and the key help. Each card's title screen is
   read from the game's own files between key presses (about 100 ms a game on
   the Pi Zero W, the last 24 kept), the way the web page picks it: scene 0's
   title graphic (the picture a `.LST` holds longest, an animation's last
   frame, a backdrop's window), else its menu picture. Once every card on the
   page is drawn it logs `GMOS list frame crc <n>`.
5. Reads `/dev/input/event0` without blocking. On the grid the arrows move
   (repeats too), PgUp and PgDn turn the page, Home and End go to the first and
   last card, Tab and Shift+Tab (or `]` and `[`) change the tab, and Enter opens
   the details view: the title screen at full size, the category, title,
   company and year, the blurb, and Play and Back buttons (Left, Right or Tab
   pick one; Enter or Space presses it; Esc goes back). Play logs
   `GMOS chose <DIR> "<title>"` and plays the game ([play.md](play.md)); the
   game's end or Esc comes back to the details view. Esc or Q on the grid logs
   `GMOS quit` and exits 0, leaving the last frame on the screen. After
   `SYN_DROPPED` it reads the current state and goes on. If the device goes away
   it logs `GMOS input removed` and opens it again once it is back.

Between input reads the list sleeps 10 ms (`sleep_until`); it will block on
input once the kernel has `poll`.
