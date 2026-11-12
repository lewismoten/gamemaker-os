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

Sleep and sound (kernel ABI 1.12 or newer): when the screen blanks, the library's
music and menu sounds stop with it. `awake_screen_dark()` asks the kernel
(call 22, operation 2) once a pass of the library loop; while it says the
screen is blanked, `au_sleep` makes the audio pump do nothing, so the synth,
the song's file reads and the sound writes cost no CPU, and the kernel drops
anything written anyway. The song, speaker and clip stand still where they
were. On any key, CEC key or USB input the screen wakes, and the same song
continues from that spot, fading in from silence over about 0.7 seconds (it
does not restart: the library's cycle of songs carries on rather than starting
over, and no catch-up for the time asleep is rendered). Slowdown alone (60
seconds) does not touch the sound. Older kernels are never asked.

Native library Options (Back from cards, or Up from tabs) includes:

- **Screen off after: 5 / 10 / 30 min / Never**. OK cycles the choices.
  Call 23 receives 300, 600, 1800 or 0 blank seconds respectively; zero
  disables HDMI blank and CEC Standby.
- **Power saving: On / Off**. OK toggles clock slowdown after 60 seconds
  or disables slowdown (0 seconds). This is independent of Screen off.

Arrows select either control; OK changes it without closing Options; Back
returns to the library. Both controls work with an empty library too. Defaults
remain 10 minutes and Power saving On.
Changes apply immediately and are saved with the other native GMOS data in
`/sd/OS/SAVES/GMOS/SETTINGS.DAT`, using the same temporary-file, fsync and
rename path as Favorites and Recent. Music remains a session-only setting.
The versioned settings file is validated as a whole; missing files use defaults,
and unreadable or invalid files use defaults with a log message. A failed save
shows “Changes not saved”; a failed call 23 reports “Power timeouts not applied”.

At boot, GMOS loads the saved settings and applies them through ABI 1.11
call 23 before entering the library event loop. Older kernels quietly skip
call 23 (saving itself needs ABI 1.7). The separate JS SETTINGS sample remains
memory-only; native GMOS does not launch it.

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
`settings_check` additionally drives native Options with only remote keys,
round-trips saved settings, checks all eight slowdown/blank combinations and
Never = 0 through actual SDK call-23 records, reloads Never/Off at boot,
checks older/incompatible ABI gating, and exercises atomic-save failure and
invalid-file defaults. These host checks do not measure elapsed board time or
TV behavior.

On a real Pi Zero W with the power-saving kernel, still verify:

- Leave demos, cartoons/slides and active play running without input for 2–3
  hours (crossing the kernel's 32-bit microsecond wrap). Confirm no clock
  slowdown, HDMI blank, CEC Standby or audio regressions; unplug input too.
- Return to library/DOS/pause/save/load/score menus and a key-wait page; confirm
  slowdown after 60 seconds and blank/standby after 10 minutes, including
  immediate idle after releasing a long-held token. Resume and check wake.
- Quit, fail a load, restart and USB STOP during playback; verify kernel holds
  are cleared. Repeat with an older kernel to confirm ordinary playback.
- Change both power controls, restart GMOS, and confirm persistence and the
  selected idle delays; verify Never prevents blank/standby and Off prevents
  slowdown independently.
- Check the actual TV's CEC configuration and wake/standby behavior. Host tests
  and a successful ARM build cannot establish this hardware behavior.
