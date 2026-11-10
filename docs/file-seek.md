# Packed-game random access

`src/sdpack.c` is the board transport for `src/gmp.cpp`. Logical offsets still
map through `gmp_read_at` to physical offsets in `NAME.GMP`, `NAME.G01`, etc.
The pack format, inflation code and logical mapping are unchanged.

## Kernel contract and compatibility

The vendored SDK is pinned to sandbox-kernel commit
`a6044c2c28091035b922941b8f402841d49defc4` (ABI 1.10; see `sdk/PINNED`).
Seek was introduced in ABI 1.3 as call 9, `sbx_seek(fd, offset,
&position)`, a 48-byte record using absolute `SBX_SEEK_SET` only.

GMOS passes the running startup major/minor to `sp_set_abi` before opening packs.
Only major 1/minor >= 3 enables seek; the default is disabled. Older kernels and
unknown majors never receive call 9. Defensive `-ENOSYS` disables seeking for
this process and resumes the original reopen/read-discard path. Other seek
errors propagate as the existing pack read failure (`-1`); they do not silently
fall back or move the cached cursor. Successful seeks update it from the kernel
output. Payload read errors retain the prior close-and-recover behavior.

On an enabled kernel, moving within the open volume uses seek, including forward
gaps. Changing volumes still closes the old file and opens the correct new one;
only the per-volume offset is passed to seek. The first-volume header/index cache
remains in use. Per-volume range checks precede cache/file access. Valid absolute
positions include exact EOF; past-end requests fail without clamping. Kernel
seek traverses FAT links but does not replay payload bytes: this is not a claim
of constant-time FAT access. Very small first volumes that split the initial
8-byte header remain unsupported by `sp_open`, as before; the index and payload
can span supported volumes.

## Automated checks

```
make test
make test-asan
python3 tests/bench_sdpack.py
python3 tests/smoke_sdpack_kernel.py /path/to/kernel-source
make all js TOOLCHAIN=/path/to/arm-none-eabi-
```

The ordinary test targets now build the real board `sdpack.c` against a host
transport (`tests/host/sdpack.c`), not only the independent hostio pack adapter.
Tests cover both ABI paths, forward/backward/repeated access, EOF and bounds,
short reads, seek failures and recovery, ENOSYS in both directions, malformed
and truncated packs, missing volumes, the existing `tests/data/dice.gmp` read
byte-for-byte against its loose files, index splits and three-volume data
boundaries, and a deterministic 2,400,000-byte generated pack. No private pack
contents are included.

The optional smoke command compiles GMOS's actual reader with an SDK-record
bridge into the supplied kernel's real checked dispatcher and FAT reader. It
uses fragmented fake media, single/split dice and a 2.4 MB three-volume pack,
checks zero card writes and host-loads `build/GMOS.ELF` when available. It does
not execute ARM instructions. Ordinary `make test` does not require a second
repository.

The benchmark compares the ABI 1.2 fallback with ABI 1.3 using five repetitions
of listing + index open + 50 nonsequential 256-byte entry reads, on a generated
2.4 MB pack. On the implementation host:

| Volumes | Path | File opens | Bytes read | Median ms |
|---|---|---:|---:|---:|
| 1 | 1.2 fallback | 36 | 46,019,013 | 2.995 |
| 1 | 1.3 seek | 1 | 13,088 | 0.387 |
| 3 | 1.2 fallback | 51 | 12,030,548 | 1.238 |
| 3 | 1.3 seek | 36 | 13,088 | 0.680 |

The seek path issues 50 seeks in each row; 13,088 bytes are the 288-byte index
plus 50 * 256 requested entry bytes. A separately compiled immutable develop
`sdpack.c` produced exactly the fallback open/byte counts (medians 3.280/1.402
ms). Timings are warm host stdio measurements, not SD/Pi speed estimates.
Counter differences, not timing alone, prove removal of reopen/replay work.

The Makefile selects libc++ on macOS and libstdc++ on Linux; no Linux linker
adapter is needed. Set TOOLCHAIN to the installed Arm GNU prefix when needed.

The measurements above are historical host evidence. Pi seek performance and
physical SD behavior remain **hardware-untested** by these checks.
