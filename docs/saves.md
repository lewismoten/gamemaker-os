# Saves and high scores on the Pi

Status: first slice (design, file layer, high scores), now linked into `GMOS.ELF`. The game-state save
(`GAME0.SAV`) and demos (`.REC`) are later slices. Nothing has been written to a real card yet.

## Decisions

- **DOS's own files.** `<GAME>.HIS` is DOS PLAYGAME's high-score file byte for byte, so a
  card's folder can be copied to a DOS game folder and back. No headers, JSON or checksums
  inside a DOS file.
- **One place to write.** The kernel lets an app write only to `/sd/OS/SAVES/<GAME>/`
  (sandbox-kernel `docs/saves-storage.md`). `<GAME>` is the game's 8.3 stem, so two
  games never share files. The game's own folder and ZIP/GMP sources are never modified.
- **Power cut safe.** `save_put` writes `NAME.TMP`, syncs, closes, then renames over the real
  name; the kernel's rename is copy-on-write. After a cut the real file is the old one or the
  new one. A leftover `.TMP` is never read; the kernel gives apps no delete, so the next save of that file overwrites and renames it away.
- **Not transparent DOS discovery.** DOS looks for `.HIS` next to the game, not in `OS/SAVES`.
  Moving a card between the Pi and DOSBox needs an explicit copy of the game's folder.

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
  that doesn't place goes straight to the list, as in DOS.
- **From the list**: `H` (high scores) on the grid or the details view shows the selected game's Top Ten
  alone; the key-help line says so.
- **Where it lives**: `hisload.c` reads `/OS/SAVES/<GAME>/<GAME>.HIS`; with none yet it seeds from the
  game's own `<GAME>.HIS` (read only), so DOS scores carry over. A new name is written back to
  `/OS/SAVES` only. On a kernel older than ABI 1.7 the entry is skipped (it could not be saved).
- The game's own high score picture (`File(0,8)`) is the list's backdrop, as in DOS.
- Tests: `tests/test_hiscreen.py` (flow; renders `build/host/top-ten/*.png`).

## Still to do (later slices)

1. `GAME0.SAV` to `GAME9.SAV` and `SAVNAMES.DAT`: layout is 37755 + 5M + 3R bytes (max 41505) on
   the original 3.00 binary; needs a codec and a bridge into the portable core (the core drops
   monster status fields and keeps no recorder). Needs Kaylee's design review first.
2. Re-vendoring the SDK once the kernel with `/OS/SAVES` is released (`sdk/PINNED` names 5732b42).
3. Real-card checks: power cut during a save, card full, long run.
4. Demos (`.REC`), later.
