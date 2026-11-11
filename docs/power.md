# Idle power and playback

`GMOS.ELF` checks the kernel's startup ABI major/minor. On major 1,
minor 11 or newer, `src/awake.c` owns one call-22 hold with
`SBX_POWER_NO_BLANK | SBX_POWER_NO_SLOWDOWN`. It remains held throughout
active gameplay, demo/attract playback, FLI/FLC cartoons, palette fades and
finite timed GIF/BKD/text/slide pages. No input or periodic token renewal is
needed, including for a 2–3 hour movie. Normal heartbeats and absolute sleeps
continue. This has no power syscalls on older kernels.

The hold is released at the library, DOS menu, pause/save/load screens,
high scores/name entry, completed/failed playback and static pages waiting
indefinitely for a key. A cartoon configured to wait for a key afterward keeps
the hold until its animation finishes. A demo keeps running when the input
device disconnects. Optional library background music does not inhibit idle
saving. The independent experimental `JS.ELF` shell and its sample SETTINGS
app are separate from the native GMOS menus; this change targets `GMOS.ELF`.

GMOS has no settings screen. It leaves call 23 untouched: the kernel defaults
remain 60 seconds to slowdown and 10 minutes to HDMI blank/CEC Standby.
Releasing a hold does not reset inactivity, so an expired timeout can take
effect immediately upon returning to a menu. Acquiring requests restoration
if the clock or TV was already idle; hardware restoration is asynchronous.

Updates are idempotent. A failed acquire creates no owned token and can retry
on the next loop. A failed release retains the exact 64-bit token for retry;
EBADF discards a token already absent from the kernel. All returns from main
attempt release, including error returns. The kernel ultimately clears holds
on exit, fault, restart or USB STOP, even if a release fails. The vendored SDK
and `sdk/PINNED` come from sandbox-kernel's ABI 1.11 power-saving revision.

`make test` includes `awake_check`: the production module, actual SDK record
wrappers and real launcher/demo sessions through fake raw syscalls. It checks
both flags, ABI gating, repeated updates during three simulated hours,
cartoon/timed-page/key-wait boundaries, menus, pause/resume, save/load overlays,
failed starts, fatal-playback state, demo end/cancel, exit and syscall errors.
These host checks do not measure elapsed board time or TV behavior.

On a real Pi Zero W with the power-saving kernel, still verify:

- Leave demos, cartoons/slides and active play running without input for 2–3
  hours (crossing the kernel's 32-bit microsecond wrap). Confirm no clock
  slowdown, HDMI blank, CEC Standby or audio regressions; unplug input too.
- Return to library/DOS/pause/save/load/score menus and a key-wait page; confirm
  slowdown after 60 seconds and blank/standby after 10 minutes, including
  immediate idle after releasing a long-held token. Resume and check wake.
- Quit, fail a load, restart and USB STOP during playback; verify kernel holds
  are cleared. Repeat with an older kernel to confirm ordinary playback.
- Check the actual TV's CEC configuration and wake/standby behavior. Host tests
  and a successful ARM build cannot establish this hardware behavior.
