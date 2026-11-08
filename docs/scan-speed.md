# Scan-speed partial implementation (t_04029321)

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
packed priority/mapping, irrelevant folders and same-size GAM changes. ZIP remains
unsupported exactly as in the uncached scanner. Existing sdpack tests cover split
volumes and ABI 1.3 seek/legacy fallback.

```sh
make test
make test-asan
python3 tools/profile_scan.py . /private/card-copy /private/output 5
python3 tools/verify_scan_probe.py . /private/output/scan-probe /private/checks
```

For Linux's libstdc++ vs the existing `-lc++` link option, the recorded task uses a
workspace-local linker script `libc++.so` containing `INPUT(-lstdc++)`, then passes
`HOSTCC="cc -L/path/to/that/directory"`. It does not change global libraries. Use
`ASANPY="env LD_PRELOAD=$(cc -print-file-name=libasan.so) python3"` on Linux.

Host evidence: the stress fixture (DICE, 20 identical copies, 140 irrelevant long
folders) retains one game and 21 CRC checks; discovery logs fall from 161/10,275
bytes to 2/115 bytes including the no-index report. With a counting-only log sink,
median native listing is 1.665250 ms before vs 1.628092 ms after (five samples).
A reconstructed layout of 56 real GAM files gives 1.881879 vs 1.890749 ms: **no
meaningful POSIX scan-speed gain**. Neither fixture is the requested real 135-game
card. The complementary sandbox-kernel `feature/scan-speed` change reduces actual
FAT path-lookup SD-read amplification; do not infer its physical latency from
these POSIX times. See private task handoff for raw evidence and Pi instructions.

## Persistent-cache requirement remains open

There is **no on-card cache in this partial implementation**. Every launch still
rediscovers all candidates and hashes GAM prefixes, so no new stale-cache trust is
introduced. Missing/bad JSL remains metadata-only, never authority to hide files.
No titles, thumbnail loader, GMP seek path or SDK were changed.

The clarified one-file card cache needs source inventories (including duplicates
and empty folders), per-source metadata and changed-source rescans, bounded and
checksummed decoding, fresh JSL application, install-aware invalidation and a
kernel-backed atomic write/replace transaction. Current SDK lacks dates and
application create/write/rename; kernel SD opens remain read-only. FAT names,
sizes and timestamps alone also miss same-size USB PUTF replacements because the
writer assigns fixed dates. No host-only emulation or unsafe FAT write shortcut is
included. Actual 135-card timing and current Pi games-page frame CRC parity remain
unverified; the Mac owns integration/hardware validation.
