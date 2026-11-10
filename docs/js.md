# JS.ELF: a JavaScript layer for making your own OS

`JS.ELF` runs a small web-page-like app from the SD card on sandbox-kernel. The
app is JavaScript (QuickJS-ng, ES2023: `let`, arrow functions, classes, template
strings, `Map`, promises) with a few browser-like globals over native code. JS
only moves things around. The element tree, layout, drawing, files and the
Game-Maker play core are C. It was built as a spike on 2026-11-07. The numbers
and the recommendation are in sandbox-kernel's `docs/js-spike.md`.

For the whole picture (boot, what a program may do, limits, USB development) see sandbox-kernel's [OS author's guide](https://github.com/lewismoten/sandbox-kernel/blob/develop/docs/os-authors.md).

## Your first app

```
/OS/AUTORUN.TXT          line 1: the program the kernel starts (GMOS.ELF or JS.ELF)
                         line 2: the app JS.ELF runs (default APPS/HELLO)
/OS/APPS/HELLOPI/MAIN.JS the new example script
```

Start with [Hello, Pi](../apps/HELLOPI/MAIN.JS), a small, commented example for
outside OS authors. It creates text and a 300x150 canvas entirely in MAIN.JS;
no INDEX.HTM, game assets or filesystem access is needed. Arrows move the yellow
square, the blue square animates on its own, Enter switches between 640x480x32
and 960x540x16, and Esc calls `sys.exit(0)` to return to the app picker. State
lives in memory; the filesystem is read-only. See the [app guide](../apps/README.md).
To try it:

```
make js                                                     # build/JS.ELF
scripts/usb_kernel.py <port> put build/JS.ELF               # stripped: about 790 KB
scripts/usb_kernel.py <port> putapp apps/HELLOPI HELLOPI    # /OS/APPS/HELLOPI/
scripts/usb_kernel.py <port> run JS.ELF                     # F10 opens the picker; select HELLOPI
```

(`usb_kernel.py` is sandbox-kernel's. `putapp` and 1 MiB files need its
2026-11-07 kernel or later.) On the host, `make host` builds `build/host/jsrun`,
which runs an app from a folder that stands in for the card. `--vclock` makes the
clock virtual, `--keys ms:code:value,...` presses keys, and `--shot` saves the
screen; `tools/jsshot.py` turns that into a PNG.

```
build/host/jsrun <card folder> /sd/OS/APPS/HELLOPI --vclock --ms 2000 --keys 500:28:1,520:28:0 --shot a.shot
python3 tools/jsshot.py a.shot a.png 2      # 2x, nearest neighbor, as a 1080p screen shows 960x540
```

The default [HELLO example](../apps/HELLO/) has a header clock, Games and About
buttons, and a grid of game title screens. Enter on a game card plays it; Esc
returns to the page. It remains at `APPS/HELLO`. To autorun the new example,
put `JS.ELF` on AUTORUN.TXT line 1 and `APPS/HELLOPI` on line 2.

## Exit and app switching

Call `sys.exit(status = 0)` and return from the current callback, or press
**F10**. JS.ELF releases the app and returns to its built-in app picker;
Up/Down and Enter launch another app, F10 in the picker quits to the kernel.
CLOCK, FILES and SETTINGS also bind Q to `sys.exit(0)`. Esc remains navigation.
For the precise cooperative-exit semantics, ABI boundary, blocked-script
limitations and Pi smoke procedure, see [app-exit.md](app-exit.md).

## What a page can use

| Global | What it is |
| --- | --- |
| `document.createElement(tag)` | `div` (also `section`, `header`, `main`), `text` (also `span`, `p`, `h1`, `label`), `img`, `button`, `canvas`, `game` |
| `document.body`, `getElementById`, `querySelector`, `querySelectorAll` | one simple selector: `#id`, `.class` or a tag |
| `element.appendChild / insertBefore / removeChild / remove` | `parentNode`, `children`, `firstChild`, `lastChild`, `nextSibling`, `previousSibling` |
| `element.id / className / textContent / tabIndex` | buttons (and `tabIndex = 0`) can take focus |
| `element.innerHTML = '...'` | the same tags; `id`, `class`, `style` and other attributes; `&lt; &gt; &amp; &quot;` |
| `element.style.*` | the CSS subset below; a value outside it is logged and ignored |
| `element.addEventListener(type, fn)` | `keydown`, `keyup` (`key`, `code`, `repeat`), `click`, `focus`, `blur`, `ended`; events bubble to `document`; `preventDefault()` stops the default |
| `element.focus() / blur()`, `document.activeElement` | the arrow keys move focus to the nearest button that way; Enter or Space clicks |
| `element.getBoundingClientRect()` | the laid-out box |
| `img.src` | a `.GIF` on the card, or a game folder (its title screen) |
| `canvas.width`, `canvas.height` | backing pixels, default 300x150; keep small to fit the fixed heap |
| `canvas.getContext('2d')` | `fillStyle`, `font` (`9px` steps), `fillRect`, `clearRect`, `fillText`, `drawImage(img or canvas, x, y[, w, h])` |
| `game.src = '/sd/GAMES/DICE'; game.play()` | plays the folder's game with the native play core at 18.2 Hz; keys go to it while it has focus; Esc or the game's end fires `ended`; `game.stop()`, `game.ticks`, `game.playing`, `game.press(code, down)` |
| `requestAnimationFrame`, `cancelAnimationFrame` | 60 a second on the kernel clock, never bunched up after a slow frame |
| `setTimeout`, `setInterval`, `clearTimeout`, `clearInterval`, `performance.now()` | |
| `console.log` (and `info`, `warn`, `error`) | to the kernel's USB log, `JS ` in front |
| `fs.readFile(path)`, `fs.readdir(path)` | read-only, `/sd/...` |
| `screen.setMode(w, h, depth)` | the page's mode: 32 (XRGB8888) or 16 (RGB565) bits; 960x540x16 is exactly half of 1080p |
| `screen.gameLowres(on, w, h)` | a playing game switches to its own 8-bit mode with its palette: 320x200, or larger with the picture centered (384x216 is 1080p / 5) |
| `sys.exit(code = 0)` | request cooperative return to JS.ELF's app picker; F10 is the reserved native shortcut |
| `sys.stats()`, `sys.memory()` | timing and heap counters (the benchmarks use them) |

CSS subset (`src/js/dom.c` has the details): `left`, `top`, `width` and `height`
in px; `position: static | absolute`; `display: block | flex | none`;
`flexDirection: row | column`; `gap`; `padding`; `border` (`"2px solid #fff"`),
`borderWidth`, `borderColor`; `color`, `background`, `backgroundColor`
(`#rgb`, `#rrggbb`, `rgb(r,g,b)`, a few names, `transparent`); `outlineColor`
(the focus ring); `fontSize` (9, 18, 27 or 36 px of the 5x9 bitmap font);
`textAlign`.

Left out on purpose: style sheets and selectors with more than one part,
margins, percentages, `z-index` (later siblings paint over earlier ones),
wrapping flex lines, other fonts, transparency other than no background,
mouse and touch, `fetch`, writing files, modules (`import`). Each could be
added later if a real app needs it.

## How it works

- `src/js/dom.c`: the element tree. Changing a property marks the element. The
  next frame lays out what changed, repaints only the damaged rectangles
  (merged, at most 16), and presents only those. Moving focus between two cards
  repaints and presents those two cards, not the screen.
- `src/js/jsrt.c`: the QuickJS-ng bindings and the loop. Each turn reads input,
  runs due timers and frames, steps the game, then paints. A game element in
  low-res mode copies the play core's 320x200 picture straight to an 8-bit
  screen and sets the palette only when it changes (fades too).
- `src/js/prelude.js`: events, `document`, timers, `innerHTML` and `console`,
  in JS (compiled into the ELF by `tools/embed_js.py`).
- `src/js/plat.h`: what the runtime needs from the machine. `plat_sbx.c` is the
  board (the kernel's system calls) and `tests/host/jsrun.c` is the host.
- `src/js/sys.c`: newlib's libc and libm (from the Arm GNU toolchain) get one
  fixed 3 MiB heap for `malloc`. There are no threads and no files through stdio.
- `apps/BENCH`: the benchmarks.

`make test` includes `tests/test_js.py`. It checks the DOM-like API with
`tests/js/PROBE`, HELLOPI rendering, animation, input and Esc exit, the HELLO
page at 960x540, playing a game from JS and coming back to the page's mode,
and that two runs on the virtual clock give the same picture.
