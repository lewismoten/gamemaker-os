# Playing a game

Enter on the games list plays the chosen game from its original DOS files in
`/sd/GAMES/<DIR>`, the same files the list reads its title from. Nothing is
converted or packed first.

## One play core, shared

The rules of play (the hero, keys, blocks, scrolling, monsters, links between
levels) are `third_party/gmcore/gmcore.h` and `gmmon.h`, the play core of the
Pocket Player in [Game-Maker: Sandbox Edition](https://github.com/lewismoten/GameMaker-SandboxEdition)
(`hardware/pocket-player/src`). That core is a port of the Sandbox Edition's web
engine, which follows RSD's `PLAYGAME.C`. It is header-only and holds no board
code, so the same files build the ESP32 firmware there and `GMOS.ELF` here.

`third_party/gmcore` is a copy pinned by commit (`PINNED`), never edited here.
`scripts/vendor_gmcore.sh <checkout> [commit]` copies the two headers and that
repository's `LICENSE` and records the commit. A fix to the core goes into the
Sandbox Edition, then comes here with the script.

What differs is how each side feeds the core. The Pocket Player reads files that
a packer squeezed for its small memory. `src/game.cpp` reads the DOS files
directly:

- the `.GAM` (version 3.00 files; older ones are not read yet): a level is a
  scene of type 10. The first level starts where scene 0's first link points;
  links to scene 1 win the game.
- each level's `.MAP` (block layer, monster layer and the 200 placed monsters),
  `.BBL`, `.PAL` and `.SND`, its hero (`.CHR`, `.CBL`; a level without them keeps
  the last hero) and its monsters (`.MON`, `.MBL`).

All buffers are static (about 600 KB); there is no allocator. The core is C++17
built without exceptions, RTTI or static constructors; `src/libc.c` supplies
`abs`.

## A game, start to end

`src/session.c` runs a game as the Pocket Player does:

1. The title (scene 0's title graphic), then the first level's intro.
2. The level, a tick every 1/18.2065 s (the PC timer's 1193182 / 65536 Hz). A
   slow step runs up to 4 ticks to catch up and drops time beyond that.
3. On a link: the level's exit pictures, the next level's intro, the next level.
   After a death: the level's intro again, unless no lives are left.
4. Winning shows the level's exit and the won pictures; losing the last life
   shows the lost pictures. Then the games list.

Pictures are the `.GIF` files the slots name, directly or through a `.LST` (one
picture per line with its pause in seconds; none waits for a key). A key moves
on early. `src/gif.c` is a C port of the Sandbox Edition's GIF decoder.
Animations (`.FLI`), backdrops (`.BKD`) and text pages (`.TXT`) are skipped and
logged for now.

Keys arrive as Linux key codes and reach the game as DOS scan codes: the arrows,
Home, End, Page Up and Page Down (the diagonals) as the number pad's, and the
rest of the keyboard as it is (Space, Enter, letters). Esc always goes back to
the list.

## The screen

The play view is PLAYGAME's 312 by 196 (the CRT controller showed no more while
playing) at twice the size: 624 by 392, centered on the 640 by 480 screen. A
picture between levels is 320 by 200 at twice the size, 640 by 400 from row 40.
Colors are the 6-bit VGA palette widened to 8 bits as the Sandbox Edition does
it.

## Timing on the board

`main.c` reads the kernel's clock (`clock_gettime`), runs the session and
sleeps until the next tick (`sleep_until`), so the program does not spin. Each
game's clock starts once its level is read and drawn. Every 5 s it logs the
frames shown and the slowest step, draw and wake-up. Roll the Dice on a Pi
Zero W (kernel generation 15, 2026-11-07):

```
LOG OS: GMOS card title.gif, 12 s
LOG OS: GMOS card frame crc 1276733601
LOG OS: GMOS card one.gif, 3 s
LOG OS: GMOS card frame crc 334454467
LOG OS: GMOS level 1
LOG OS: GMOS frame crc 4226055335 tick 1
LOG OS: GMOS frame crc 3551797356 tick 18
LOG OS: GMOS 5 s: 92 frames, tick 133, step max 2946 us, draw max 23406 us, late max 4691 us
LOG OS: GMOS 5 s: 92 frames, tick 225, step max 2895 us, draw max 23360 us, late max 1051 us
```

A tick takes about 3 ms and drawing the doubled screen and presenting it about
23 ms, so a 55 ms tick has room to spare. The first window's wake-ups are later
(up to 22 ms) because of the checksums logged then.

## How it is checked

- `tests/test_play.py` plays `tests/games/DICE` (the Starter "Roll the Dice"):
  1200 ticks of walking and jumping give the same state after every tick
  (position, sequence, scroll, score, hit points, lives, counters and a hash of
  the view) as the Pocket Player's own host test on its packed copy of the game
  (`tests/data/dice-pocket.txt`). The GIF pictures decode to the same palette and
  pixels as its packed cards (`dice-cards.txt`). The session's cards, tick rate,
  catch-up, keys, Esc and layout are checked too, and the frames the board
  reported (above) are rendered and compared by CRC. PNG files go to
  `build/host/play-*.png`.
- `tools/pocket_reference.py <checkout> compare` runs both players on every
  Starter that a Sandbox Edition checkout has built; on 2026-11-07, 104 of 106
  were the same for all 1200 ticks. The other two had changed since the Pocket
  Player's data was last packed. `... write` regenerates the two reference
  files.
- `tools/board_play.py <kernel checkout> <port> [--put]` installs and plays Roll
  the Dice on the board over USB and saves the log. `--check <log>` replays the
  game keys the board logged on the host and compares the view hash of each
  tick: all 237 matched in the run above.

## Next

- Sound: PC speaker effects first (the core already queues them), then Sound
  Blaster clips and music.
- Real input: a USB keyboard or gamepad, or a remote (HDMI-CEC, IR), in place of
  the synthetic device.
- The animated and text pages between levels, and the fades.
- Older `.GAM` files (before version 3.00).
- Presenting only what changed, to cut the 23 ms draw.
