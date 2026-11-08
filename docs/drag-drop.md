# Copying games onto a FAT card

This is a **host-qualified candidate**, paired with sandbox-kernel ABI 1.5
(local signed commit `3010a7c4c66f76bbef61fe21ccf0545cfe1ea39d`, `sdk/PINNED`).
New desktop-card/Pi acceptance is not claimed. The Mac owns integration and
physical-card testing; neither repository was pushed or merged for this work.

## Copy, eject, play

1. Use the kernel's existing **FAT32** card/partition, with its boot files and
   installed `OS/GMOS.ELF`. This change does not add exFAT or FAT16 support.
2. On the computer, open `GAMES` on that card. Copy a game folder with its raw
   assets, a supported `.ZIP`, or a plain-ZIP self-extracting `.EXE` into it.
   Long names and spaces do not need manual 8.3 renaming. Keep the asset names
   that the game's `.GAM` references; arbitrarily renaming assets still breaks
   those references.
3. A collection folder is allowed: `GAMES/My Collection/My Game/MAIN.GAM`.
   Loose games are found at folder depths 1–2. ZIP/EXE files are considered
   directly in `GAMES` or in one collection folder. A depth-2 game is also
   found when its parent already contains a game; deeper discovery is bounded
   away, while assets themselves may be nested within the kernel path limits.
4. Eject/unmount the card safely, power down the Pi, insert the card and boot.
   This is not a hot-swap workflow. Select the game, open its details and Play.
   Esc leaves the game for the list. Hardware input remains the existing kernel
   input mechanism; this does not add USB gamepad drivers.

Existing GMPs still work in `GAMES` and `POCKET`, including their established
`NAME.GMP`, `NAME.G01`… volume convention. A renamed long single-file GMP can
be opened via its card-generated alias without the user renaming it. Do not
independently rename the files of a multi-volume set: their shared volume base
must remain consistent. ZIPs and folders do not require conversion to GMP.
USB `put`/`putgame` still has its separate 8.3 installation contract.

## ZIP layouts and limits

Supported ZIP32 methods are **stored (0)** and **raw deflate (8)**, inflated by
vendored puff. A `.GAM` and its assets may be at the ZIP root, or inside one
wrapper folder. Nested assets under that game root are supported. The reader
is the same `gp_read_fn` interface used by loose files and GMPs. It reads the
archive in place: no extraction, no executing an EXE stub, no new game-data
writes. SFX offsets may be absolute or relative to the appended ZIP. Central
sizes support ordinary data-descriptor archives as well.

There is one selected game root per ZIP. Multiple `.GAM` files in that same
root keep the first central-directory `.GAM`, matching loose first-GAM
selection. Distinct game roots, deeper wrapper layouts, nested archives,
ZIP64, encryption, multi-disk ZIPs, symlinks and other compression methods
(including implode, bzip2 and LZMA) are rejected. A valid-looking entry whose
payload has a bad CRC, broken deflate or wrong exact length fails when read;
an invalid main `.GAM` leaves that archive out of the list. Malformed archive
rejection uses a bounded scan diagnostic rather than crashing the menu.

Bounds: 256 displayed games and a shared 256-entry deferred nested-folder/archive
candidate queue (excess candidates are diagnosed and skipped); 64 MiB archive,
1,024 central entries, 255-byte internal paths,
1 MiB per uncompressed file, 1 MiB + 64 KiB per compressed entry. The current
`.GAM` discovery/play-core buffer remains 64 KiB. The kernel still limits the
**entire syscall path including `/sd` to 255 UTF-8 bytes** and asset depth to
16. A joined path that is too long is skipped/rejected, never truncated into a
different file. VFAT names unrepresentable within the kernel's bounds fall
back to aliases; GMOS does not promise arbitrary-length desktop paths.

Directory/file spelling is preserved for actual reads. Kernel long-name
lookup supplies Unicode simple case folding (no Unicode normalization or
full expansions); ZIP-internal matching folds ASCII, as legacy DOS asset
names require. Menu `dir`/`gam` labels retain their legacy 8/12-byte display
fields; **these labels are not launch paths**. Exact long paths and main-GAM
names are kept separately. Titles still come from the `.GAM`/library index.

Absolute paths, drive/colon paths, NUL/control bytes, empty path components,
`.` and `..` are refused. Backslashes in archive asset paths are normalized to
slashes. Case-insensitive duplicate archive paths are rejected, not aliased by
a hash collision. Local headers/names/methods/flags, bounded ranges and CRCs
are checked before returning asset bytes. A failed ZIP read does not publish
partial bytes into its caller's buffer.

## Desktop noise, duplicates and the index

Ignore `._*`, `.DS_Store`, `.Trashes`, `.Spotlight-V100`, `.fseventsd`,
`System Volume Information`, `Thumbs.db`, `desktop.ini` and `__MACOSX` in game
scanning/reads. GMP enumeration also checks the actual long name, so an
AppleDouble file cannot be mistaken for a GMP by its innocent short alias.

Equal main-`.GAM` CRCs list once, preferring **GMP > loose folder > ZIP/SFX**.
Within an equal kind, existing encounter order is retained; POCKET GMPs are
still considered before GAMES GMPs. Removing a winner exposes its remaining
copy next startup. Library titles/categories and menu ordering are unchanged.

`/OS/CACHE/GAMES.IDX` remains the only published index file. The `GMI2`
namespace invalidates old scan interpretations. Exact source paths and compact
candidate title/CRC/GAM/library records are keyed by live size/date/identity
metadata. Source kind and final tabs/order are reconstructed by the same live
scanner and library application, not trusted from a cached visible-winner list.
Startup enumerates current sources and GAM entries but skips unchanged payload
parsing; additions, removals, renamed sources, changed metadata and formerly
invalid archives are reconsidered. Missing/corrupt/old caches rebuild. The
usual FAT size/date caveat remains: metadata is not a cryptographic content
hash; deliberately preserving all metadata can defeat an inexpensive check.

Writes use GAMES.TMP, fsync and rename-over; unchanged warm scans do not rewrite
it. Cache I/O errors disable persistence and keep live discovery. This inherits
the kernel's bounded cache-only write contract, **not a general filesystem
journal or a new physical power-loss guarantee**. See kernel cache-storage docs.

## Verification and outstanding qualification

Executed on Linux with GCC host sanitizers and ARM GCC 14.2.1:

- Plain `make test` and `make test-asan` (TOOLCHAIN supplied): 149 discovered,
  142 passed, seven external-data skips each. No owned tests excluded. The
  sanitizer target also runs the existing fuzz driver and 400 deterministic
  ZIP mutation/truncation trials. Linux host linking/preloading now chooses
  libstdc++/LD_PRELOAD; Darwin retains libc++/DYLD_INSERT_LIBRARIES.
- Paired kernel `make test-asan`: 277 passed, zero skipped, plus its native
  VFAT sanitizer campaign. Paired ARM GMOS build and kernel `make boot` pass.
- `tests/smoke_drag_drop_kernel.py`: real scanner and board folder/GMP/ZIP
  readers send exact SDK records through the real checked kernel dispatcher
  and FAT implementation. Long/Unicode folders, long main GAM, long nested
  assets, a nested game beneath another valid game, renamed GMP, ZIP/SFX and
  duplicate precedence read all DICE assets,
  reproduce four rendered frames and 90 keyed ticks. Desktop noise lists zero.
- The same integration reads three existing legacy-game ZIPs without changing
  their source bytes: 45, 95 and 47 files match independent Python zipfile
  extraction, and each plays 90 ticks equal to its loose-file oracle. These
  are real legacy-asset archives from the private reference collection, not
  evidence that their bytes are historically identical to first distribution.
  The available `cd/` ZIPs are non-game tools using unsupported implode.
- Thirteen integration cases pass both synthetic fragmented VFAT wire images
  and independent mkfs.fat+mtools-written cards. The wire-image run also passes
  ASan/UBSan across GMOS, bridge and real kernel core. These are host fake-SD
  tests, **not actual macOS/Windows writers, ARM execution or physical cards**.
- The checked ELF loader accepts/loads GMOS at 8,327,168 mapped bytes, below
  the unchanged 8 MiB program quota. GMP and ZIP share mutually exclusive
  inflate scratch and ZIP name indexes are compact; no kernel quota increase.
  An oversized writable-segment witness still returns `ELF QUOTA`.

The existing synthetic135 benchmark preserves the exact ordered games and
menu CRC `3288680219`. Forty alternating warm runs per candidate show:

| Host FAT metric | Approved scan-speed base | Drag/drop candidate |
|---|---:|---:|
| Warm median | 1.977841 ms | 2.137936 ms |
| Data sectors read | 1,676 | 1,674 |
| Cache bytes read | 27,253 | 26,437 |
| Payload file opens / GAM CRCs | 1 / 0 | 1 / 0 |
| Warm writes / scan log lines | 0 / 2 | 0 / 2 |

Thus the incremental-cache benefit and bounded logs remain, but this host
measurement has **8.09% (+0.160 ms) warm CPU overhead**, not a demonstrated
zero-regression result. Cold samples were about 6.25 / 6.45 ms; they are host
samples, not Pi SD timing. Mac must compare cold/warm listing and actual launch
latency on the approved real card before accepting the performance criterion.
No physical-Pi launch, add/remove, power-cut or board framebuffer run was done.
No QEMU ARM binary or serial Pi device was available in this worker.

Reproduce the optional integration after building host and ARM targets:

```sh
GMOS_HOST=build/host python3 tests/smoke_drag_drop_kernel.py /path/to/kernel /private/out
# Add MTOOLS_BIN=/path/to/mtools/bin for an independently written FAT image.
# GMOS_RSD_ARCHIVES is a path-separator-delimited list of private ZIP inputs.
# With build/asan, preload the compiler's ASan runtime into Python as for make test-asan.
```

The private handoff binds exact signed branches, source/artifact hashes,
commands, complete logs, corpus fingerprints and remaining Mac/Pi gates.
