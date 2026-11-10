# Isolated PC-speaker PCM producer

`src/pcspeaker.c` / `.h` are unlinked qualification components. Neither GMOS.ELF,
JS.ELF nor the host gameplay library links them. No game/session/sound dispatch,
remote `SND` grammar or destructive `gp_take_sound` call changes. The sound branch is not merged; local audio is not shipped.
Hardware playback remains **hardware-untested**. A separate host-only composition probe can render
into private bounded scratch, submit copied bytes to the kernel ring, and commit
the staged producer state only when the entire write succeeds. That probe is not
a syscall integration or a scheduler.

## Format, clock and oscillator

`pc_start` accepts exactly 2160 or 2430 readable bytes and tune index 0..29.
It copies only that tune's 36 unsigned LE frequencies (72 bytes); no input pointer
survives. The 270-byte digital-name tail is ignored in this explicitly
speaker-only configuration. Invalid requests leave producer state untouched.
The caller supplies trusted nonaliasing spans; asset EOF/range/permissions must
be checked by a future integration, not inferred from these C pointers.

A rational accumulator adds 1193180 per emitted frame, subtracting 48000*8191
when a sequencer boundary is crossed. There are exactly 36 steps then silence.
This places each boundary at the first PCM sample at or after the ideal boundary,
less than one 48 kHz sample late, rather than rounding every step to 330 frames.
Tune duration is ceil(36*48000*8191/1193180) = 11863 output frames.
Retrigger resets phase, step and fractional time; stop zeroes state. Silence after
the final step never resurrects a note. Render chunks are bounded to 1920 frames;
zero frames is a no-op, invalid sizes/output leave state untouched.

The original integer phase oscillator uses phase modulo48000 and 50% duty.
Frequency <20 is silent (phase held); frequency >20000 is clamped. It duplicates
mono into stereo S16_LE at fixed amplitude3146, rounded from32767*0.12*0.8, so this
single voice cannot clip. This is not a future multi-channel clipping/mixing test.
Unlike WebAudio's band-limited square, it aliases, has explicit integer phase,
no 5 ms scheduling lead, and holds phase during silent steps. It is not claimed
sample-identical to the browser, or identical to PIT frequency divisor rounding.

`pc_tick_frames` is accounting, **not a live scheduler**: it carries the fraction
for the unchanged GMOS literal549254157 ns10 per tick at48000 Hz. The caller
initializes its remainder to zero and maintains its private serialized lifetime.
The producer does not slow/change gameplay or force eight sequencer steps per tick.

## Sources and attribution

Behavioral references are original RSD `code/GM/SOUND.H:10–29,41–44` (Oliver Stone
header), `PLAYGAME.C:4421–4445,4610–4624`, and Sandbox Edition's
`web/packages/engine/src/formats/sound.ts:1–7,29–47`,
`audio/cmfPlayer.ts:8–12`, `audio/engine.ts:197–225,66–96`, under that project's
retained MIT notices (2014 gandrewstone; 2026 Lewis Moten modifications).
Qualification used Sandbox revision5353512a5b8e87e72321c8a0ba87c5443ef878eb.
Format/clock/level behavior follows those sources; the integer oscillator and
rational arithmetic implementation are original here. No Circle, OPL/VOC or
third-party synthesizer implementation was acquired/adapted. AI: Hermes
GPT-6.1 Sol / openai-codex, implementation, tests and documentation.

## Verification and limits

`python3 tests/test_pcspeaker.py` exercises the actual isolated component;
`AUDIO_SANITIZE=1` enables ASan/UBSan. Cases include format/index errors, all30
selected tunes, LE decoding, zero/low/high/clamp, exact36-step clock oracle,
output amplitude/stereo, arbitrary split buffers, long idle, stop/retrigger and
1000000 gameplay-interval fractional-accounting checks. External qualification
may compile the same source for ARM1176 soft-float and run in qemu-user. Such
execution measures host/QEMU overhead, not Zero W cycles or real deadlines.

The PCM fixture is synthetic rendered data for frequency/duration inspection,
not TV listening. H1 sink/firmware/source-license/channel/cache qualification,
real DMA/IRQ/copy syscall integration, actual producer service during blocking
SD calls, USB acceptance, gameplay scheduling and real Zero W no-drop/audible
acceptance remain open. No saves/demos, VOC, CMF or OPL implementation is included.
