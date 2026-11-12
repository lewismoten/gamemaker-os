# Play frame performance

Investigation on `fix/frame-rate-1110`, 2026-11-10. Host measurements and Pi
estimates are distinct; this change has not been run on a Pi.

## Reproduce

From the repository root:

```sh
make build/host/frame_bench
build/host/frame_bench /Users/lewismoten/dev/GameMaker/games/year2000/disk year2000.gam 1000
make test
make all js quota
```

The benchmark accepts any unpacked game directory, GAM filename, and tick count
(1–1,000,000). It opens the first playable scene directly, seeds the world with 1,
and holds Right for 32 ticks, Left for 32, repeating. If a link/death/win occurs,
it reloads the first scene and reports the restart count. Loading is outside the
timed stages. Year 2000 needs no restarts in the measured 1,000 ticks.

It times the real tick, index compositor, old and new scaling, old and new frame
hash, a CPU present proxy, and first-level CMF synthesis for one tick of 48 kHz
stereo PCM. The old scaler is frozen in the benchmark as an independent pixel
oracle. Timing probes run in fixed order on the same frame; host scheduling and
cache warmth affect results. Maxima include host preemption. The default host
build uses `-O1`; the actual ARM build uses `-Os` and a freestanding byte-loop
libc. Host microseconds cannot be converted to Pi cycles by a CPU frequency ratio.

Audio here measures music synthesis, without VOC effects, ring transport or file
service. Missing music, as in the small DICE fixture, measures silence. Present
is volatile writes with a color swap into cached host memory, without HDMI,
syscalls, uncached stores, interrupts, cache effects of kernel aliases, or UART.
E-ink has no useful host wall-time analogue. The board stage counters cover those
costs instead.

## Host results

Final run on this arm64 macOS host, default `HOSTFLAGS=-O1`, 1,000 Year 2000
ticks (54.9 s of simulated game time), after the test/build jobs completed:

| Stage | Before mean µs/tick | After mean µs/tick |
| --- | ---: | ---: |
| Core tick + drain sound queue | 2.25 | 2.25 |
| Composite indexed frame | 24.11 | 24.11 |
| Scale/write staging rows | 51.28 | 85.28 (includes dirty comparison) |
| Diagnostic view hash | 97.00 | 78.01 |
| Present proxy | 198.91 | 136.72 |
| CMF PCM for one tick | 170.78 | 170.78 |
| Sum of these measured stages | 544.33 | 497.15 |

The new unconditional scaler alone averages **47.39 µs**, versus 51.28 µs for
the old scaler. The production dirty path spends extra CPU comparing rows in
exchange for fewer uncached hardware writes. The approximately 9% improvement
in the sum of host stages includes a cached-memory present proxy; it is not a
Pi FPS result. No change was made to audio synthesis. E-ink, asset reads, kernel
transport, UART and sleep/wake latency are measured only by the board counters.

The run reports 328.5 presented rows/tick, zero first-scene restarts, checksum
3239199777, identical old/new pixels and hashes, and 125 character frame changes
in the separate 128-tick sparse-draw check. Final verification: `make test`
passes 208 tests (7 environment-dependent skips); `make all js quota` passes.
GMOS image headroom is 1008 KiB; JS image headroom is 1772 KiB. No sanitizer,
DYLD settings, commit, device install or hardware run was used.

## Actual frame path

* Play switches to **640×480 XRGB8888** (`src/main.c`, `PLAY_W/PLAY_H`). The library
  uses 960×540 RGB565. Firmware scales the chosen mode to the HDMI output.
  Preserving output requires keeping the current 2× layout: full cards are
  640×400 at y=40; the cropped play view is 624×392 at x=8,y=44. A 3× scale would
  change the geometry and pixels.
* `gs_step` advances the shared play core at the PIT-derived period
  `GS_TICK_NS10/10 = 54,925,415.7 ns`. `World::tickInner` performs scrolling,
  monsters, character movement/animation and background block changes;
  `World::tick` also builds the sprite draw list. `gp_render` composites indexed
  rows, including sprites, into the session's 320×200 array.
* The old scaler already built one 256-entry 6-bit VGA to XRGB palette table per
  frame. It repeated the lookup/expansion for both copies of every source row.
  The new path expands each source row once and uses aligned word copies for
  the output rows. It compares against the mapped staging surface, writes only
  changed rows, and returns a single bounding row span. No retained cache or
  extra full-frame allocation is required. Every row outside the crop stays black.
* The measured kernel revision `a6044c2`, `core/sys.c::ioctl_fb`, copies the
  requested rectangle from the application's write-back staging pages to an
  uncached hardware framebuffer, calling inline `fb_native` per 32-bit pixel.
  `drivers/fb_hw.c` maps the hardware buffer uncached. `core/uspace.c` documents
  physically tagged cache aliases: the kernel reads the same staging cache lines.
  There is **no per-present flush syscall, page flip, vsync wait or second
  scanout buffer** in this path. The staging-to-hardware copy is not a swap.
* The old per-tick diagnostic called `gp_frame_hash`, rendering all 196 view rows
  a second time before hashing. It now hashes the existing indexed pixels, with
  exactly the same FNV result and serial line format.
* `heartbeat` can pump audio. The explicit audio block also services requested
  assets and feeds the ring, waking at most 10 ms later. The synth produces
  48 kHz stereo; the original play policy kept 50–65 ms queued (superseded by
  [bounded refills and sliced waits](audio-underrun.md)). Asset I/O and synthesis
  share the single CPU with play and presentation.
* `ge_poll` checks the clock and sends only changed status, at least 2.1 s apart.
  A steady title does not trigger a physical e-ink refresh every frame.
* `log_line` in the pinned kernel fans out to the console ring, polling PL011
  UART, and USB log queue. The UART settings imply roughly 115200 baud. Forty
  bytes take about 3.5 ms on the wire, though the FIFO can overlap some of that
  with execution. Serial diagnostics are a real cost; console_log itself only
  updates its character ring.

## Timing and animation

PLAYGAME.C's `NewTimer` advances `TimerCounter` at the BIOS/PIT tick rate;
`playscene` calls `movechars` using that clock. `movechars` advances `chr.cframe`
when `clock >= ci.NextAni`, then sets `NextAni = clock + CHRFRAME.pause`.
The shared core has the same rule in `moveChars`, including empty frames,
sequence changes and per-frame pauses. Neither `gp_render` nor `gs_draw` advances
animation. No change to character pauses or core simulation was needed.

The session previously ran up to four overdue ticks and then reset its time base
when still far behind, discarding time. It now retains the debt, runs at most four
ticks per slice so input/audio/health can run, and renders only once caught up
(or immediately on a level transition). It never draws the intermediate catch-up
states. The normal next-tick deadline remains tied to the original time base.
Explicit pauses and level loading retain their existing clock-reset behavior.
This preserves deterministic PIT-rate core ticks under overload; it does not try
to reproduce every visual artifact of an overloaded DOS machine.

The native regression compares 128 ticks of dense drawing against eight-tick
late arrivals drained in four-tick slices without intermediate draws. Year 2000
has **125 character frame changes**, with identical sequence, frame, position,
and final view hash at every compared boundary. The scaler oracle compares all
pixels and row padding on every benchmark tick, plus card/play transitions,
palette changes, an external staging-buffer edit, unchanged frames and forced
full redraws. Separate simulated scanout buffers verify the partial-present span.
The DICE version runs in `make test` without an external GameMaker checkout.

## Pi budget and hardware follow-up

One tick allows about **54.9 million cycles at 1 GHz**. A full 640×480×4 present
writes 1,228,800 bytes. In the 1,000-tick Year 2000 script the new bounding span
averages 328.5/480 rows: about 841 kB per tick, **31.6% fewer uncached writes**.
At 18.2065 ticks/s that reduces hardware writes from about 21.3 to 14.6 MiB/s.
Unchanged frames issue no present. Scrolling or widespread animated backgrounds
can still change almost the whole view, so dirty-row savings vary by scene.

A bandwidth sensitivity estimate, **not measured Pi throughput**: at an effective
20–100 MiB/s, those hardware stores alone take about **12–59 ms before** and
**8–40 ms after** per tick. At 50 MiB/s the estimate is 23.4 → 16.0 ms. Add
simulation, staging reads/writes, color conversion, audio and logging. CPU-side
row comparisons cost time; the host benchmark reports that overhead explicitly.
The working hypothesis is that uncached presentation, audio and diagnostics are
more important on the Pi than the core tick. Only board counters can rank them.

The ARM program now emits `GMOS perf` lines every five seconds with call counts,
total, mean and maximum microseconds for:

* tick slices (up to four core ticks), indexed rendering, scale/compare, present;
* explicit audio service/pump, heartbeat including its audio hook, e-ink;
* hashes/CRC and per-tick diagnostic logging.

These use the kernel monotonic clock, include elapsed syscall/interrupt time,
and do not read a privileged CPU cycle counter. A tick-slice mean must be read
alongside the total tick count, not treated as the mean of one tick during catch-up.
Audio hooks during asset reads can be nested inside other measured operations;
therefore the totals are not a perfectly disjoint CPU accounting ledger. The
existing audio report also gives pump CPU time, underruns and service/gap maxima.
The five-second summary's own serial output is outside these stage samples and
can itself delay a later slice. No benchmark sleeps or synthetic vsync waits were
added. The owner should compare `present`, `audio`, `diagnostics`, and `late max`
while actually walking, and retain the mode/clock lines with the report.

Expected behavior: animation follows every simulated tick; short stalls skip
visual intermediates while the simulation catches up instead of permanently
slowing the game clock. Lower framebuffer traffic and removal of the redundant
render should leave more of the tick budget available. Full-rate output still
depends on the board fitting all work within that budget; this is not a measured
Pi FPS claim. No hardware was flashed or started during this investigation.

## Board result: the stalls were log loss and a report burst (2026-11-10)

On PI-1 (Pi Zero W), holding Right, Left and Down in Year 2000 level 1:

- The per-step `GMOS t N` lines showed tick gaps up to 11, but those gaps are serial
  log lines the kernel dropped, not stalls: the 5 s lines counted 91 to 93 ticks and
  91 to 92 frames per 5 s (18.2 Hz, every tick drawn). Do not read frame rate from the
  `GMOS t` lines.
- The device now counts it itself: `GMOS 5 s ticks per step: 1 x A, 2 x B, 3 x C, 4+ x D`.
- A real stall remained: the 5 s report was a burst of about 12 serial lines in one
  loop pass, holding the loop up to about 100 ms (late max 70 to 107 ms, one or two
  2-tick steps per 5 s). The report now goes out one line per frame.
- After: 91 to 92 steps of 1 tick per 5 s, none of 2 or more; late max 11 to 28 ms.

## Review fixes (run624)

The report header, histogram, audio row and stage rows now share one budget:
at most one report line immediately after each successful framebuffer present.
Catch-up iterations, unchanged draws and failed presents do not drain it. An
unfinished snapshot is retained; later five-second snapshots are skipped while
it is pending. The next accepted header includes the cumulative `reports dropped`
count. Sampling windows still reset at each five-second check, so skipped windows
are not silently merged into a later five-second report.

Level ticks are now 64-bit. Due ticks and next deadlines split quotient and
remainder before multiplication, avoiding the intermediate overflow at about
58 years as well as the old 32-bit counter wrap at about 7.5 years. Total tick
IDs remain modulo 2^32; histogram and sound-repeat logic use unsigned differences.

`tests/test_play_report.py` compiles the production report helpers and present/
snapshot call sites with fake clock, log and framebuffer I/O. It covers the
zero-frame drain, delayed double-line/overwrite witnesses, retained 111 us versus
new 222 us samples, drop reporting, and the audio row's frame budget.
`tests/host/timing_check.c` runs the real host session/core across UINT32_MAX,
checks bounded catch-up and resume, and compares late-session boundaries against
128-bit host arithmetic. These are host regressions, without a new Pi result.
