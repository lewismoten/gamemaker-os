# Packed games (.GMP)

A game can sit on the card as one file, `NAME.GMP`, instead of a folder of DOS
files. It is the Pocket Player's one-file game from Game-Maker: Sandbox
Edition (`web/src/export/pocketFile.ts`), so the same file plays on both
players.

## Why

Every file on a FAT32 card takes at least one cluster, and the Sandbox
Edition's 135 public games are about 9,100 small files. Packed, each game's DOS
files are deflated into one file:

| What is on the card (135 games, 2026-11-07) | Size |
| --- | --- |
| Loose folders, 2 KiB clusters | 129.6 MB |
| Loose folders, 4 KiB clusters | 141.2 MB |
| One `.GMP` each, DOS files only | 34.7 MB |
| One `.GMP` each, DOS files and the Pocket Player's own data | 58.3 MB |

The raw DOS files are 119.2 MB; deflate takes them to 34.1 MB. Cluster
rounding matters much less than compression at these cluster sizes.

## The format

A `.GMP` is a `GMS1` bundle: `"GMS1"`, an entry count, then 40-byte entries
sorted by name (`name[32]`, offset, length; little endian), then the data. The
Pocket Player's own entries are `game.txt` (`GMP 1`, then `id|title|levels`),
`<id>.cfg`, `cards.pak`, its packed level files, `library.json` (the game's
`gmp-library` record) and `thumb.pth`. Game-Maker OS adds nothing to the
format: it reads the game's DOS files, which the writer stores as entries
named `dos/<NAME>` (the upper-case 8.3 name), each its length (4 bytes, little
endian) and then the raw deflate. The Pocket Player finds entries by name and
never asks for those, so it skips them.

A game the Pocket Player cannot play is written without `game.txt` (only
`library.json` and the `dos/` entries). The Pocket Player lists only packs
with a `game.txt`, so it leaves those out; Game-Maker OS lists them.

## The reader

- `src/gmp.h`, `src/gmp.cpp`: the index is read by the Pocket Player's own
  code, `third_party/gmcore/gmsmall.h` (vendored unchanged with the play core by
  `scripts/vendor_gmcore.sh`), so both devices parse the same bytes. A file is
  inflated by puff (`third_party/puff`, from zlib's `contrib`). `gmp_read` is a
  `gp_read_fn`, so the play core, the session and the title-screen picker read
  a pack exactly as they read a folder. Limits: 4,096 entries, 1 MiB a file
  (the largest DOS file in the Sandbox games is 658 KB), and an entry must
  inflate to exactly its stated length.
- `src/sdpack.c`: on the board, the packs in `/sd/GAMES` and `/sd/POCKET`. A
  pack's index is read into memory once when it is opened; its data is read
  forward through the kernel's file API, which has no seek yet (a read behind
  the last one opens the file again).
- `src/main.c`: packs are listed after the folders. A pack holding the same
  `.GAM` (by CRC-32) as a game already listed is skipped, so a card can carry
  both while it changes over. Each pack's `library.json` gives its title and
  tab; `LIBRARY.JSL` then adds the company, year and blurb.

## Volumes (for floppies, later)

The reader takes a pack as a set of volumes: `NAME.GMP`, then `NAME.G01`,
`NAME.G02` ... (up to 15 further volumes), read as one file in that order. The
bundle's offsets count through the whole set, so an entry may start in one
volume and end in the next. The board finds the further volumes in the same
folder listing (the listing gives each file's size) and stops at the first
missing number. Today every pack is one volume; nothing in the format changes
for a split pack: a splitter cuts the bytes of one `.GMP` at the floppy's size.
The index has to fit in `NAME.GMP`. What a splitter should add is in the
Sandbox Edition's format notes (a volume count and the whole set's size, so a
stray `NAME.G01` from another game is caught); the Pocket Player would need the
same volume layer under its `SmallPak::readAt`.

## One card for both players

Both players read a folder named `POCKET` at the top of the card (FAT names are
case-blind, so the Pocket Player's `/pocket` is the same folder):

```
POCKET/
  DICE.GMP        one game: the Pocket Player's data and its DOS files
  BLASTER.GMP
  ...
GAMES/
  LIBRARY.JSL     optional, Game-Maker OS only: company, year and blurb
```

Game-Maker OS also lists `.GMP` files in `/GAMES` beside game folders.

Make one with the Sandbox Edition:

```
cd web
npx vite-node scripts/pocketData.ts --gmp <card> --gmos       # <card>/pocket/*.GMP, every public game
npx vite-node scripts/gmosCard.ts --out <card> --index-only   # <card>/GAMES/LIBRARY.JSL
```

`--gmos` writes every public game, Starters included; a game the Pocket
Player's packer leaves out gets a pack only Game-Maker OS lists. Without
`--gmos`, `--gmp` writes the Pocket Player's games, still with their DOS files
(`--no-dos` leaves them out).

Over USB, the kernel's `PUTF` installs `GAMES/<NAME>.GMP`, `POCKET/<NAME>.GMP`
(and `.G01` volumes) and `GAMES/LIBRARY.JSL`, up to 1 MiB a file:

```
usb_kernel.py <port> put DICE.GMP --name POCKET/DICE.GMP
```

## Tests

`tests/test_gmp.py` builds a pack of `tests/games/DICE` the way the writer
does and checks that every file reads back byte for byte, that it plays tick
for tick (the Pocket Player host test's state lines) and card for card like the
folder, that a pack split into volumes reads the same, and that broken packs
(cut short, wrong counts, entries past the end, wrong lengths, bad deflate) are
refused. `make test-asan` also fuzzes `tests/data/dice.gmp`: mutated bytes and
index fields, random volume splits, reads of random sizes and the title picker.
With `GMOS_GMP=<folder of .GMP files>` and `GMOS_GAMES=<the Sandbox Edition's
games folder>`, it checks real packs from `pocketData.ts` against the loose
games, byte for byte.
