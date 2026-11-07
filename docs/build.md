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
`getdents` and the `health` heartbeat.

## Build

```
make            # build/GMOS.ELF (Arm GNU toolchain; set TOOLCHAIN to its prefix)
make test       # host tests; renders build/host/games-list*.png
```

The host tests build the title reader, list and drawing code (`src/gam.c`,
`src/menu.c`, `src/view.c`, `src/font_data.c`) as a host library and render frames
to PNG. One test renders the frame the board showed after Down, Down, Enter and
checks it against the screen checksum the kernel logged. Set `GMOS_GAMES` to a
folder of game folders to title real games as well. The font is edited in
`tools/font.txt`; `python3 tools/make_font.py` regenerates `src/font_data.c`.

## Install and run

With the board on USB (the kernel's `scripts/usb_kernel.py`, from a kernel
checkout; replace `<port>` with the board's serial port):

```
usb_kernel.py <port> put build/GMOS.ELF                 # /OS/GMOS.ELF
usb_kernel.py <port> putgame <game folder> DICE         # every file, as /GAMES/DICE/...
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
2. Lists `/sd/GAMES`: each folder holding a `.GAM` file is a game. The title is
   the start scene's description in that file, as the Sandbox Edition web player
   shows it; an empty description shows the folder name. The list is sorted by
   title.
3. Draws "GAME-MAKER OS", the list (nine rows, scrolling), the folder names, the
   key help and the position, and presents the frame.
4. Reads `/dev/input/event0` without blocking: Up and Down move (repeats too),
   Enter logs `GMOS chose <DIR> "<title>"` and shows it in the footer, Esc or Q
   logs `GMOS quit` and exits 0, leaving the last frame on the screen. After
   `SYN_DROPPED` it reads the current state and goes on. If the device goes away
   it logs `GMOS input removed` and opens it again once it is back.

The program busy-waits between events; it will sleep once the kernel has `poll`.
