# Boot art

Approved art from ChatGPT GPT-6.1 Sol, made from the maintainer's directions (MIT, `art/LICENSE.txt`); the layout notes are `art/README.txt`.

- **Splash** (`ba_splash`, `src/bootart.c`): 960x540 RGB565, drawn right after the OS sets that mode (the kernel starts a program in 640x480x32, so the 640x480 art is not used) and shown until the games list is ready. It keeps the surface's stride, not the picture width.
- **Library diagnostics** (`ba_diag`): shown instead of an empty list when no game is found. The OS prints its reasons in the framed area, 24 px a row, up to 12 rows.
- **Data:** `tools/make_bootart.py` turns `art/badge-96.png` into `src/bootart_data.c`: 13 RGB565 colors and 1565 bytes of run-length data (high nibble color, low nibble run - 1). No PNG decoder runs on the board. Text is the OS's 5x7 font at 6 x scale a letter. The badge is drawn at 2x on the splash and 80 px (nearest neighbor) on the diagnostics screen. Together it costs about 4 KB of the 8 MiB image quota.
- **Tests:** `tests/test_bootart.py` renders both screens to `build/host/bootart/` (set `GMOS_SHOTS` to copy them elsewhere) and compares them with `art/boot-960x540.png` and `art/diag-960x540.png`; a few dozen pixels differ where the art's glyphs and the OS font disagree by a pixel.
- **E-ink art is not wired.** The kernel's e-ink API is still being built. `art/eink/` keeps the 250x122 screens and the 96 and 40 px marks as PBMs (and PNGs) for later; `art/README.txt` says how to use them (PBM P4, MSB first, 1 = black).
- `art/favicon.ico` is kept for a web page; the repository has none yet.
