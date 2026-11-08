# Unlinked PCM8 VOC host core

`src/voc_host.c` / `.h` are **not in any production build list**. They do not
open an audio device, consume a game event, read a game file, schedule a producer,
or enable HDMI/DMA/IRQ. `sound.c`, the one destructive `gp_take_sound()` call,
remote SND grammar, SDK/ABI and all boot paths are unchanged. This is a bounded
host qualification component, not an available Pi audio feature.

## Ownership and calls

The caller supplies a nonnull, initialized, serialized `struct voc_host` in
private fixed storage (not a small task stack). `voc_stop` initializes it.
`voc_begin` stops the previous digital voice even on rejection, validates the
header and copies the complete source. The source may then be overwritten or
released. No caller pointer survives. The caller must establish the actual EOF
and complete length first; a capped read returning its cap is not an EOF proof.
Source spans may be unaligned but must be mapped, nonaliasing and complete.
This trusted-memory seam is **not syscall pointer/permission/lifetime checking**.

`voc_prepare(v, budget)` does 1..64 block visits per call. It returns 1 while
pending, 0 only after the complete clip validates, or a stored negative error.
No output is published before validation finishes. The metadata index retains
only cumulative sample ends and source offsets (or a silence sentinel), not a
decoded sample array. Repeat iterations add bounded span descriptors, not PCM
copies. All source bytes stay in the owned buffer. Zero-length data contributes
no descriptor. The full per-clip cap is **10000 block visits, including the
terminator**; a cap hit is an explicit limit error, not truncated-success.

`voc_render` writes at most1920 frames of48000Hz mono duplicated to interleaved
stereo S16_LE. It uses absolute rational source positions, linear interpolation
across block/silence/repeat boundaries and a repeated final source sample at the
right edge. Frame count is `max(1, floor(samples*48000/rate))`, as in the web
Safari fallback. PCM8 is `(byte-128)*256`; master gain4/5 is applied once using
signed64-bit intermediate arithmetic and division toward zero. Output after
stop/end is silence. Split buffers give identical samples and state. Pending or
failed validation returns an error without output/playback mutation. A zero-frame
render is a no-op, even when pending. Invalid output/count arguments are atomic.
Object corruption, partial overlap and concurrent calls are outside this seam.

`voc_speaker_render` combines that voice with the existing, unmodified
`pcspeaker` producer, using7680 bytes of bounded local scratch. Speaker samples
already contain the approved0.12 level and0.8 master factor; they are not scaled
a second time. `pcm_mix` adds signed16-bit stereo values in signed32-bit and
saturates to[-32768,32767]. Exact in-place output is supported; partial overlaps
are forbidden. Speaker and digital stop/retrigger remain independent. If VOC is
not ready, neither output nor the speaker clock advances. A future scheduler
must explicitly decide whether to stop the failed/pending voice and continue
speaker-only output; this module does not make that live policy choice.

## Supported subset and explicit errors

- Complete26-byte header, full signature, check word and first-block offset
  bounds. Header extensions before the first block are allowed. Version is not
  separately restricted; its check word must validate.
- Mono PCM8 type1 (codec0) and type2 continuation after a type1, silence3 with
  exactly3 payload bytes, terminator0 with no trailing bytes.
- One nonnested repeat6/7 pair at a time. Finite sections play count+1 times;
  count0 and0xFFFF play once, matching the web safety policy. Unmatched ends,
  unterminated repeats and wrong fixed-size block payloads fail. Nested repeats
  are unsupported, not partially decoded.
- One constant rounded sample rate across every type1/silence block, all256 old
  time constants (3906..1000000Hz). Rate-changing graphs are unsupported.
- Source≤2MiB; duration≤120seconds of source samples; ≤10000 block visits and
  ≤10000 descriptors. `sizeof(struct voc_host)` is2177204 bytes on the tested
  host/ARM layouts; no heap allocation or decompressed-duration-sized buffer.
  This reservation has **not** been accepted as a live GMOS memory budget.
  Preparation yields between64-visit slices; the initial bounded source copy is
  synchronous and has no real Pi latency acceptance.
- `-22` malformed/truncated/bad call, `-95` unsupported type/codec/rate graph,
  `-7` source/duration/work limit, `-11` render before ready. On content failure,
  call `voc_stop` for a silent idle voice or begin a new clip. No fallback beep is
  silently selected. A failed retrigger cannot resume the old digital voice.
- Types4/5/8/9 and unknown types are unsupported in this slice. In particular,
  PCM16, extended-rate/stereo and ADPCM codecs1..3 are not implemented. Rejecting
  **all** block9 and8 forms is stricter than parsing and checking their channels;
  no unsupported channel or codec can be accepted through those forms.

## Actual source provenance and oracle boundary

Behavioral reference: Game-Maker: Sandbox Edition at
`c97a75265160776318fdde804134b22b01c3fe40`, MIT source copyright2014 gandrewstone
and2026 Lewis Moten modifications, retaining that repository's notices.

- `web/packages/engine/src/formats/voc.ts:1-18,25-125`: signature, block grammar,
  PCM8 conversion, rounded time constant, repeat count+1/infinite-once and10000
  guard. The C memory/index/validation implementation is original here.
- `web/packages/engine/src/audio/engine.ts:238-269,273-295`: digital replacement,
  independent stops and the explicit linear resampling fallback. The browser's
  normal AudioBuffer resampler is platform-dependent; neither its waveform nor
  audible output is claimed byte-identical. The fixed implementation is exact
  against an independent integer oracle and within1.01 S16 LSB of the specified
  Float32 fallback after0.8 master, in the private qualification corpus.
- The web decoder **does not check block9 channels or codec** (`voc.ts:100-105`);
  it picks PCM16 for bits16 and PCM8 otherwise. Its block8 path ignores packing
  and only interprets mode1 as stereo (`:93-98`). It accepts truncated payloads
  by clamping, skips unknown/marker/text types, and concatenates rate-changing
  data. This module deliberately rejects these forms; those omissions are not
  adopted as native safety behavior. It does not adopt the web's ADPCM tables.
- Original `code/GM/SOUNDC.C:153-233,236-253` (Peter Savage/RSD) stops the prior
  voice, opens the named VOC, checks only `Cr`, uses the **low byte** at20 for
  driver offset, then delegates to resident Creative driver function6. That
  absent driver is not a portable PCM oracle. Source-grounded DOS stop/header
  comparisons are static; no Creative driver execution or DOS PCM equivalence
  is claimed. This decoder validates the actual u16 container offset instead.

All525 tracked real VOC assets at the reference pin were classified privately:
454 accepted and compared against the **executed actual TypeScript decoder**;
71 explicitly rejected. Also256 time constants, continuation/silence/repeats
and16 block9 channel/codec-omission witnesses were exercised. Existing CD assets
remain their authors' work, not newly relicensed by the project's MIT source
license; no original clip is copied into this production repository. The
private delivery records exact paths, hashes and exclusions. Synthetic test
inputs and PCM captures are identified separately from original files.

## Reproduce and remaining gates

`make test` runs the actual production C assertion harness through
`tests/test_voc_host.py`; `AUDIO_SANITIZE=1 make test-asan` additionally instruments
it with ASan/UBSan. `tests/host/voc_host.c` tests rejects, ownership, silence,
resampling, exact allocation boundaries, repeats, limits, split buffers and
saturation. Private signed evidence contains test-first RED/GREEN, corpus
oracles, ARM1176 instruction assertions, mutations and real-ring host composition.
Native and qemu-user results are not Pi clock/capacity/deadline measurements.
No hardware test or TV listening took place.

Still separate: independent review and Mac acceptance/integration; H1 sink,
firmware/config/clock and compatible driver provenance; safe DMA/cache/IRQ/USB;
checked ABI and source-reader ownership/EOF; measured producer starvation during
synchronous SD; live one-take local/remote scheduling; actual Zero W combined
CPU/tick-drop/underrun, memory, TV channel/frequency/stop/listening. CMF/OPL,
saves/demos, installation and publication are outside this slice.

Implementation, tests and documentation: **Hermes Agent / OpenAI GPT-6.1 Sol via
openai-codex**, directed by Lewis Moten. Existing PC-speaker provenance remains.
No third-party driver or emulator implementation is incorporated. QEMU is only
a separate GPL test tool; the test-only Linux EABI transport is from the prior
accepted MIT host-core qualification, never part of a Pi runtime.
