# Host-only scan investigation probes

These tools originated in the profiling investigation and compile the actual
scanner without changing its production source. They don't run on hardware.
The scanner regression tests use the prefix transport too; the tools themselves
are not firmware dependencies. Default operation is read-only. Explicit
`GMOS_SCAN_CACHE=1` enables ABI1.4 cache writes to the supplied **disposable card
copy**; see [the persistent cache contract](scan-cache.md). Output directories
must remain separate from input data.

```sh
python3 tools/profile_scan.py . /path/to/card /path/to/output/scan 5
python3 tools/verify_scan_probe.py . /path/to/output/scan/scan-probe /path/to/output/verify
python3 tools/profile_fat.py /path/to/sandbox-kernel /path/to/output/fat
```

The scan probe compiles the exact scanner prefix of `src/main.c` (ending before
`open_input`) with definition-only renaming for inclusive phase timers. It links
the repository's actual GAM, menu, library, sdpack, GMP and puff implementations.
The output includes its source digest, compiler argv, operation counts, ordered
storage identities, raw timing samples and inclusive phase times. It enables
ABI 1.3 seek by default; explicit cache mode enables ABI1.4. C/C++ compilers and
Python are required. Exact metadata/native time fields in the POSIX shim aren't
physical FAT timestamps; the actual-FAT cache probe covers that separate layer.

**Limits:** native host `readdir` order and POSIX file I/O are not FAT short-name
resolution or physical SD latency. Log calls count bytes but do not send them
through UART/USB; health calls are counted but do not invoke a supervisor.
Inclusive timers overlap (find_gam is inside add_game; add_pack is inside
find_packs). `io_ns` includes host stat/dirent packing but not close calls. A
cold process is not a cold filesystem cache. No framebuffer, thumbnails or
launch execution are measured. Native filesystem naming rules can differ from
FAT's short-name rules, so use representative DOS-safe inputs.
`verify_scan_probe.py` requires a fresh output directory and checks eight
baseline discovery/duplicate/index behaviors with repository DICE fixtures.
It is not sufficient cache regression coverage alone; `tests/test_scan.py`,
`tests/test_scan_cache.py`, the actual-FAT probe and kernel tests provide that.

The FAT probe compiles unchanged `core/assets.c` from a separately supplied
kernel tree against a read-only host `sd_read`. It uses that kernel's existing
`tests/fatgen.py` to build synthetic fragmented FAT32 images, with 1, 16, 64,
122 and 135 directories and 1,315-byte artificial GAME.GAM files. It measures
one GAMES enumeration, then two absolute opens (directory/list + GAM open)
per folder, without reading GAM payloads. It reports 512-byte sd_read **call
counts**, not distinct sectors or real SD timings. It bypasses the syscall
validation layer. Synthetic card.img is removed after successful completion;
all retained sources and result JSON remain in the output directory.

Probes never rewrite production `src/`, SDK or pack data. The implemented cache
and bounded discovery diagnostics are described separately in `scan-cache.md`.
Re-run the probes after code changes: scan source-shape changes can require
updating the extraction point. Keep real card data and generated outputs out
of Git. On-device profiling should use aggregate counters/clock samples and
one summary emitted *after* the listing, with per-folder diagnostics optional.
