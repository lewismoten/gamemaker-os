# SPDX-License-Identifier: MIT; host tests of the JS runtime (src/js/) through
# build/host/jsrun on a virtual clock: the DOM-like API (tests/js/PROBE), the
# HELLO app's 960x540 page, a game played from JS, and repeatability.
import os, pathlib, struct, subprocess, tempfile, unittest

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
JSRUN = pathlib.Path(os.environ.get('GMOS_HOST', ROOT / 'build/host')) / 'jsrun'


def card(apps):
    """A card folder: /OS/APPS/<name> for each app folder, /GAMES/DICE."""
    d = pathlib.Path(tempfile.mkdtemp())
    (d / 'OS/APPS').mkdir(parents=True)
    (d / 'GAMES').mkdir()
    for name, src in apps.items():
        (d / 'OS/APPS' / name).symlink_to(src)
    (d / 'GAMES/DICE').symlink_to(HERE / 'games/DICE')
    return d


def run(sd, app, ms, keys='', shot=None):
    args = [str(JSRUN), str(sd), '/sd/OS/APPS/' + app, '--vclock', '--ms', str(ms)]
    if keys: args += ['--keys', keys]
    if shot: args += ['--shot', str(shot)]
    p = subprocess.run(args, capture_output=True, text=True, timeout=120)
    return p.returncode, p.stdout


def read_shot(path):
    d = pathlib.Path(path).read_bytes()
    magic, w, h, depth = struct.unpack_from('<4I', d)
    return w, h, depth, d[16 + 768:]


class Probe(unittest.TestCase):
    def test_dom_api(self):
        sd = card({'PROBE': HERE / 'js/PROBE'})
        code, out = run(sd, 'PROBE', 2000, '100:106:1,110:106:0,200:28:1,210:28:0,300:106:1,310:106:0,400:105:1,410:105:0')
        self.assertEqual(code, 0, out)
        fails = [l for l in out.splitlines() if 'FAIL' in l or 'Uncaught' in l]
        self.assertEqual(fails, [], out)
        self.assertIn('JS DONE', out)
        self.assertGreaterEqual(out.count("JS PASS"), 19, out)


class Hello(unittest.TestCase):
    def test_page_at_960x540(self):
        sd = card({'HELLO': ROOT / 'apps/HELLO'})
        with tempfile.TemporaryDirectory() as t:
            shot = pathlib.Path(t) / 'a.shot'
            code, out = run(sd, 'HELLO', 1500, '500:28:1,520:28:0', shot)
            self.assertIn('JS hello OS ready', out)
            w, h, depth, px = read_shot(shot)
            self.assertEqual((w, h, depth), (960, 540, 16))
            self.assertGreater(len(set(px[i:i + 2] for i in range(0, len(px), 2))), 20)   # the title screen, not a flat page
            again = pathlib.Path(t) / 'b.shot'
            run(sd, 'HELLO', 1500, '500:28:1,520:28:0', again)
            self.assertEqual(shot.read_bytes(), again.read_bytes())   # the virtual clock makes it exact

    def test_game_from_js(self):
        sd = card({'HELLO': ROOT / 'apps/HELLO'})
        with tempfile.TemporaryDirectory() as t:
            shot = pathlib.Path(t) / 'g.shot'
            # Games, the first card (DICE), then Enter through its pages into level 1.
            code, out = run(sd, 'HELLO', 9000, '500:28:1,520:28:0,1000:28:1,1020:28:0,3000:28:1,3050:28:0,5000:28:1,5050:28:0', shot)
            self.assertIn('JS game /sd/GAMES/DICE/DICE.GAM', out)
            self.assertIn('GMOS level 1', out)
            w, h, depth, px = read_shot(shot)
            self.assertEqual((w, h, depth), (320, 200, 8))   # the game's own mode
            code, out = run(sd, 'HELLO', 9000, '500:28:1,520:28:0,1000:28:1,1020:28:0,6000:1:1,6020:1:0', shot)
            w, h, depth, px = read_shot(shot)
            self.assertEqual((w, h, depth), (960, 540, 16))   # Esc: back to the page's mode


if __name__ == '__main__':
    unittest.main()
