# Save/load and the DOS `GAME0.SAV` wire codec

Status: F5/F6 save/load is linked into `GMOS.ELF`, using `src/saveplay.c`,
`gp_state_save` / `gp_state_load` in `src/game.cpp`, and the byte codec in
`src/savefile.c/.h`. The codec and state round trip have host checks. The menu,
real-card writes and DOS↔Pi runtime interchange remain **hardware-untested**.

## During play

During a level, Back/Esc opens the pause menu: arrows and OK choose Save game
or Load game. F5/F6 still open those screens directly. Up/Down selects one of
ten slots; OK/Enter selects it. Save names use the shared [on-screen keyboard](library.md):
arrows and OK edit up to 20 characters; Done writes. Physical typing hides it;
Backspace or Left erases, Enter writes, and Up/Down restores the keyboard.
Back/Esc cancels. A result message lasts two seconds or until a new key press.
The game's own Save/Load picture is used when present.

Slots are `GAME0.SAV` through `GAME9.SAV`; `SAVNAMES.DAT` holds ten 21-byte
name records. Both use `/sd/OS/SAVES/<KEY>/`, like [high scores](saves.md).
The key depends on source path and `.GAM` name; moving a game changes it.
A valid 1–8 character DOS stem is required for these screens. Kernel ABI 1.7+
is needed to write; the menu itself is not gated by that minor version.
This command belongs to native GMOS play, not the JS `<game>` element.

Names are written before the slot, in separate staged transactions. A slot
write can fail after its name changes; the pair is not one atomic save.
If the names file cannot be read (other than being absent), it is left untouched
for that menu session; a successful slot save with a failed name read or write
shows "Game saved; name not saved!".
Missing or refused saves show a message. The loader backs up current play
before replacement and attempts rollback if loading a level fails; failure of
both load and rollback ends the game. No checksum or game identifier exists
in the DOS file, so not every damaged or wrong-game file can be detected.

The live bridge maps portable-core state to DOS records and validates the
fields it uses on load. Invalid monster records keep the fresh level's record.
Fields absent from the core, including DOS screen pointers and the recorder,
are zero on save; the codec below preserves opaque bytes independently.
This is not full DOS runtime equivalence or demo recording support.
The DOS `.SAV` format has no camera pixel-offset field, so loading during a block
scroll redraws from the saved map origin and may visibly shift the view.

## Reference and compatibility boundary

The format source is the public GameMaker-SandboxEdition repository, revision
`7f614cfbfa5c8c15c83d586a46edf57af79d1afc`:

- `code/GM/PLAYGAME.C:1773–1792,1874–1890`: the individual `fread`/`fwrite` calls.
- `PLAYGAME.C:113–172,207–265`: `minfo`, `cinfo`, block timers and recorder.
- `code/GM/CHAR.H:18–51`: character/sequence/frame records.
- `code/GM/MAP.H:8–30`, `BLOC.H:6`, `GMGEN.H:29`, `code/DRV/GEN.H`:
  array capacities, filename capacity, enum and integer types.
- `code/GM/MAKERS.MAK`: Borland large-model build (`-ml`); four-byte far pointers.

Source declarations alone do not prove Borland packing. The public shipped
`cd/gm/playgame.exe` (163130 bytes, SHA-256
`49ed1c7898b05cd19813e1731f7694f52ee17d768f01e38991666fac86365d4b`)
independently corroborates the sizes in the table below. Its MZ header is 8192
bytes. Load-image offsets `0xc322–0xc4dd` contain the save writes, with record
sizes 1,2,2,2,4,4,1,135,3784,2,2,55,10,2,1,5,70,3 and array counts
150,150,200,200,10000,`M`,`R` as applicable. `0xb9a8–0xbb6e` corroborates
reads and the restored recorder far pointer at object offset 55. These are static
binary checks, **not** a successful DOS runtime session.

The public web save fixtures at this revision start with JSON (`{"version":1`),
not these DOS records. They are not a DOS oracle and are neither imported nor
accepted as interchangeable files. No historical/private save file is committed.
The committed Python oracle creates original synthetic structural fixtures from
public layout facts; its opaque patterned records are not playable game sessions.

## Layout

There is no magic, version, checksum, padding between writes, or extra header.
All scalar words are little endian: DOS `int`/`uint`/enum are 2 bytes,
`long`/`ulongi`/far-pointer tokens are 4 bytes. Byte arrays retain every byte.
`M` is the first byte, the number of block-spawn timers (0–150; one per background
block in `PLAYGAME.C:4221–4236`). `R` is the signed recorder index (0–1000).
Total length is **37755 + 5M + 3R**, from 37755 to 41505 bytes.

| Offset | Bytes | DOS field |
| --- | --- | --- |
| 0 | 1 | `MaxBlkMon` (`M`) |
| 1, 3, 5 | 2 each | `prevscene`, `doscene`, `linkin` |
| 7 | 4 | saved sound clock |
| 11 | 150 × 4 | `nxtchg` |
| 611 | 150 | `blkmap` |
| 761 | 135 | `cinfo` object image |
| 896 | 3784 | `chrstruct` object image |
| 4680, 4682 | 2 each | `mx`, `my` |
| 4684 | 200 × 55 | `minfo` object images |
| 15684 | 200 × 10 | `monmapstruct` object images |
| 17684 | 100 × 100 × 2 | map `[y][x]`, block then monster-slot byte |
| 37684 | 1 | `NoMons` |
| 37685 | M × 5 | block byte then next-spawn `ulongi` |
| 37685 + 5M | 70 | `Recording` object image (below) |
| 37755 + 5M | R × 3 | 16-bit delta time then key byte |

Recorder offsets relative to `37685 + 5M`:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 2 | signed `RecIndex` (`R`) |
| 2 | 2 | signed `LastRecordedKey` |
| 4 | 51 | `RecFile` (raw bytes; no NUL required by this codec) |
| 55 | 4 | DOS far-pointer token `Rec` |
| 59 | 4 | `RecordTimer` |
| 63 | 1 | `RecFlag` |
| 64 | 4 | `LastKeyPress` |
| 68 | 2 | enum `Paused` |

## API and validation

`save_game_decode` returns an owned caller-provided `struct save_game`.
`save_game_size` returns the exact encoded size (or zero for invalid counts).
`save_game_encode` writes exactly that size and updates `*n` only on success.
There are no heap allocations or I/O in the codec, and no dependency on native
struct layout, native pointer width, wire alignment or host endian ordering.

The fixed DOS object images (`character_info`, `character`, `monster_info`,
`monster_map`, `map`) are deliberately exposed as raw wire bytes; scalar headers,
clocks, timer records and recorder metadata/records are decoded to fixed-width
integers. All raw fields, stale pointer tokens, reserved bytes, filename tails and
noncanonical booleans survive decode/encode unchanged. No DOS token becomes a
host address. Native `struct save_game` padding and unused variable-array capacity
never enter the wire. Decode zeros its entire destination only **after** framing
validation succeeds, including unused array entries.

Reject NULL required arguments, overlapping object/wire/output-size storage,
block counts over 150, negative/over-1000 recorder counts, all short fixed or
variable records, and any extra trailing byte with `SAVE_EINVAL`. Output capacity
below the exact length returns `SAVE_EFBIG`. Every failure leaves destination and
output-length storage unchanged. Wire buffers may be unaligned; object and
`uint32_t *n` must have ordinary native C alignment. Object/wire ranges use the
entire supplied input length/output capacity when checking overlap.

This is structural validation, **not** semantic permission to resume a game.
No game identifier exists in the file. Scene links, map cells, inventory indices,
monster fields, filename text and gameplay values are not trusted/normalized by
this layer. DOS's unchecked short-read/ignored-trailer behavior is intentionally
not reproduced. Other DOS versions, recorder-disabled builds and differently
packed/recompiled executables are unsupported.

The state object is roughly 43 KiB. Allocate it in owned bounded storage, not a
small board stack. The live bridge uses the session's bounded storage and
restores fields understood by the portable core, including post-load drawing
and key resets. Runtime DOS↔Pi, real-card and power-loss acceptance are still
open. Byte preservation by this codec alone does not establish safe gameplay
restoration.

## Host verification

`tests/test_state_save.py` checks live-state round trips, truncated wire files
and sampled mutations. It allows damaged fields that the bridge can accept;
it does not prove that every corruption is refused or exercise the F5/F6 menu.
The checks below cover the separate wire codec and save-file layer.

```sh
python3 -m unittest discover -s tests -p test_game_save.py -v
AUDIO_SANITIZE=1 python3 -m unittest discover -s tests -p test_game_save.py -v
python3 -m unittest discover -s tests -p test_savefile.py -v
make host
GMOS_HOST=build/host python3 -m unittest discover -s tests -v
# Keep the existing venv interpreter if the ASan helper resolves symlinks and
# thereby drops installed venv-only dependencies (no package install needed):
make test-asan FUZZ_ROUNDS=100 ASANPY="$(command -v python3)"
```

The independent Python `struct.pack` oracle follows DOS's write order, not C
struct layout or codec offsets. The C harness checks every decoded field/opaque
byte against independent values, then compares every re-encoded byte with that
oracle. The matrix has 1151 distinct `(M,R)` pairs covering every allowed value
of each count, minimum, maximum, signed-word extremes and high-bit clocks. The
largest fixture tests every one of its 41505 insufficient input lengths and 41505
insufficient output capacities using exact-sized allocations, malformed counts,
trailers, error atomicity, overlap rejection, NULL arguments, unaligned wire and
sentinels. Sanitizers cover the real codec. This is not the full Cartesian count
matrix and does not assert that patterned opaque records are semantically valid.

Copyright (c) 2026 Lewis Moten. Codec, tests and this document: Hermes Agent,
OpenAI Codex GPT-6.1 Sol. Format follows RSD's MIT-released source (copyright
(c) 2014 gandrewstone; (c) 2026 Lewis Moten modifications); see NOTICE.md.
