# Persistent games scan cache

On sandbox-kernel **ABI 1.4+**, GMOS uses `/sd/OS/CACHE/GAMES.IDX` to memoize
source parsing. Older kernels retain the uncached scanner and never receive the
new metadata/write calls. LIBRARY.JSL is still enrichment, **not a discovery
manifest or authoritative list**. Existing GMP seek/title loading is unchanged.

## Discovery and trust

- Discover packs in POCKET, then GAMES, then loose GAMES folders, in the same
  live native encounter order. Keep existing prefix-GAM CRC duplicate selection
  and final sorting. Cache **all candidates**, including suppressed duplicates
  and invalid sources, not just visible games. Removing a winner therefore
  reveals the same next candidate as an uncached scan.
- Read live names, sizes, raw FAT modification/creation dates and first-cluster
  identities. Match the **exact metadata bytes**, not a metadata CRC. A pack key
  covers every discovered contiguous volume. A loose source key covers the same
  first .GAM that the uncached scanner would choose. Enumerate every DOS child
  folder even if it used to be empty; parent folder dates alone cannot detect a
  newly added/reordered .GAM. No GAM payload/decompression/duplicate CRC repeats
  for unchanged positive or negative candidates.
- Preserve source paths and candidate layout,
  source title/GAM name/duplicate key and embedded library records. Keep the JSL
  bytes as a separately keyed record. Replay embedded records and JSL in their
  original order, preserving category/tab, later-record overwrite behavior,
  metadata matching precedence and reader mappings. Do not cache pointers or
  thumbnail pixels. ZIP/SFX candidates and long paths are supported; see
  [desktop discovery and cache limits](drag-drop.md).
- Names/sizes/dates/identity are a **metadata trust contract**, not proof that
  arbitrary file bytes are unchanged. External tools must update meaningful FAT
  dates or remove GAMES.IDX after edits. A same-size, in-place edit preserving all
  exposed metadata can be invisible. Kernel USB installs explicitly unpublish
  the cache before mutation because their FAT timestamps are fixed, including
  same-name/same-size updates. This conservatively rebuilds all candidates after
  a USB install; ordinary directory changes reuse unaffected candidates.
- The file is bounded to **256 KiB / 1024 records**. Each top-level metadata
  inventory holds at most 512 short-named files. Oversized inventories/cache,
  bad magic/version/count/checksum/lengths, missing cache or unavailable new ABI
  safely fall back to live parsing. Malformed library text keeps the old parser's
  behavior. Any detected source/JSL I/O failure prevents persisting that scan.
  Changed valid sources are parsed again; invalid sources are negative-cached
  only when no underlying pack I/O error was detected.

The portable GMI2 container has a 16-byte little-endian header (magic, byte count,
entry count, CRC32 covering header[0:12] and payload). Each entry has path/key/value
lengths and bounded bytes. The board source value is an asserted 292-byte record
plus up to 4096 bytes of the embedded library record; scalar fields are LE on
supported ARM32/host platforms. Bump the magic when scanner interpretation changes.
CRC32 detects accidental corruption, not malicious cache modification.

## Publication and failure

Only publish a complete newly built cache. Write GAMES.TMP in bounded chunks,
fsync, close, then cache-only rename-over GAMES.IDX. Kernel COW keeps old data
until destination publication and uses independent source/destination chains.
An unchanged warm scan does **zero writes**. An interrupted transaction's temp
is never consumed as an index and may survive until another successful rebuild.

Cache write/rename failures leave the live list usable and emit one diagnostic;
rebuild is still possible in RAM on read-only/full/problematic media. See the
kernel's `docs/cache-storage.md`: whole-sector durability, initially non-cross-
linked FAT and orphan/mirror-repair limitations are explicit. This is not proof
of arbitrary real-card torn-sector or physical-removal safety. No persistent
success is claimed when the writer fails.

## Reproduce host measurements

These are optional probes, not substitutes for the normal regression suites:

```sh
make test
make test-asan
python3 tools/make_scan_fixture.py "$G" "$K" "$OUT/synthetic135.img"
GMOS_SCAN_CACHE=1 python3 tools/profile_scan_fat.py \
  "$G" "$K" "$OUT/synthetic135.img" "$OUT/fat-cache" 6
python3 tools/verify_scan_fat.py "$OUT/fat-cache/results.json"
# Baseline G0/K0 checkouts are read-only detached develop references:
python3 tools/profile_scan_fat.py "$G0" "$K0" \
  "$OUT/synthetic135.img" "$OUT/fat-baseline" 5
# An unchanged COPY of an actual card layout (POSIX, not FAT transport):
python3 tools/profile_scan.py "$G" "$CARD_COPY" "$OUT/native-uncached" 5
GMOS_SCAN_CACHE=1 python3 tools/profile_scan.py \
  "$G" "$CARD_COPY" "$OUT/native-cache" 6
```

`GMOS_SCAN_CACHE=1` deliberately enables writes **to the supplied copy**. Never
point it at original evidence or a live SD device. Default probes are read-only.
The FAT probe runs actual assets.c/fatput.c with file-backed sector I/O, not the
user address-space dispatcher or SD hardware. Kernel syscall tests cover that
separate ABI path. JSON retains compiler commands and production source digests.
The synthetic fixture is 135 variants of repository DICE GAM plus one duplicate
pack, generated JSL and an empty folder—not a real 135-game card copy.

On Linux/x86_64 in the implementation run, five warmed-process baseline samples
were about 14.3 ms vs about 2.0 ms warm cache. The full scanner's sector calls
fell from 23,005 to 1,676, with 136 to zero GAM CRCs and zero warm writes. The
cold build also writes the cache and is a separate phase. These are **host-only**
results; final exact samples/build IDs belong to the private signed handoff.

## Mac/Pi validation gate

Physical-card durability and current full-library performance acceptance remain
**hardware-untested**. The historical host samples above are not current Pi results.

No Pi run or current 135-game card/frame baseline was available on Kaylee.
Retain baseline/candidate card copies. Mac alone reviews/imports bundles and
installs firmware. Record exact kernel commit/build ID, INFO generation, ABI,
card identity, cold power-on vs restart, UART/USB state. User's original numbers
were generation24, approximately23s listing /122 games and72ms GMP title loading;
those are user-reported, not measurements of this change.

With the normal reviewed install procedure, use actual serial PORT and check:

```sh
python3 "$K/scripts/usb_kernel.py" "$PORT" info
python3 "$K/scripts/usb_kernel.py" "$PORT" stop
python3 "$K/scripts/usb_kernel.py" "$PORT" put "$G/build/GMOS.ELF"
python3 -u "$K/scripts/usb_kernel.py" "$PORT" run GMOS.ELF --seconds 60 2>&1 |
  python3 -u -c 'import sys,time; [(print(time.monotonic_ns(),s.rstrip(),flush=True)) for s in sys.stdin]'
```

After any PUTF (including reinstalling GMOS), the cache is intentionally missing:
measure the first rebuild **separately** from at least five subsequent restarts
without PUTF. Compare old/new GMOS and old/new kernel separately to isolate FAT
sector batching vs cache reuse. Record at least three cold launches too.
Measure hello→`GMOS N games in /sd/GAMES`, then first/all-thumbnails-ready timing.
Verify readiness <10s and heartbeat behavior, particularly bounded fsync/rename
latency, on the actual card. Serial receipt times include transport/log latency.

Capture settled `GMOS list frame crc` for an identical recorded tab/page/key
sequence on baseline, cold rebuild and warm startup. The old board-list.txt
reference is explicitly stale; do not substitute it. Play/return one loose and
one GMP game and recheck ~72ms GMP title loading. Test missing/corrupt JSL and
cache, add/remove sources, new GAM in an empty folder, first-GAM replacement,
changed/removed duplicate winners, split volumes, same-size USB replacement and
read-only/full media. Qualify interrupted temp/fsync/rename/remount on a disposable
card, including fsck if needed. Do not claim physical-removal safety merely from
host write-cut tests, menu CRCs, or POSIX rename.

## Archive ZIPs and a quiet scan

`/sd/ARCHIVE` ZIPs are keyed in the same cache by their live name, size and dates
(path `/sd/ARCHIVE/NAME`), so a warm boot reads none of them: before the first draw
every known ZIP joins the Archive tab from the cache. Unknown or changed ZIPs are
read one per idle pass; any known ones after them join free in the same pass. A
record is added only while it fits (the whole cache is dropped on overflow); the
cache is written once, when the last ZIP is read (`cache_wait`/`cache_seal`).
The key is card metadata, never the GMOS build, so installing a new GMOS does not
rescan. Only the `GMI2` magic changes that.

While files are left (`menu.scan_left`) the header shows "Scanning games... N left"
and the Archive and Search counts show "..." until the scan ends. The list redraws
every 3 seconds, and once more at the end. The scan runs only in the idle list loop,
never during a game or an idle demo.
