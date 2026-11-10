# Saves and high scores on the Pi

Status: file layer, high scores and F5/F6 save/load are linked into `GMOS.ELF`.
The [save/load guide](game-save.md) covers controls, the DOS wire codec and live
state bridge. Host checks cover the codec and state round trip; real-card writes
and runtime DOS interchange remain **hardware-untested**.
[Demo preview playback](demo.md) is also implemented; recording new demos is not.

## Decisions

- **DOS's own files.** `<GAME>.HIS` is DOS PLAYGAME's high-score file byte for byte, so a
  card's folder can be copied to a DOS game folder and back. No headers, JSON or checksums
  inside a DOS file.
- **One place to write.** The kernel lets an app write only to `/sd/OS/SAVES/<GAME>/`
  (sandbox-kernel `docs/saves-storage.md`), besides the separate cache scope.
  `<GAME>` is a key made from up to four stem letters and four hex digits of a
  hash of the source path and `.GAM` name. This separates same-named games in
  different folders, but a 16-bit hash can collide and moving a source changes
  the key. The `.GAM` stem must be 1–8 letters, digits, `_` or `-`; long or
  unsupported stems have no Top Ten or F5/F6 screen. Game folders and ZIP/GMP
  sources are never modified.
- **Staged file replacement (hardware-untested).** `save_put` writes `NAME.TMP`, syncs, closes, then renames over the real
  name; the kernel's rename is copy-on-write. The intended contract keeps the old or new real file after a cut; host fake-kernel
  checks do not establish physical-card durability. A leftover `.TMP` is never read; the kernel gives apps no delete, so the next save of that file overwrites and renames it away.
- **Not transparent DOS discovery.** DOS looks for `.HIS` next to the game, not in `OS/SAVES`.
  Moving scores between the Pi and DOSBox needs an explicit copy of the `.HIS` file
  between the save folder and DOS game folder. Live `.SAV` interchange still needs
  runtime acceptance.

## `.HIS` (from `code/GM/HISCOREL.C`)

Ten records of a signed 32-bit little-endian score and 30 name bytes: 340 bytes, no header. DOS
reads whatever exists (a short or missing file gives zero rows). A score takes the first row it
strictly beats, so a tie sits below the equal score; lower rows shift down and the last drops;
a score of 0 or less never places. The name prompt takes 22 characters and DOS clears only the first byte of the row before it, so the old name's tail bytes stay after the new name's terminator; `his_insert` does the same. DOS also
rewrites the file when nothing placed; the Pi doesn't, to spare the card.

## Code

| File | Job |
|---|---|
| `src/hiscore.c/.h` | Parse, serialize, place and insert; pure |
| `src/savefile.c/.h` | `save_put`, `save_get`, `save_his_load`, `save_his_record` over a `save_ops` table |
| `src/savefile_sbx.c` | `save_ops` over the kernel's open/write/fsync/close/rename |
| `tests/host/savefile.c`, `tests/test_savefile.py` | A fake kernel that cuts power after every step |

## The Top Ten on the Pi

- **At game end** (a win, a loss or Esc, as in PLAYGAME, once the first level began): a score above 0 that
  beats a row gets `hiscreen.c`'s name entry ("Congratulations!", "You scored number N on the Top
  Ten!", "Enter Your Name:", 22 characters, Enter or Esc accepts, Backspace or Left erases, a blinking
  box cursor), then the Top Ten with "Your Latest Score" and "Hit any key" (30 s or any key). A score
  that doesn't place still shows the Top Ten. Both paths return to the game's DOS menu.
- **From the game menu**: See Highest Scores shows the table without name entry, then returns to the menu.
- **From the list**: `H` (high scores) on the grid or the details view shows the selected game's Top Ten
  alone; the key-help line says so.
- **Where it lives**: `hisload.c` reads `/OS/SAVES/<KEY>/<STEM>.HIS`; with none yet it seeds from the
  older `/OS/SAVES/<STEM>/<STEM>.HIS`, then the game's own `<STEM>.HIS` (read only), so DOS scores carry over. A new name is written back to
  `/OS/SAVES` only. On a kernel older than ABI 1.7 the entry is skipped (it could not be saved).
- The game's own high score picture (`File(0,8)`) is the list's backdrop, as in DOS.
- Tests: `tests/test_hiscreen.py` (flow; renders `build/host/top-ten/*.png`).

## Still to check

- Real-card power cuts, card-full errors and long runs (**hardware-untested**).
- DOS↔Pi runtime save interchange; byte layout and host round trips do not prove it.
- Save/load menu behavior on the board, including failed reads and writes.
- New demo recording; existing `.REC` preview playback is separate.

The SDK already includes save storage (ABI 1.7) and is pinned at ABI 1.10;
no SDK refresh is needed to expose the current calls.
