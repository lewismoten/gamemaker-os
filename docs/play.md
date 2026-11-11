# Playing a game

Enter opens details; Play opens the game from its DOS files in a loose folder,
GMP or supported ZIP/SFX archive. See [copying games](drag-drop.md) for limits.
A loose game needs no conversion or packing first.

## The DOS menu

Opening a game shows its title graphic or slide list, then PLAYGAME's main menu.
Up/Down wraps through Play, Read Instructions, Read Storyline, See Credits,
See Highest Scores and Quit; Enter selects. Esc at the menu or Quit returns to
the GMOS library. Pages use the existing GIF/BKD, FLI, text and `.LST` reader;
keys advance pauses and text pages, and Esc returns from a page to the menu.
Missing instructions/story/credits show PLAYGAME's message for ten seconds or
a key. Play starts a fresh game, with its first level's intro. F5/F6 during play
keep their existing save/load behavior.

A win, the last life lost, or Esc during play checks the Top Ten. A qualifying
score opens keyboard name entry (22 characters, Backspace erases, Enter accepts),
then the score table, then the game's menu. Scores stay in the existing
`/OS/SAVES` location; see [high scores](saves.md). The e-ink display continues to
show the active game's title until returning to the library.

`src/session.c` implements this as `gs_launch`; the direct-play `gs_start` API
and demo preview remain available to existing callers, including JS apps.
`src/dosmenu.c` reads the menu words, RGB targets/color selectors, title/row
positions and marker geometry from a recognized XFERPLAY 3.00 build, looking
for `<GAM stem>.EXE`, `XFERPLAY.EXE`, then `PLAYGAME.EXE`. Other executable builds
use stock values. Executables are read as data, never executed.

A game can also supply `PLAYER.JSON` (or `player.json`) with the web player's
record format, for example:

```json
{"base":"xferplay-3.00","patches":{"str.menu.play":"GO","layout.menu.play.x":72,"color.rgb.blue":[10,20,63]}}
```

This overrides the executable's menu values. Put it beside the `.GAM`, in the
same ZIP game folder, or in a GMP's compressed `dos/PLAYER.JSON` entry. GMOS can
only read customization data included in the game; existing packs without it
use the stock menu. Gameplay tuning and non-menu player patches are outside
this reader's scope.

Host coverage is in `tests/test_dosmenu.py` and `tests/host/dosmenu_end.cpp`:
keys/screens, multi-page text and slides, missing pages, patched words/positions/
colors, malformed records, win/loss through name entry and Top Ten, and restart.
The complete launcher still needs a TV/USB-keyboard check: intro/menu pictures
and colors, arrow repeat, all six choices, qualifying score/name persistence,
F5/F6 after restarting, and e-ink transitions back to the library.

## One play core, shared

The rules of play (the hero, keys, blocks, scrolling, monsters, links between
levels) are `third_party/gmcore/gmcore.h` and `gmmon.h`, the play core of the
Pocket Player in [Game-Maker Pocket](https://github.com/lewismoten/gamemaker-pocket)
(`src/`, commit in `third_party/gmcore/PINNED`). That core is a port of the Sandbox Edition's web
engine, which follows RSD's `PLAYGAME.C`. It is header-only and holds no board
code, so the same files build the ESP32 firmware there and `GMOS.ELF` here.

`third_party/gmcore` is a copy pinned by commit (`PINNED`), never edited here.
`scripts/vendor_gmcore.sh <gamemaker-pocket checkout> [commit]` copies
`gmcore.h`, `gmmon.h` and `gmsmall.h` and records the commit, retaining the local
license notices. A core fix goes into gamemaker-pocket, then comes here with
the script.

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

`src/session.c` runs the DOS launcher around the shared play core:

1. The title (scene 0's title graphic), then the game's main menu. Choosing
   Play shows the first level's intro on a cleared screen.
2. The level, a tick every 1/18.2065 s (the PC timer's 1193182 / 65536 Hz). A
   slow step runs up to 4 ticks to catch up and drops time beyond that.
3. On a link: the level's exit pages, the next level's intro, the next level.
   After a death: the level's intro again, unless no lives are left.
4. Winning shows the level's exit and the won pages; losing the last life
   shows the lost pages. Then the Top Ten flow (name entry if the score places), and the game’s menu.

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
rest of the keyboard as it is (Space, Enter, letters). Esc leaves play for
the game’s menu (after the Top Ten). During a level, F5 opens Save Game
and F6 opens Load Game; see [controls and limits](game-save.md). These native
save/load screens and real-card writes are **hardware-untested**.

## The screen

A game plays in 640x480 at 32 bits a pixel, the mode every program starts
in: GMOS switches to it from the games page's 960x540x16 when the game starts
(about 4 ms) and back when it ends. On a 1080p monitor the GPU scales it 2.25
times, with black at the sides.
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

## Sound events

This branch links local audio into `GMOS.ELF` (see below). A game's sounds also
go out on the USB log as
`SND` lines (`src/sound.h`, from the session in `src/session.c`), for a page on
the computer at the other end to play from its own copy of the game's files
(the Sandbox Edition's Pi remote, `#/pi-remote`, with its Pi sound switch on):

```text
SND game DICE DICE.GAM            a game starts: its /sd/GAMES folder and .GAM
SND music start DICE/DICE.CMF     PlaySong: the title's song, each level's, won or lost
SND music stop                    a slot with no song
SND voc DICE/JUMP.VOC             an effect with a Sound Blaster clip in its .SND
SND beep DICE/DICE.SND 9          an effect on the PC speaker: tune 9 of that .SND
SND stopall                       the game ends (or Esc) ...
SND end                           ... and the list comes back
```

Effects are taken from the core's queue after every tick: one line a tick at
most (the last effect started, the one heard), and the same effect again within
3 ticks is left out. A log write never waits for the USB side. Each line is
under 80 characters. `tests/test_sound.py` checks them.

## Sound on the board (ABI 1.8)

Local HDMI playback remains **hardware-untested**. Missing, busy, incompatible
or failed devices fall back to silent play, preserving remote `SND` events.
With ABI major 1, minor >= 8 and a usable `/dev/audio`, GMOS opens it once and plays through HDMI, 16-bit stereo at 48 kHz.
The synth runs in the main loop, never blocking the 18.2 Hz tick or the heartbeat: `au_pump` (`src/audio.c`) asks the
ring's level and writes only when under 250 ms is queued (the heartbeat pumps too, so long reads inside a step keep
feeding). Queue targets are clamped to the reported capacity: kernel develop currently has 16,384 frames,
while 4,096-frame rings also work with less protection against slow storage. Game play uses a shorter
120 ms queue for effects, topped up every pass and between sleep slices of at most 10 ms.
The pump rechecks the ring after each write to account for synthesis-time drainage, and batches
up to 40 ms of PCM without changing synthesis or PLAYGAME's sound events. This trades up to
120 ms of output latency for headroom; older 4,096-frame rings cap it at about 85 ms.
See [audio underruns](audio-underrun.md) for the diagnosis and host regression.
HDMI is used when available; otherwise the kernel's null backend drains silently.

- `src/opl.c`, `src/cmf.c`: an integer OPL2 and CMF player, a port of the web engine's `audio/opl.ts` and `audio/cmfPlayer.ts`
  (tables from `tools/make_opl_tables.py`, no floats in the sample loop). `tools/compare_opl.py` renders songs with both and
  prints the difference (1 to 6% RMS on most songs, envelopes within 2%).
- `src/pcspeaker.c`: the PC-speaker effects (36 steps of 1/145.67 s, a square wave at 12% x 0.8).
- `src/vocplay.c`: streamed VOC clips mixed with the speaker and FM voices; unsupported clips are refused safely.
- Every song change fades the old one out (0.68 s), resets the chip (all channels keyed off) and fades the new one in.
- A game's own sound, or a preview's (its menu song and the game's effects, as PLAYGAME's demo plays them), replaces the
  library's background tune when enabled. Library music defaults off at every boot; the owner can toggle
  it with **M** in the library (the footer shows the setting). Game/demo sound follows the DOS game regardless
  of that setting. The background cycle (`bg_tick` in `src/main.c`): the OS's own tune (`tools/make_ostune.ts`,
  composed in code) every third pick, otherwise a random library game's first-level song (its menu song if none), a
  short gap between tunes. Songs over 8 KB are skipped for the background.
- The log has `GMOS audio ...` lines with the kernel's counters, the synth's CPU in per mille, and each underrun with
  what the loop was doing.

## Next

- Real input: a USB keyboard or gamepad, or a remote (HDMI-CEC, IR), in place of
  the synthetic device.
- Pre-3.00 levels may use older `.CHR` and `.MON` layouts; only Roll the Dice
  converted to the old `.GAM` layout has been played so far, not the CD's
  shareware games themselves.
- Additional audio backends: PWM on GPIO 18 and 13 would need a filter and amplifier
  added to the board; HDMI is already supplied by kernel develop.
- Presenting only what changed, to cut the 23 ms draw.
