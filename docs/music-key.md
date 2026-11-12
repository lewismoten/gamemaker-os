# M key and TV sound

The M shortcut uses **Linux key code 50**, not ASCII `M` (77) or USB HID usage
`0x10` (16). Library music starts off; game music starts on independently.
Each toggle logs exactly `GMOS music on` or `GMOS music off`. Game music
muting leaves effects running and advances the song silently, so unmuting
continues at its current position. Already queued PCM can remain audible for
up to about 312 ms.

## Report to event

The trace below uses sandbox-kernel commit
`32fd0d26005b147d79da787d120976e79399eeea` from `sdk/PINNED`.

1. `drivers/usb_host.c` accepts keyboard interfaces operating in boot protocol,
   arms reports and routes them by device address and interface index.
2. `core/usbkbd.c` keeps each keyboard's previous report separately. It accepts
   ordinary eight-byte reports (modifier byte, reserved byte, six usage slots)
   and strips a report ID when the first report has nine bytes. The M usage
   can occupy any of the six slots. Skipped interfaces, short reports and
   exhausted keyboard slots have kernel USB logs.
3. `core/hidboot.c` maps page 7 usage `0x10` to Linux code **50**. Its letter
   table is correct. `usbkbd.c` publishes the union of held keys from all
   attached keyboards to the input queue. Holding M on both keyboards is one
   held key: the second press cannot create another toggle until both release
   it. Removing one keyboard releases only keys no remaining keyboard holds.
4. `/dev/input/event0` carries 16-byte ABI records with type `SBX_EV_KEY`,
   code 50, and values 1 (press), 0 (release), or 2 (repeat). `gi_read` checks
   byte counts, and `gi_accept` bounds launcher repeats. M releases and repeats
   do not toggle. SYN_DROPPED resets launcher state; device loss reopens input.
5. `gm_event` toggles in grid, details and Options. Search and folder entry
   intentionally own the letter M; high-score name entry also keeps letters.
   A demo stops on the key before the library handles it. The game loop calls
   `play_music_event` before card-skip, DOS menu, pause, save and gameplay input.
   Thus M cannot skip a title card or become a gameplay key while toggling.

No reports captured from the living-room keyboards were available. Host tests
model both an ID-prefixed receiver and an ordinary boot keyboard, including
concurrent reports and removal, through unchanged snapshots of the pinned
kernel decoder and multi-keyboard union. A receiver that sends a different
report format or never enumerates still needs its kernel USB trace.

## Toggle to PCM

`GM_MUSIC` updates the library setting even when audio is unavailable.
`music_feedback` logs every toggle and retries opening a closed device when
turning music on. An already-open device is queried even while idle, so the
toggle immediately exposes a failing handle. Library background selection starts with the embedded OS
CMF tune, then cycles through game tunes. A game takes the larger song buffer;
`gs_session` sound events request its current song via `au_event`/`au_music`.
These callbacks remain connected while the device is closed, so recovery can
load the latest request into the correct buffer. The library setting does not
mute games. A game slot with no song naturally remains silent.

The client gates audio on ABI minor >= 8 (startup rejects an incompatible
major), opens `/dev/audio` write-only, and validates the initial query's 48 kHz
rate, nonzero 32-bit capacity and queued <= capacity. Query output fields are
zeroed by the SDK before the ioctl. `au_service` loads the requested CMF;
`au_render` synthesizes signed 16-bit little-endian interleaved stereo.
`au_pump` queries live capacity, writes whole four-byte frames, preserves short
writes and retries `-11` (EAGAIN) later. Heartbeats, frame work and sliced
sleeps keep it fed.

Every fatal open/query/write failure records its operation and original kernel
status, for example:

```text
GMOS music on
GMOS audio open /dev/audio: error -19
```

`-19` is ENODEV, `-16` EBUSY, `-5` EIO, and `-38` ENOSYS (also used for the
explicit pre-1.8 gate). Invalid successful query/write results get a labelled
client validation error (`query format: error -22`, `write count: error -5`).
Fatal pump failures close the handle and drop queued PCM. A brief **No sound
device** overlay appears in both framebuffer formats, including idle game menus
and an empty library. Subsequent toggles on a closed device repeat the retained
error or retry the open; neither the setting nor its log depends on a live fd.

## What caused apparent no-ops

- Library Options swallowed M before the shortcut. It now accepts the shortcut;
  text fields retain their intended letter handling.
- Games had no music shortcut. Code 50 passed into ordinary DOS/game input
  (including title-card skipping). The new game handler reserves it.
- The library setting could change while `/dev/audio` was unavailable, with no
  toggle log or on-screen explanation. Fatal query/write failures silently
  disabled audio, closed the handle, removed the game callback and never
  retried. Open errors lost the kernel status, and initial query failures were
  entirely silent. Disabling music with a closed device also skipped the
  background-cycle reset, delaying a later restart. These paths now provide
  feedback and allow recovery.
- A TV sink can be missing even when every application syscall succeeds.
  In the pinned kernel, `core/audio_null.c` supplies a ring that drains and
  discards PCM if `drivers/hdmi_audio.c` fails initialization. `core/sys.c`'s
  audio query reports rate, queue and counters; flags remain zero for both
  HDMI and null. ABI 1.8 has **no sink-presence/error query**. GMOS cannot infer
  audible output from those counters or display a reliable missing-sink note
  for a null backend. No backend flag has been invented in the vendored SDK.

The physical TV failure cannot be identified from this checkout alone. On the
board, the kernel's `AUDIO` command must show `backend=hdmi`; if it reports null,
inspect the retained `AUDIO hdmi: unavailable ...` boot reason. The boot card
needs `hdmi_drive=2` for HDMI audio signaling. The pinned kernel documentation
also records corrected HDMI initialization ordering and packet enable logic;
verify the board's kernel build includes those corrections. Even a healthy
ring and DMA counter do not establish audible TV output. GMOS logs that sink
status requires kernel AUDIO diagnostics when opening the device.

## Regression coverage

`make all test` includes `tests/test_music.py`: pinned kernel report decoding,
two keyboards through the production `gi_read` path, grid/details/Options and
game handlers, text ownership, held/repeat/release behavior, ABI gating, absent
and busy devices, initial and runtime query failures, incompatible formats,
short writes, EAGAIN, fatal writes, invalid transferred counts, close/reopen,
toggle/error logs, note pixels in RGB565/XRGB8888 and note expiry. It also checks
that muted music advances and PC-speaker effects remain audible. The board
helpers are compiled directly from `src/main.c`; the fake audio query uses the
vendored SDK's exact record initialization.
