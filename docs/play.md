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

- the `.GAM`: a level is a scene of type 10. The first level starts where
  scene 0's first link points; links to scene 1 win the game. A file older
  than version 3.00 (20 fixed 330-byte scenes, names without extensions, as
  most shareware games on RSD's CD have) is first converted to 3.00 bytes the
  way the Sandbox Edition's `parseGame` and `serializeGame` convert it, after
  RSD's `GameClass::Load`: the slot decides each name's extension, and such a
  game has no title, intro or exit pages, and a `.BKD` won picture.
- each level's `.MAP` (block layer, monster layer and the 200 placed monsters),
  `.BBL`, `.PAL` and `.SND`, its hero (`.CHR`, `.CBL`; a level without them keeps
  the last hero) and its monsters (`.MON`, `.MBL`).

All buffers are static (about 600 KB); there is no allocator. The core is C++17
built without exceptions, RTTI or static constructors; `src/libc.c` supplies
`abs`.

## A game, start to end

`src/session.c` runs a game as the Pocket Player does:

1. The title (scene 0's title graphic), then the first level's intro on a
   cleared screen. PLAYGAME's menu between the two is left out, as the Pocket
   Player leaves it out.
2. The level, a tick every 1/18.2065 s (the PC timer's 1193182 / 65536 Hz). A
   slow step runs up to 4 ticks to catch up and drops time beyond that.
3. On a link: the level's exit pages, the next level's intro, the next level.
   After a death: the level's intro again, unless no lives are left.
4. Winning shows the level's exit and the won pages; losing the last life
   shows the lost pages. Then the games list.

## Pages between scenes

A slot names one page, or a `.LST` that names several, one a line with its
pause in seconds (`NAME [pause]`; none waits for a key). Each page is shown as
the Sandbox Edition's `showGraphic` shows it, a port of PLAYGAME's
`ShowGraphic`:

- `.GIF`: the palette goes black, the picture is drawn, and the palette fades
  to the picture's in 65 steps, one a vertical retrace (65/70 s, `FadeTo`);
  then the pause (15 s unless the list gives one). `src/gif.c` is a C port of
  the Sandbox Edition's GIF decoder; a picture of any size is drawn from its top
  left, over what was showing.
- `.BKD` (a backdrop): an x and y offset and a GIF name; the 320 by 200 window
  at that offset into the GIF, faded in the same way (20 s). The GIF is decoded
  through the window, never whole, so a 320 by 3600 sheet costs no more memory.
- `.FLI` (an animation): `src/fli.c` ports the Sandbox Edition's decoder
  (colors, line and run-length deltas, black and copy frames, and the FLC
  chunks). Its frames play over whatever is showing, one every `speed`
  retraces, with no fade; at speed 0 all at once. Then the pause (0 s).
- `.TXT` (text pages): written over the picture showing in PLAYGAME's 8 by 8
  font, 31 characters a line (its `fgets` buffer), 16 lines a page from row 5,
  in the palette entry nearest purple. When more lines follow, `MORE...` in
  red and a pause, then the next page over the same picture. Each page waits
  its pause (90 s, or the list's); the picture is put back after the last. In a
  list the purple is matched in the list's last picture's palette, otherwise in
  the game's colors (the menu picture's palette before the first level, then
  the level's), as PLAYGAME's `colors` hold them. A missing file shows
  PLAYGAME's red "Missing File" lines.

A level fades in from the palette showing to its own, and when it ends (a
link, a death, the win) it fades to black from its last frame; the level's
clock starts after the fade in. Key presses made while pages show wait in a
buffer, as the BIOS kept them for PLAYGAME's `GetKey`: each pause reads one,
so a key pressed during an animation ends its 0 s pause, and one pressed
during a fade ends the picture's pause as soon as the fade is done. The buffer
is emptied after each level and when a text page starts, as PLAYGAME did.
Times are kept from the start of each page, so slow reads or draws do not add
up: a page that ends on time begins the next at that moment.

## Keys

Keys arrive as Linux key codes and reach the game as DOS scan codes: the arrows,
Home, End, Page Up and Page Down (the diagonals) as the number pad's, and the
rest of the keyboard as it is (Space, Enter, letters). Esc always goes back to
the list.

## The screen

The play view is PLAYGAME's 312 by 196 (the CRT controller showed no more while
playing) at twice the size: 624 by 392, centered on the 640 by 480 screen. A
page between levels is 320 by 200 at twice the size, 640 by 400 from row 40.
Colors are the 6-bit VGA palette widened to 8 bits as the Sandbox Edition does
it.

## Timing on the board

`main.c` reads the kernel's clock (`clock_gettime`), runs the session and
sleeps until the next tick, fade step, animation frame or pause end
(`sleep_until`, looking at input at least every 20 ms), so the program does not
spin. Each game's clock starts once its level is read, drawn and faded in. It
logs each page as it begins and the screen's CRC once the page is fully up,
and every 5 s the frames shown and the slowest step, draw and wake-up. Roll the
Dice on a Pi Zero W (kernel generation 15, 2026-11-07), with no key pressed
until the level:

```
 4.368 LOG OS: GMOS play DICE/DICE.GAM
 4.511 LOG OS: GMOS page starter.fli, 38 frames 5/70 s apart, then 0 s
 7.126 LOG OS: GMOS page title.gif, 12 s
 7.192 LOG OS: GMOS page frame crc 2144990842
 8.084 LOG OS: GMOS page frame crc 1276733601
 8.4   LOG OS: GMOS 5 s: 69 frames, tick 0, step max 0 us, draw max 23402 us, late max 0 us
20.053 LOG OS: GMOS page one.gif, 3 s
21.026 LOG OS: GMOS page frame crc 334454467
24.168 LOG OS: GMOS level 1
25.04  LOG OS: GMOS frame crc 4226055335 tick 1
       LOG OS: GMOS 5 s: 92 frames, tick 177, step max 2901 us, draw max 23394 us, late max 4240 us
```

The animation runs 38 x 5 retraces (2.71 s) from the moment the game opened,
so reading its files does not delay the title; each fade takes 65/70 s; the
pauses are on time to a few tens of milliseconds. Drawing the doubled screen
and presenting it takes about 23 ms, so a fade shows about 40 of its 65 steps
and an animation at 5 retraces (71 ms) a frame shows every frame. A tick takes
about 3 ms, so a 55 ms tick has room to spare. The first window of play wakes
later (up to 21 ms) because of the checksums logged then.

Five more Starters, copied to the card with `tools/board_play.py --put` and
played a little the same way (Enter every 4 s on the pages, then right, two
jumps, left, up, down and standing), on 2026-11-07:

| Folder | Starter | What it shows | Pages up (CRC same as host) | Ticks logged, same as host |
| --- | --- | --- | --- | --- |
| `OVERHEAD` | Overhead | a top-down level | 3 of 3 | 254 of 254 |
| `MONSTERS` | Meet the Monsters | monsters | 3 of 3 | 255 of 255 |
| `KEYDOORS` | Keys and Doors | keys and doors | 3 of 3 | 254 of 254 |
| `CARTOONS` | Pictures and Cartoons | the sting, then a list as the intro | 3 of 3 | 255 of 255 |
| `CUTSCENE` | Cutscenes | an intro list ending with a text page | 6 of 6 | 254 of 254 |
| `DICEOLD` | Roll the Dice saved before 3.00 | the converted `.GAM` | none (no slots) | 259 of 259 |

Every 5 s window of play showed 92 frames, a step of at most 2.9 ms, a draw of
at most 24.4 ms and a wake-up at most 4.5 ms late.

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
- `tests/test_pictures.py` checks the pages against the Sandbox Edition
  engine's own code: every frame of the Starter sting (`STARTER.FLI`), the
  Sandbox sting (`sandlogo.fli`, two versions) and a cartoon (`bloom.fli`), two
  MAXIMUS backdrops (windows 600 and 3000 rows into 320 by 3600 sheets), Roll
  the Dice's won and credits text pages (two pages each, with `MORE...`), a
  missing text file, Cutscenes' level intro text, and the pre-3.00 conversion
  of Roll the Dice and of the 14 shareware `.GAM` files on RSD's CD all give the
  hashes in `tests/data/pictures-ref.txt`. A short script in a Sandbox Edition
  worktree computed them with that engine's `formats/fli.ts`, `displayFile` on
  its `Screen`, `drawImage`, `parseGame` and `serializeGame`; only the hashes
  are kept here. It also checks the fade steps against `FadeTo`'s formula, the
  title and level fades on the session clock, the animation's frame times, the
  key buffer, and that Roll the Dice saved before 3.00 plays the same 1200
  ticks as the 3.00 file. Files that are not in `tests/games` are read from a
  Sandbox Edition checkout with its games built, named by `GMOS_SANDBOX`;
  without it those tests are skipped. PNG files go to
  `build/host/pictures-*.png`.
- `tools/board_play.py <kernel checkout> <port> [--game DIR] [--folder PATH]
  [--put] [--put-elf] [--pages]` installs a game folder (Roll the Dice by
  default), picks it in the list, plays it a little and saves the log.
  `--check <log> [--folder PATH]` replays the game keys the board logged (and
  the moments a level let go of them) on the host and compares the view hash of
  each tick; the table above gives the results.

## Next

- Sound: PC speaker effects first (the core already queues them), then Sound
  Blaster clips and music.
- Real input: a USB keyboard or gamepad, or a remote (HDMI-CEC, IR), in place of
  the synthetic device.
- Pre-3.00 levels may use older `.CHR` and `.MON` layouts; only Roll the Dice
  converted to the old `.GAM` layout has been played so far, not the CD's
  shareware games themselves.
- Sound on the Pi Zero W, which has no analog jack: HDMI audio first, since the
  TV is already the screen and the speaker and it needs no extra parts; PWM on
  GPIO 18 and 13 needs a filter and amplifier added to the board.
- Presenting only what changed, to cut the 23 ms draw.
