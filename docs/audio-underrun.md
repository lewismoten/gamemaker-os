# Audio refill starvation: 2026-11-11

This follow-up supersedes the 120 ms gameplay policy in the historical notes below.
No board run was performed; the numbers here are deterministic host simulations.

The ring is 16,384 stereo frames at 48 kHz (341.3 ms), but gameplay used only
5,760 frames (120 ms). A single 150 ms draw/read consumes 7,200 frames: even a
full gameplay queue cannot cover it. The 18.2 Hz tick is about 54.925 ms, and
render, scale, present, diagnostics and file reads can compound between polls.
The library's 250 ms threshold was larger, but details decode checkpoints only
serviced audio indirectly when their progress indicator actually presented.
A long decode without a progress redraw could therefore starve background CMF.

OPL2/CMF, streamed VOC and PC-speaker beeps share `au_render`. CMF advances at
about 145.7 sequencer ticks/s independently of game ticks. Pumping must replace
samples consumed while synthesis runs. The previous per-call budget of twice
the target allowed 11,520 gameplay or 30,000 library frames of synthesis in one
call. At the test's 4 ms CPU per 480 frames, those budgets take 96 or 250 ms.
VOC prescanning also reset its visit budget for each sample segment, allowing a
whole many-segment clip to be scanned synchronously despite a nominal budget.

The fix:

- Both modes use a 12,000-frame (250 ms) low-water floor and a 15,000-frame
  (312.5 ms) target, clamped to the reported kernel capacity. The floor covers
  the requested 150 ms stall, one 55 ms game tick, and 45 ms synthesis/scheduling
  margin before runtime observations exist. The effective threshold is the
  larger of that floor and `ceil((worst gap + worst synth slice + 20 ms) * 48000)`,
  capped at the target/capacity. Observations include library and quiet polls;
  lifetime maxima survive interval report resets and gameplay transitions.
- Every pump renders at most 1,920 frames (40 ms of audio), writing at most
  480 frames (10 ms) before checking live queue drainage again. At the test's
  40% synth CPU load this is at most 16 ms of synthesis per call. An overloaded
  synth still returns after that frame budget. Partial-write/EAGAIN tails keep
  their original sample order. Short writes no longer need a 1,920-frame hold.
- Refills run before/after game ticks and core rendering, scale/compare, list
  drawing and frame presents, details decode checkpoints, and around loose,
  pack and ZIP reads. The decode hook stays active outside details loading.
  Heartbeats service audio around progress redraws; waits retain 10 ms slices.
  Tick debt and the one-report-line-per-successful-present budget are preserved.
- VOC preparation shares one 64-visit budget across all segments in a service
  call and continues on subsequent calls. Accepted/refused counters advance
  once at completion; cancelling a pending clip does not count it as accepted.
- Audio logs now measure gaps/service time in every mode and print the effective
  low-water mark. Gap/service maxima reset per log; cumulative audio counters
  and the lifetime threshold observations do not reset.

`tests/host/audio_clock.c` drains the fake ring during stalls, synthesis and
sleep, using a real CMF through the production mixer. Twenty 150 ms stalls at
the low-water boundary produce **zero underruns** in both game and library mode
with 4 ms synthesis cost per 480 frames. A counterexample starting each stall
with the old 5,760-frame target produces **40 underrun events** including
recovery drainage. Tests prime the ring before injecting the stalls.

Both 60-second simulated ring tests (16,384 and 4,096 frames) execute **1,093
game ticks**, report **zero underruns**, write at most **480 frames** at once,
and sleep at most **10 ms**. Three-second monotonic-clock runs also have zero
underruns. A counter test observes a 280 ms quiet library gap, raises the
threshold to **14,400 frames**, and verifies that interval resets and gameplay
transitions retain this sizing. The VOC test verifies 64, then 128, then
completion at 130 sample blocks, including acceptance/refusal/cancel counters.
PCM identity, partial writes, EAGAIN, capacity clamps, fades, speaker timing and
the reference OPL waveform remain covered by the host tests.

The tradeoff is effects latency of up to 312.5 ms. An older 4,096-frame ring
holds only 85.3 ms and cannot survive an unsliced 150 ms stall. No queue covers
sustained synthesis slower than real time or arbitrary storage stalls; runtime
gap and synth CPU logs are still needed for board qualification.

Final verification: `make all test` passes **244 tests (7 skipped)**, with the
ARM image at **7.04 MiB of 8 MiB**, leaving **984 KiB** headroom. `git diff
--check` passes. No network, sanitizer, display window, audio device or commit
was used; changes remain in the working tree.

## Historical diagnosis and verification (2026-11-10)

## Cause

The reported Pi Zero W run had about 104 underruns per five seconds, with
audio service/pump averaging 4–4.7 ms and peaking at 9–25 ms. These are the
owner's board measurements; this fix has not been run on hardware.

`AU_CHUNK` was 480 frames, or 10 ms at 48 kHz. It was **not** a one-chunk-per-tick
limit: `au_pump` already looped over chunks, and the play loop already capped
its wake to 10 ms. The game tick remains 54.9254157 ms (about 18.2 Hz). Render,
present, synthesis, storage and diagnostics can still delay the next pump;
the old `sbx_sleep_until` itself did not service audio.

The old gameplay low/target levels were 2,400/3,120 frames (50/65 ms). More
importantly, the pump computed the deficit **once**, then subtracted writes
without accounting for the ring draining while OPL synthesized those writes.
At 40% synthesis CPU cost, rendering a nominal 65 ms refill takes 26 ms,
leaving only about 39 ms queued, not 65 ms. That is less than one game tick
and vulnerable to the measured long stages. The kernel's 16,384-frame capacity
(341 ms) did not help because gameplay deliberately used only a small part.
An older 4,096-frame ring holds only 85.3 ms.

## Change

- Gameplay tops up to 5,760 frames (120 ms) on every pump, clamped to actual
  capacity. Library/loading retains its 12,000/15,000-frame policy.
- The pump queries again after every successful write, including partial
  writes. It replaces frames consumed during synthesis instead of mistaking
  frames written for the current queue level. A 48-frame (1 ms) finishing
  margin avoids chasing tiny deficits with repeated synthesis/syscall overhead.
- Large deficits render batches of up to 1,920 frames (40 ms) into a retained
  buffer, still using the same 480-frame synthesis slices. Partial writes and
  EAGAIN retain the unsent tail in sample order. CPU accounting includes all
  slices. A call renders at most twice the target, preventing an unbounded
  refill loop if synthesis cannot keep up with real time.
- Board waits use the same absolute deadline but sleep in at most 10 ms
  slices, pumping before and after each slice. Existing input polling and
  session tick deadlines remain unchanged. Heartbeat pumping remains enabled.
- CMF/OPL synthesis, VOC mixing, PC-speaker timing, sound selection/retrigger
  rules and PLAYGAME events are unchanged. The deliberate tradeoff is more
  output latency (up to about 120 ms), not altered sounds or game speed.
- The five-second report still emits at most one report line per successfully
  presented frame; sleep pumping emits no extra report lines.

Bigger writes reduce transport calls, not OPL's per-sample CPU cost. The
existing `synth CPU ... per mille` report is still needed to distinguish
insufficient scheduling headroom from sustained CPU overload. No finite queue
can hide a permanent inability to synthesize 48,000 frames per second, or an
unserviced stall longer than its headroom.

## Host regression

`tests/host/audio_clock.c`, run by `tests/test_audio.py`, exercises the production
pump and sliced wait with both 16,384- and 4,096-frame fake rings. They drain
at 48 kHz during **all** elapsed time: game rendering/presenting, synthesis and
sleep. It runs deterministic 60-second simulated sessions and three-second
monotonic-wall-clock sessions for each ring size. Randomized game/render costs
are 9–25 ms plus up to 6 ms of presentation; injected synthesis costs are
3–5 ms per 480 frames, with occasional additional 5 ms stalls. Each run must
have zero underruns, bounded sleep slices and larger catch-up writes. Existing
tests still check PCM identity, partial writes, EAGAIN, capacity clamping,
music fades, speaker timing and the reference OPL waveform; gameplay latency
now allows 130 ms rather than 80 ms.

```sh
python3 -m unittest discover -s tests -p test_audio.py -v
make test
make all js quota
```

No sanitizer, DYLD settings, ctypes probes, commit or device install is used.

Final host verification: `make test` passes 212 tests (7 environment-dependent
skips). `make all js quota` passes; GMOS image headroom is 1,000 KiB and JS
headroom is 1,772 KiB. Both simulated and wall-clock ring runs report zero
underruns; the simulated runs each execute 1,093 ticks in about 60 seconds.
The overload case also verifies that slower-than-real-time synthesis returns
at the per-call rendering budget instead of spinning indefinitely.

## Expected board log

The open line should still report the real kernel capacity. During steady play,
successive `GMOS audio game 5 s` rows should show the cumulative `underruns`
counter staying flat (ideally zero after startup), no new overruns/EAGAIN,
and around 240,000 additional rendered frames per five seconds. The logged
queue is an instantaneous sample, not a guaranteed 5,760: it drains between
refills. After a refill it should be close to 5,760 on the current kernel, or
4,096 on the older ring, instead of repeatedly approaching zero. Pump gaps
should be comfortably below this headroom; the 18.2 Hz tick and one-line-per-
present report cadence remain unchanged. Sustained growth in underruns still
requires checking pump-gap, sound-read and synth-CPU figures on the board.
