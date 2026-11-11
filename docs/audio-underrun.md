# Audio underruns: 2026-11-10

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
