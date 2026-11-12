# Scan diagnostics and earlier speed measurements

The diagnostic suppression below remains in use. The later
[persistent cache](scan-cache.md) and [ZIP/long-name discovery](drag-drop.md)
are now implemented. The timings here describe the earlier diagnostic-only
change, not the current scanner. Full-card performance acceptance remains
**hardware-untested**.

Normal startup now counts per-entry pack/folder diagnostics and emits at most one
aggregate summary instead of synchronously logging every skipped folder and pack.
Readiness/heartbeats, missing-folder/index reports, all discovery, prefix CRC32
identity, duplicate winners, launch mapping, JSL enrichment and ordering remain
unchanged. The post-scan `GMOS game ...` listing in `main()` is still present; the
new suppression applies to discovery diagnostics, not that later dump.

For full scan diagnostics, force a rebuild (make does not track variable changes):

```sh
make -B SCAN_VERBOSE=1 TOOLCHAIN=/path/to/arm-none-eabi-
# Restore the default summary-only discovery diagnostics:
make -B SCAN_VERBOSE=0 TOOLCHAIN=/path/to/arm-none-eabi-
```

The host scanner probe uses default summary mode; `GMOS_SCAN_CFLAGS` may supply
`-DGMOS_SCAN_VERBOSE=1` or sanitizer flags. It reports full sorted game metadata,
storage indexes and pack-volume mapping, plus CRC32 of the complete menu and
mapping objects. Those CRCs are **not framebuffer CRCs**. `tests/test_scan.py`
executes the actual scanner prefix, sdpack and GMP reader through POSIX transport;
the sanitizer suite also instruments this compiled scanner. Tests cover missing,
corrupt and stale JSL, live add/remove, previously empty folders, duplicates,
packed priority/mapping, irrelevant folders and same-size GAM changes.
`tests/test_zip.py` and `tests/test_scan_cache.py` cover later ZIP and cache work. Existing sdpack tests cover split
volumes and ABI 1.3 seek/legacy fallback.

```sh
make test
make test-asan
python3 tools/profile_scan.py . /private/card-copy /private/output 5
python3 tools/verify_scan_probe.py . /private/output/scan-probe /private/checks
```

The Makefile now selects libstdc++ on Linux and libc++ on macOS; the earlier
Linux linker adapter is no longer needed.

Host evidence: the stress fixture (DICE, 20 identical copies, 140 irrelevant long
folders) retains one game and 21 CRC checks; discovery logs fall from 161/10,275
bytes to 2/115 bytes including the no-index report. With a counting-only log sink,
median native listing is 1.665250 ms before vs 1.628092 ms after (five samples).
A reconstructed layout of 56 real GAM files gives 1.881879 vs 1.890749 ms: **no
meaningful POSIX scan-speed gain**. Neither fixture is the requested real 135-game
card. The complementary sandbox-kernel `feature/scan-speed` change reduces actual
FAT path-lookup SD-read amplification; do not infer its physical latency from
these POSIX times. See private task handoff for raw evidence and Pi instructions.

## Current cache status

`src/scan_store.h` and `src/scan_cache.c` now implement the on-card cache,
using live metadata, candidate records, fresh library enrichment and staged
write/fsync/rename. ABI 1.4 adds the cache calls; the vendored SDK is ABI 1.11.
See [scan-cache.md](scan-cache.md) for trust and failure limits, and
[drag-drop.md](drag-drop.md) for the current GMI2 format and discovery behavior.
Physical-card power-cut safety and full-library Pi timing still need acceptance.
