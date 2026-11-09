# SPDX-License-Identifier: MIT
"""The boot splash and the library diagnostics screen (src/bootart.c) on the host: rendered to PNGs
(build/host/bootart/, and the progress shots), checked against the approved art in art/ (the diagnostics'
framed area is empty in the art, where the OS prints its reasons), with a pitch wider than the picture. A few dozen pixels differ on purpose: the art's glyphs and the OS's own
5x7 font disagree by a pixel in a few letters."""
import ctypes as C, os, pathlib, unittest
from gmos_page import lib, png, q, rgb, W, H, ROOT, OUT
ART = ROOT / 'art'
SHOTS = os.environ.get('GMOS_SHOTS')
def reference(name):
    """The approved art quantized as the screen holds it, as 0x00RRGGBB."""
    from PIL import Image
    return [q(r << 16 | g << 8 | b) for r, g, b in Image.open(ART / name).convert('RGB').getdata()]
def render(fn, *args, stride=W):
    c = lib(); buf = (C.c_uint16 * (stride * H))(); getattr(c, fn)(buf, stride, *args)
    return [buf[y * stride + x] for y in range(H) for x in range(W)], buf
class BootArt(unittest.TestCase):
    def shot(self, name, px):
        d = OUT / 'bootart'; d.mkdir(exist_ok=True); png(d / name, px, W, H)
        if SHOTS: pathlib.Path(SHOTS).mkdir(parents=True, exist_ok=True); png(pathlib.Path(SHOTS) / name, px, W, H)
    def setUp(self):
        c = lib(); c.ba_splash.argtypes = [C.c_void_p, C.c_uint32]; c.ba_diag.argtypes = [C.c_void_p, C.c_uint32, C.POINTER(C.c_char_p), C.c_uint]
    def test_splash_matches_art(self):
        px, _ = render('ba_splash'); self.shot('boot-splash-960x540.png', px)
        ref = reference('boot-960x540.png'); bad = sum(rgb(a) != b for a, b in zip(px, ref))
        self.assertLess(bad, 150, f'{bad} pixels differ from the approved art')
    def test_pitch_is_respected(self):
        a, _ = render('ba_splash'); b, buf = render('ba_splash', stride=W + 37)
        self.assertEqual(a, b)
        self.assertTrue(all(buf[y * (W + 37) + x] == 0 for y in range(H) for x in range(W, W + 37)), 'wrote past the row')
    def test_diagnostics_matches_art_outside_the_frame(self):
        lines = (C.c_char_p * 3)(b'NO GAMES FOUND.', b'LOOKED IN /SD/GAMES AND /SD/POCKET.', b'INDEX: NO LIBRARY.JSL FOUND')
        px, _ = render('ba_diag', lines, 3); self.shot('library-diagnostics-960x540.png', px)
        ref = reference('diag-960x540.png')
        bad = sum(rgb(px[y * W + x]) != ref[y * W + x] for y in range(H) for x in range(W) if not (112 <= x < 848 and 138 <= y < 453))
        self.assertLess(bad, 150, f'{bad} pixels differ from the approved art')
        self.assertTrue(any(px[y * W + x] != px[0] for y in range(150, 200) for x in range(128, 400)), 'no reasons printed')
    def test_bars_rotate_in_order(self):
        c = lib(); c.ba_bar.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32]
        base, buf = render('ba_splash'); frames = []
        order = [q(0x182C78), q(0x7898F8), q(0xF8D450), q(0xF8FCF8)]
        for step in range(1, 9):
            c.ba_bar(buf, W, step); px = [buf[y * W + x] for y in range(H) for x in range(W)]
            seen = [rgb(px[498 * W + 426 + 28 * i + 10]) for i in range(4)]
            self.assertEqual(seen, [order[(i + step) % 4] for i in range(4)])  # each color moves one bar along
            self.assertEqual(px[:497 * W], base[:497 * W]); frames.append(px)  # nothing above the strip changes
        d = pathlib.Path(SHOTS or OUT / 'bootart'); d.mkdir(parents=True, exist_ok=True)
        from PIL import Image
        ims = [Image.frombytes('RGB', (W, H), b''.join(bytes(((rgb(p) >> 16) & 255, (rgb(p) >> 8) & 255, rgb(p) & 255)) for p in f)) for f in [base] + frames[:4]]
        ims[0].save(d / 'boot-splash-loading.gif', save_all=True, append_images=ims[1:], duration=120, loop=0)
if __name__ == '__main__': unittest.main()
