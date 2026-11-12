# Demo preview

Leave a game on its details view for 3 seconds and its demo plays where the title screen was.
Any key press stops it (and then does what it always does: Enter plays the game, Esc goes back).
When the demo ends, the title screen returns and the 3 seconds start again. A game with no demo
shows nothing extra. Options and folder-name entry suppress idle previews.
On ABI 1.11+, active demos hold the TV awake and prevent clock slowdown, even
without an input device; stopping releases the hold. See [power.md](power.md).

## The recording

The demo is the `.REC` named in scene 0's file slot 11 of the game's `.GAM`, the file PLAYGAME's
`Rec->InitPlayback(Game.File(0,11))` plays. A record is 3 bytes: a 16-bit `DeltaTime` (game ticks
since the previous key event) and a scan code (bit 7 set: released). At most 1000 records.

`src/session.c` `gs_demo_start` opens the game, reads the `.REC` into the session's text-page
buffer (3000 bytes at most; nothing new is allocated for the recording) and begins level one.
`demo_feed` is PLAYGAME's `Recording::DoPlayBack`, run once before each game tick: the timer counts
up, and every record whose time has come presses its key (`gp_key`). So a record at time T is
pressed before tick T, tick for tick as in the web engine (`tests/test_demo.py` compares the view's
hash after every tick with the play core fed the same keys by hand).

- The demo ends at its Esc (the recording's last key), a win or the loss of the last life.
- A link to another level or a lost life continues at once, keys let go.
- A gap over 4096 ticks is a DOS demo's wait at a page the preview does not show; it counts as one tick.
- On this branch, `GMOS.ELF` plays the menu CMF song and demo effects through
  optional ABI 1.8 `/dev/audio`, independently of the library **M** toggle
  (library music defaults on). Missing or failed audio falls back to silence;
  HDMI playback is **hardware-untested**. The first level's pages (title, intro) are skipped.

## The screen

`gm_draw_picture` repaints only the picture (640 x 400 at 36, 68) each tick and `main.c` presents
just that rectangle, so the Pi does not redraw the page 18 times a second. The log has
`GMOS demo start <DIR>, <n> keys`, `GMOS demo t <tick> h <view hash>` (tick 1, then every 91st)
and `GMOS demo stopped: a key` or `the demo ended`.
