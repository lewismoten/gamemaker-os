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
        ref = reference('boot-960x540.png'); bad = sum(rgb(a) != b for a, b in zip(px[:W * 490], ref))  # above the loading bars, which move
        self.assertLess(bad, 150, f'{bad} pixels differ from the approved art')
    def test_pitch_is_respected(self):
        a, _ = render('ba_splash'); b, buf = render('ba_splash', stride=W + 37)
        self.assertEqual(a, b)
        self.assertTrue(all(buf[y * (W + 37) + x] == 0 for y in range(H) for x in range(W, W + 37)), 'wrote past the row')
    def test_diagnostics_matches_art_outside_the_frame(self):
        lines = (C.c_char_p * 3)(b'NO GAMES FOUND.', b'LOOKED IN /SD/GAMES AND /SD/POCKET.', b'INDEX: NO LIBRARY.JSL FOUND')
        px, _ = render('ba_diag', lines, 3); self.shot('library-diagnostics-960x540.png', px)
        ref = reference('diag-960x540.png')
        bad = sum(rgb(px[y * W + x]) != ref[y * W + x] for y in range(H - 50) for x in range(W) if not (112 <= x < 848 and 138 <= y < 453))
        self.assertLess(bad, 150, f'{bad} pixels differ from the approved art')
        self.assertTrue(any(px[y * W + x] != px[0] for y in range(150, 200) for x in range(128, 400)), 'no reasons printed')
    def test_bars_travel_and_stay_visible(self):
        c = lib(); c.ba_bar.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32]
        base, buf = render('ba_splash'); bg = rgb(base[0]); gifs = {}
        for segs, x0, w, pitch in ((4, 426, 24, 28), (8, 425, 12, 14)):
            frames = []; peaks = []
            for ms in range(0, 1600, 100):
                c.ba_bar(buf, W, ms, segs); px = [buf[y * W + x] for y in range(H) for x in range(W)]; frames.append(px)
                shades = [rgb(px[498 * W + x0 + i * pitch + w // 2]) for i in range(segs)]
                for y in range(497, 501):
                    self.assertEqual([rgb(px[y * W + x]) for x in (422, 423, 424, x0 - 1, 536, 537, 538)] if x0 == 426 else
                                     [rgb(px[y * W + x]) for x in (422, 423, 424, 536, 537, 538)], [bg] * (7 if x0 == 426 else 6))
                    for i in range(segs):  # each segment is one solid color across its whole box
                        self.assertEqual({rgb(px[y * W + x]) for x in range(x0 + i * pitch, x0 + i * pitch + w)}, {shades[i]})
                self.assertNotIn(bg, shades); self.assertNotIn(0x182C78, shades)  # always visible: never background or navy
                self.assertEqual(px[:497 * W], base[:497 * W]); peaks.append(max(range(segs), key=lambda i: (sum(shades[i].to_bytes(3, 'big')), -i)))
            while len(peaks) > 1 and peaks[-1] == 0: peaks.pop()  # the last frames are back around the loop
            self.assertEqual(peaks[0], 0); self.assertEqual(sorted(set(peaks)), list(range(segs)))  # the white peak visits every segment, in order
            self.assertEqual(peaks, sorted(peaks)); gifs[segs] = frames
        d = pathlib.Path(SHOTS or OUT / 'bootart'); d.mkdir(parents=True, exist_ok=True)
        from PIL import Image
        for segs, frames in gifs.items():
            ims = [Image.frombytes('RGB', (W, H), b''.join(bytes(((rgb(p) >> 16) & 255, (rgb(p) >> 8) & 255, rgb(p) & 255)) for p in f)) for f in frames]
            ims[0].save(d / f'boot-splash-loading-{segs}seg.gif', save_all=True, append_images=ims[1:], duration=100, loop=0)
            if segs == 8: ims[0].crop((380, 360, 580, 510)).save(d / 'boot-splash-loading-frame.png')
if __name__ == '__main__': unittest.main()
