# Exiting JS apps and launching another

## App API and default key

```js
// Finish local work, request exit, then return from your callback.
document.addEventListener('keydown', event => {
  if (event.code === 'KeyQ' && !event.repeat) {
    sys.exit(0);
    return;
  }
});
```

`sys.exit(status = 0)` requests a **cooperative** exit from the current app.
Status uses JavaScript's signed 32-bit integer conversion; the first successful
request wins. A failing conversion throws without requesting exit. It does not
unwind the current JS function: statements after the call still run until that
function returns. Do not call it and then enter an infinite loop. Further timer,
frame, event-listener and pending-promise callbacks are not dispatched once exit
is requested. No shutdown event or user cleanup callback is run.

**F10** (Linux key code 68, initial keydown/value 1) is reserved by the native
runtime. It requests status 0 before dispatch to JS or a focused `<game>`;
`preventDefault()`, focus and app key bindings cannot swallow it. Repeats and
release alone do not exit. Polling input normally takes at most 10 ms of idle
sleep, **plus the execution time of the current callback/native operation**.
Esc remains app/game navigation: FILES uses it to go up; a focused game uses it
to return to its containing JS page. Q is an example-app convention, not a
host-wide reserved key. CLOCK and SETTINGS use Back/Esc or Q to call the API;
FILES uses Back at card root (after leaving text/ascending folders), or Q.

## Where control goes

JS.ELF owns the app launcher. On app exit it logs `JS app returned status N`,
releases held native-game keys and sound, frees the entire QuickJS context and
runtime (DOM, canvases, listeners, timers, frames, promises and globals), drops
buffered input, restores page mode, and opens its **built-in app picker**. App
status is reported to that launcher, not treated as a request to reboot.
Device handles and the mapped framebuffer belong to JS.ELF and remain open
across app switches; normal kernel process teardown owns them at host exit.
Native fixed-size asset caches/play-core storage remain process-owned.

The picker lists DOS-style folders (1–8 uppercase letters, digits, `_` or `-`)
under `/OS/APPS` containing `MAIN.JS`. Up/Down selects, Enter launches, eight
entries are visible per page. Each launch has a fresh JS realm and app-relative
`performance.now()` clock. There is no saved JS state between launches. Missing
or throwing MAIN.JS falls back to the picker; failures are logged. If the picker
itself cannot start, JS.ELF returns failure 5. `sys.launch(name)` is an internal
picker operation; ordinary apps cannot use it to bypass the return lifecycle.
Back/Esc or F10 in the picker quits JS.ELF with status 0 through the **existing** startup
stub's return to the kernel-supplied link address. The kernel's entry-return
path handles status/cleanup; this does not add or call a new syscall. This is
distinct from exiting an individual app.

GMOS.ELF at this baseline plays native games but **does not launch JS.ELF**.
The existing SDK has no exec/spawn/nested-program call. Therefore JS.ELF cannot
truthfully return to a suspended GMOS games list. The small picker is necessary
to support app switching without inventing a new kernel ABI. GMOS is unchanged.
After quitting the picker, use the kernel console's existing `run GMOS.ELF`
when ready to start the installed games list. No ABI or SDK records changed.

To boot directly into the picker, use this `/OS/AUTORUN.TXT`:

```text
JS.ELF
PICKER
```

Existing second lines such as `APPS/CLOCK` still launch that app first, then
return to the picker on exit. A missing second line still defaults to HELLO.
The picker is embedded in JS.ELF and needs no separately installed app.

## Recovery limits

F10 is **not preemption**: it requires the runtime to regain its event loop.
An infinite top-level script, blocking callback, endless microtask chain or
blocked native operation can prevent both the shortcut and cooperative exit.
This change adds no interrupt hook, watchdog or forced JS exception mechanism.
Allocation failures and native faults are not promised recoverable in-process.
Normal shutdown does not fix the separately investigated attached-only DOM
wrapper/GC problem; picker rows keep explicit JS references.

The companion sandbox-kernel app-exit change supplies an independent USB STOP
recovery command. Consult that delivered revision's `docs/m1.md` and
`scripts/usb_kernel.py --help` for availability and safe-boundary limitations;
a baseline kernel must not be assumed to support it. This OS change requires
no unpublished stop interface. Stopping the entire OS program is not the same
as app exit: it discards JS.ELF and returns to kernel recovery. No cross-repo
hardware run is claimed by host tests here.

## Reproducible host checks

```sh
make test
make test-asan
# Targeted lifecycle tests only, after make host:
python3 -m unittest discover -s tests -p test_app_exit.py -v
# Mirror the board driver, including picker return/relaunch:
build/host/jsrun CARD /sd/OS/APPS/CLOCK --launcher --vclock --ms 2000 \
  --keys 100:16:1,110:16:0,150:28:1,160:28:0,200:68:1,210:68:0,300:68:1,310:68:0
# Start directly in the picker:
build/host/jsrun CARD --picker --launcher --vclock --ms 2000
```

`CARD/OS/APPS/CLOCK/MAIN.JS` must contain the real app; `CARD/GAMES/DICE`
should contain `tests/games/DICE` for focused-game checks. Without `--launcher`,
jsrun retains single-app test behavior and returns its exit status to the host.
The board always uses the launcher driver. Virtual-clock timings are not Pi
latency measurements. See the private delivery handoff for Linux linker and
sanitizer evidence. The Makefile now selects libc++ on macOS and libstdc++
on Linux without a linker adapter. App switching remains **hardware-untested**.

## Pi smoke procedure (not a hardware result)

1. Build `make all js TOOLCHAIN=/path/to/arm-none-eabi-`. Review/install the
   companion kernel separately if testing USB recovery. Preserve the current
   SD card and its AUTORUN before changing it; do not overwrite a running ELF.
2. While the kernel is ready, use its `usb_kernel.py PORT put build/JS.ELF`,
   `put build/GMOS.ELF`, then `putapp apps/CLOCK CLOCK`, `putapp apps/FILES FILES`
   and `putapp apps/SETTINGS SETTINGS`. Configure AUTORUN's two lines as above
   using the kernel's documented card/update procedure. Run `JS.ELF`.
3. In the picker select CLOCK and Enter. Require `CLOCK ready` in logs and
   advancing uptime. Press Q: require status 0 and picker, **without reboot**.
   Enter CLOCK again; press F10. Require the same return. Release keys fully.
4. Select FILES and Enter. Open a folder, use Back/Esc to go up, then Back
   at card root to return to the picker. Relaunch FILES; F10 exits. Select
   SETTINGS; change a setting then Back. CLOCK also returns with Back.
   Relaunch SETTINGS: defaults must be restored; F10 returns. Repeat this
   sequence ten times and inspect logs for errors or memory exhaustion.
5. With HELLO installed, launch a native game from its page. F10 must return
   all the way to the picker even while the game has focus. Esc should instead
   return to HELLO as before. Confirm the picker is 640x480x32, not game pixels.
6. Press F10 in the picker. Require JS.ELF exit and kernel recovery/readiness.
   Run installed GMOS.ELF through the console; require its games list. No claim
   is made that GMOS was suspended/resumed.
7. Independently run JS.ELF, issue the companion tool's `stop` action over USB,
   and require its documented ready/recovery result before another put/run.
   Run GMOS.ELF again. Record kernel/OS commit IDs, serial logs and screen
   evidence. Test a deliberately non-cooperative script only with a verified
   recovery route and the companion kernel's stated limitations understood.

Neither Pi hardware nor end-to-end USB/ARM execution is emulated by jsrun.
