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


class Clock(unittest.TestCase):
    def probe(self, script, keys='', ms=2400):
        """Append test-only observers; run the real app on the real JS runtime."""
        with tempfile.TemporaryDirectory() as t:
            app = pathlib.Path(t) / 'CLOCK'
            app.mkdir()
            source = (ROOT / 'apps/CLOCK/MAIN.JS').read_text()
            (app / 'MAIN.JS').write_text(source + '\n' + script)
            sd = card({'CLOCK': app})
            try:
                code, out = run(sd, 'CLOCK', ms, keys)
                self.assertEqual(code, 0, out)
                for marker in ('Uncaught', '(it threw)', 'FAIL', 'unsupported'):
                    self.assertNotIn(marker, out)
                return out
            finally:
                import shutil
                shutil.rmtree(sd)

    def test_theme_keys(self):
        cases = [
            ('200:20:1,220:20:0', 'Paper'),  # T
            ('200:20:1,210:20:2,220:20:0', 'Paper'),  # ignore repeat
            ('200:20:0', 'Night'),  # release alone is not a press
            ('200:30:1,220:30:0', 'Night'),  # A is not a theme key
            ('200:20:1,220:20:0,400:20:1,420:20:0', 'Night'),
            ('200:28:1,220:28:0', 'Paper'),  # focused button: Enter
            ('200:57:1,220:57:0', 'Paper'),  # focused button: Space
        ]
        for keys, expected in cases:
            with self.subTest(keys=keys):
                out = self.probe('''
setTimeout(() => {
  console.log('CLOCK TEST ' + document.getElementById('theme').textContent);
  sys.exit(0);
}, 700);
''', keys)
                self.assertIn('JS CLOCK TEST Theme: ' + expected, out)

    def test_120_frame_smoke_and_time_advancement(self):
        out = self.probe('''
console.log('CLOCK TEST start=' + document.getElementById('uptime').textContent);
let observedFrames = 0;
function observeClock() {
  if (++observedFrames === 120) {
    console.log('CLOCK TEST frames=' + observedFrames);
    console.log('CLOCK TEST end=' + document.getElementById('uptime').textContent);
    console.log('CLOCK TEST date=' + document.getElementById('date-gap').textContent);
    sys.exit(0);
  } else requestAnimationFrame(observeClock);
}
requestAnimationFrame(observeClock);
''')
        self.assertIn('JS CLOCK TEST start=00:00:00', out)
        self.assertIn('JS CLOCK TEST frames=120', out)
        # Frame 1 is at t=0, so frame 120 precedes the two-second boundary.
        self.assertIn('JS CLOCK TEST end=00:00:01', out)
        self.assertIn('JS CLOCK TEST date=Date: unavailable (no civil-clock API)', out)

    def test_uptime_rollovers(self):
        # Test-only clock injection covers long uptimes without waiting days.
        for seconds, expected in [(59, '00:00:59'), (60, '00:01:00'),
                                  (3599, '00:59:59'), (3600, '01:00:00'),
                                  (86400, '24:00:00'), (360000, '100:00:00')]:
            with self.subTest(seconds=seconds):
                out = self.probe('''
performance.now = () => %d;
setTimeout(() => {
  console.log('CLOCK TEST time=' + document.getElementById('uptime').textContent);
  sys.exit(0);
}, 150);
''' % (seconds * 1000))
                self.assertIn('JS CLOCK TEST time=' + expected, out)

    def test_uptime_page(self):
        sd = card({'CLOCK': ROOT / 'apps/CLOCK'})
        try:
            with tempfile.TemporaryDirectory() as t:
                shot = pathlib.Path(t) / 'clock.shot'
                code, out = run(sd, 'CLOCK', 2200, shot=shot)
                self.assertEqual(code, 0, out)
                self.assertNotIn('Uncaught', out)
                self.assertNotIn('(it threw)', out)
                self.assertIn('JS CLOCK ready', out)
                w, h, depth, px = read_shot(shot)
                self.assertEqual((w, h, depth), (960, 540, 16))
                self.assertEqual(len(px), 960 * 540 * 2)
        finally:
            import shutil
            shutil.rmtree(sd)


if __name__ == '__main__':
    unittest.main()
