# Small JavaScript OS examples

These pages use the existing [DOM-like JavaScript API](../docs/js.md), not a web
browser. Each folder is copied to `/OS/APPS/<NAME>` and launched by JS.ELF.
The CLOCK, FILES and SETTINGS guides include launch commands, controls,
customization and Linux host screenshots.

| App | Teaches | Supported result |
| --- | --- | --- |
| [HELLOPI](HELLOPI/MAIN.JS) | First app: text, canvas animation, keyboard input, screen modes | Hello, Pi; arrows move, Enter switches mode, Esc exits |
| [HELLO](HELLO/) | Game grid, optional HTML layout, native game playback | The default launcher example |
| [CLOCK](CLOCK/README.md) | Elements, timers, keyboard themes | App uptime `hh:mm:ss`, Night/Paper; explicit civil-clock/date gap |
| [FILES](FILES/README.md) | Read-only card access, grid focus, error handling | Readable folders, `.TXT` preview/wrapping/paging, back/focus restoration; unavailable sizes clearly labeled |
| [SETTINGS](SETTINGS/README.md) | Buttons, toggles, state/render separation | Live theme/hints changes, Reset; memory-only persistence notice |

[HELLOPI](HELLOPI/MAIN.JS) is a single-file starting point for outside OS authors.
Copy the folder to `/OS/APPS/HELLOPI`, build `JS.ELF` with `make js`, and set
`AUTORUN.TXT` line 1 to `JS.ELF` and line 2 to `APPS/HELLOPI`. No game files are
required. It shows text and a small canvas: arrows move the yellow square,
the blue square sweeps automatically, Enter toggles 640x480x32 / 960x540x16,
and Esc returns to the app picker. Mode refusal is caught and shown in the UI.
Edit the colors, arrow step or animation speed in MAIN.JS to make it yours.
It uses only the documented API and keeps state in memory; fs is read-only.
See [Your first app](../docs/js.md#your-first-app) for USB and host commands.

The default [HELLO](HELLO/) game launcher remains unchanged.
[BENCH](BENCH/) contains runtime benchmarks.

## Reproduce the checks

```sh
make host
# If Linux lacks libc++, see CLOCK/README.md's build-only GCC adapter.
python3 -m unittest discover -s tests -p test_js.py -v
make test
python3 tools/js_example_shots.py
```

The capture script creates a disposable fixture card and writes real `.shot`,
`.log` and `.png` files under `build/example-shots/<APP>`. It converts through
`tools/jsshot.py` and checks exact dump dimensions/length, exception markers and
CLOCK/FILES restoration. It does not inject JavaScript into screenshot apps.
Numeric scheduled-key sequences are listed in each guide and in the script.

## Requirement-to-test results (Linux host)

| Requirement | Verified evidence |
| --- | --- |
| Three launchable apps and beginner READMEs | `apps/<APP>/MAIN.JS`, per-app guides and screenshot sequences |
| CLOCK advances and switches theme | `Clock`: 120 real frame callbacks, elapsed-time check, key/repeat cases, minute/hour/24h/100h boundaries via explicitly test-only clock injection; four captures |
| FILES card folders and arrow focus | `Files`: grid page crossing, empty folders, parent/root boundary and restored focus; root/folder captures |
| FILES text view | `Files`: TXT paging/back, CRLF/tab/non-ASCII conversion, 72-character wrapping, missing/other-file/long-path handling, large input; TXT/page-two captures |
| FILES file sizes | API gap, visibly `Size unavailable`; no fabricated metadata or string-length-as-size claim |
| SETTINGS buttons/toggles | `Settings`: native arrow focus, Enter/Space, ignored activation repeat, live labels/preview, Reset; three captures |
| SETTINGS save if API permits | API has no writes: memory-only state and visible notice, not an invented save |
| Exact headless smoke for each app | Test-only observer counts **120 callbacks per app**; does not mislabel elapsed milliseconds as frames |
| No app errors | New app probes scan stdout+stderr for `Uncaught`, `(it threw)`, `FAIL`, `unsupported` and check exit status |
| Existing behavior | PROBE and HELLO page/game/return tests pass; BENCH reaches `BENCH done 27 results` in a 60,000ms virtual-clock run |
| Screenshots | 12 real captures inspected; all four upstream CLOCK PNGs reproduced exactly; equal-time CLOCK restoration and FILES root/back dumps byte-identical |
| No C-runtime changes | Implementation limited to apps, Python smoke/capture tooling and documentation |

The earlier Linux qualification JS suite ran **15 tests, no failures**. Its full
`make test` ran **79 tests: 72 passed, 7 optional skips** (missing Arm toolchain or GMOS_CARD/GMOS_GAMES/
GMOS_SANDBOX fixtures). Linux builds used a recorded external HOSTCC adapter
substituting only `-lc++` with installed `-lstdc++`; plain `make host` failed
because libc++ is absent. BENCH virtual-clock zeros are not hardware performance
measurements. Board execution, unavailable optional fixtures and civil time are
not claimed as tested.

## Shared gaps

- No portable civil/real-time clock/date API. `performance.now()` is time since
  JS runtime startup, not independently measured OS boot age.
- Read-only fs: no persistence, write, delete, rename, stat, size or exported
  directory-type metadata. Readable-folder probing is not a stat substitute.
- Reads cap at 256 KiB without offset/truncation/byte-size metadata. FILES warns
  and additionally bounds its preview to 16,000 decoded characters.
- Native paths use a 96-byte buffer. FILES rejects non-ASCII or >95-byte paths,
  invalid components and separator-bearing names; display labels are ellipsized.
- Single-line ASCII bitmap UI, fixed pixel boxes, no native scroll/wrap or
  mouse/touch/form widgets. FILES wraps/pages in JS; SETTINGS uses buttons.
- Attached labels without explicit JS references can disappear under heap
  collection in this runtime spike. FILES/SETTINGS keep live references, release
  old page references and have regression tests. No C repair is included.
- No guarantee for arbitrarily large directories: `readdir` creates the entire
  list in the fixed JS heap. Physical board/keyboard/video validation remains open.

## Provenance

CLOCK retains its upstream signed commit `b73ed161859d0797a1c2e8a409874b10c8009025`.
Contributions in this suite were made with **OpenAI Codex (GPT-6.1 Sol)**.
