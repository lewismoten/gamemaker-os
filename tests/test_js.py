# SPDX-License-Identifier: MIT; host tests of the JS runtime (src/js/) through
# build/host/jsrun on a virtual clock: the DOM-like API (tests/js/PROBE), the
# HELLO app's 960x540 page, game playback, repeatability, and HELLOPI canvas/input/exit.
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
    return p.returncode, p.stdout + p.stderr


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


class ExamplePage(unittest.TestCase):
    def probe(self, name, script, keys='', prepare=None, ms=2400):
        """Real app + test-only DOM observer; keys go through the host runner."""
        with tempfile.TemporaryDirectory() as t:
            root = pathlib.Path(t)
            app = root / 'OS/APPS' / name
            app.mkdir(parents=True)
            source = ROOT / 'apps' / name / 'MAIN.JS'
            self.assertTrue(source.is_file(), 'Missing example: ' + name)
            (app / 'MAIN.JS').write_text(source.read_text() + '\n' + script)
            if prepare: prepare(root)
            args = [str(JSRUN), str(root), '/sd/OS/APPS/' + name,
                    '--vclock', '--ms', str(ms)]
            if keys: args += ['--keys', keys]
            p = subprocess.run(args, capture_output=True, text=True, timeout=120)
            out = p.stdout + p.stderr
            self.assertEqual(p.returncode, 0, out)
            for marker in ('Uncaught', '(it threw)', 'FAIL', 'unsupported'):
                self.assertNotIn(marker, out)
            return out


class HelloPi(ExamplePage):
    def test_frames_input_modes_and_escape(self):
        out = self.probe('HELLOPI', """
let observedFrames = 0;
function observeHelloPi() {
  if (++observedFrames === 6) console.log('HELLOPI TEST frames=6');
  else requestAnimationFrame(observeHelloPi);
}
requestAnimationFrame(observeHelloPi);
setTimeout(() => {
  console.log('HELLOPI TEST position=' + x + ',' + y);
  console.log('HELLOPI TEST mode=' + status.textContent);
}, 180);
// A successful process status alone cannot distinguish exit from --ms expiry.
setTimeout(() => console.log('FAIL HelloPi kept running after Esc'), 400);
""", '20:106:1,25:106:0,40:108:1,45:108:0,'
     '60:105:1,65:105:0,80:103:1,85:103:0,100:106:1,105:106:0,'
     '120:28:1,125:28:2,130:28:0,200:1:1,210:1:0', ms=600)
        for expected in ['HELLOPI TEST frames=6', 'HELLOPI TEST position=150,65',
                         'HELLOPI TEST mode=Mode: 960x540, 16 bits', 'Hello, Pi exit']:
            self.assertIn('JS ' + expected, out)
        self.assertEqual(out.count('JS screen 960x540x16'), 1, out)
        # Enter a second time restores the initial mode.
        out = self.probe('HELLOPI', """
setTimeout(() => console.log('HELLOPI TEST mode=' + status.textContent), 180);
""", '20:28:1,25:28:0,100:28:1,105:28:0,200:1:1', ms=600)
        self.assertIn('JS HELLOPI TEST mode=Mode: 640x480, 32 bits', out)

    def test_renders_text_and_animated_canvas(self):
        # Capture the unmodified app, without game assets or optional HTML.
        with tempfile.TemporaryDirectory() as t:
            sd = pathlib.Path(t)
            (sd / 'OS/APPS').mkdir(parents=True)
            (sd / 'OS/APPS/HELLOPI').symlink_to(ROOT / 'apps/HELLOPI')
            shots = []
            for ms in (50, 150):
                shot = sd / ('%d.shot' % ms)
                code, out = run(sd, 'HELLOPI', ms, shot=shot)
                self.assertEqual(code, 0, out)
                self.assertIn('JS Hello, Pi ready', out)
                for marker in ('Uncaught', '(it threw)', 'FAIL', 'unsupported'):
                    self.assertNotIn(marker, out)
                w, h, depth, px = read_shot(shot)
                self.assertEqual((w, h, depth), (640, 480, 32))
                self.assertEqual(len(px), w * h * 4)
                pixels = struct.unpack('<%dI' % (w * h), px)
                self.assertIn(0xffffff, pixels)  # White title text.
                self.assertEqual(pixels.count(0x29adff), 100)  # Blue 10x10 square.
                self.assertGreaterEqual(pixels.count(0xffcc33), 400)  # Yellow square.
                shots.append(px)
            self.assertNotEqual(*shots)  # The blue square moved between frames.


class Files(ExamplePage):
    @staticmethod
    def fixtures(root):
        docs = root / 'DOCS'
        docs.mkdir()
        (docs / 'NOTE.TXT').write_text('\n'.join('Line %02d' % i for i in range(30)))
        (root / 'Z.BIN').write_bytes(b'not text')

    def test_folder_text_paging_and_back(self):
        out = self.probe('FILES', '''
function sample(label) {
  console.log('FILES TEST ' + label + ' path=' + document.getElementById('path').textContent);
  console.log('FILES TEST ' + label + ' focus=' + document.activeElement.id);
  const line = document.getElementById('line-0');
  if (line) console.log('FILES TEST ' + label + ' text=' + line.textContent);
}
setTimeout(() => sample('right'), 150);
setTimeout(() => sample('folder'), 350);
setTimeout(() => sample('text'), 450);
setTimeout(() => sample('page2'), 550);
setTimeout(() => sample('back'), 650);
setTimeout(() => { sample('root'); sys.exit(0); }, 750);
''', '100:106:1,110:106:0,200:105:1,210:105:0,300:28:1,310:28:0,'
     '400:28:1,410:28:0,500:106:1,510:106:0,600:1:1,610:1:0,700:14:1,710:14:0',
     self.fixtures)
        for expected in ['right focus=entry-1', 'folder path=/sd/DOCS',
                         'text path=/sd/DOCS/NOTE.TXT', 'text text=Line 00',
                         'page2 text=Line 12', 'back path=/sd/DOCS',
                         'back focus=entry-0', 'root path=/sd', 'root focus=entry-0']:
            self.assertIn('JS FILES TEST ' + expected, out)

    def test_grid_pages_empty_folder_and_root_boundary(self):
        def prepare(root):
            for i in range(8): (root / ('D%02d' % i)).mkdir()
        out = self.probe('FILES', '''
setTimeout(() => console.log('FILES TEST page=' + document.activeElement.id), 350);
setTimeout(() => console.log('FILES TEST empty=' + document.getElementById('status').textContent), 550);
setTimeout(() => console.log('FILES TEST restored=' + document.activeElement.id), 650);
setTimeout(() => {
  console.log('FILES TEST root=' + document.getElementById('status').textContent);
  sys.exit(0);
}, 750);
''', '100:108:1,110:108:0,200:108:1,210:108:0,300:105:1,310:105:0,'
     '400:106:1,410:106:0,500:28:1,510:28:0,600:1:1,610:1:0,700:14:1,710:14:0', prepare)
        for expected in ['page=entry-5', 'empty=Empty folder. Esc: parent.',
                         'restored=entry-6', 'root=Already at card root /sd.']:
            self.assertIn('JS FILES TEST ' + expected, out)

    def test_read_errors_other_files_and_long_paths(self):
        def prepare(root):
            (root / 'A.TXT').symlink_to(root / 'missing')
            (root / 'B.BIN').write_bytes(b'not text')
            (root / ('C' * 100 + '.TXT')).write_text('too long a path')
        out = self.probe('FILES', '''
function sampleStatus(label) {
  console.log('FILES TEST ' + label + '=' + document.getElementById('status').textContent);
}
setTimeout(() => sampleStatus('missing'), 150);
setTimeout(() => sampleStatus('binary'), 350);
setTimeout(() => { sampleStatus('long'); sys.exit(0); }, 550);
''', '100:28:1,110:28:0,200:106:1,210:106:0,300:28:1,310:28:0,'
     '400:106:1,410:106:0,500:28:1,510:28:0', prepare)
        for expected in ['missing=Cannot read TXT: missing, unreadable or read error.',
                         'binary=Not a readable folder. Only .TXT previews are supported.',
                         'long=Cannot open: path must be ASCII and at most 95 bytes.']:
            self.assertIn('JS FILES TEST ' + expected, out)

    def test_ascii_wrapping_and_large_read_are_bounded(self):
        def prepare(root):
            (root / 'A.TXT').write_text('A' * 81 + '\r\n\tCaf\u00e9\n' + 'Z' * 300000)
        out = self.probe('FILES', '''
setTimeout(() => {
  for (let i = 0; i < 4; i++) console.log('FILES TEST line' + i + '=' + document.getElementById('line-' + i).textContent);
  console.log('FILES TEST rows=' + document.querySelectorAll('text').length);
  console.log('FILES TEST notice=' + document.getElementById('status').textContent);
  sys.exit(0);
}, 300);
''', '100:28:1,110:28:0', prepare)
        self.assertIn('JS FILES TEST line0=' + 'A' * 72 + '\n', out)
        self.assertIn('JS FILES TEST line1=' + 'A' * 9 + '\n', out)
        self.assertIn('JS FILES TEST line2=    Caf?\n', out)
        self.assertIn('JS FILES TEST line3=' + 'Z' * 72 + '\n', out)
        self.assertIn('Read cap 256 KiB; preview first 16000 chars.', out)
        self.assertIn('JS FILES TEST rows=17', out)

    def test_120_frame_smoke_and_root_cards(self):
        out = self.probe('FILES', '''
let frames = 0;
function observe() {
  if (++frames === 120) {
    console.log('FILES TEST frames=' + frames);
    console.log('FILES TEST path=' + document.getElementById('path').textContent);
    console.log('FILES TEST focus=' + document.activeElement.id);
    console.log('FILES TEST size=' + document.getElementById('size-gap').textContent);
    sys.exit(0);
  } else requestAnimationFrame(observe);
}
requestAnimationFrame(observe);
''')
        self.assertIn('JS FILES TEST frames=120', out)
        self.assertIn('JS FILES TEST path=/sd', out)
        self.assertIn('JS FILES TEST focus=entry-0', out)
        self.assertIn('JS FILES TEST size=File sizes unavailable: no stat API.', out)


class Settings(ExamplePage):
    def test_keyboard_toggles_preview_and_reset(self):
        out = self.probe('SETTINGS', '''
function sample(label) {
  console.log('SETTINGS TEST ' + label + ' theme=' + document.getElementById('theme').textContent);
  console.log('SETTINGS TEST ' + label + ' hints=' + document.getElementById('hints').textContent);
  console.log('SETTINGS TEST ' + label + ' preview=' + document.getElementById('preview').textContent);
  console.log('SETTINGS TEST ' + label + ' hint=' + document.getElementById('hint').textContent);
}
setTimeout(() => sample('changed'), 450);
setTimeout(() => { sample('reset'); sys.exit(0); }, 750);
''', '100:28:1,105:28:2,110:28:0,200:108:1,210:108:0,300:57:1,305:57:2,310:57:0,'
     '500:108:1,510:108:0,600:28:1,610:28:0')
        for expected in ['changed theme=Theme: Paper', 'changed hints=Hints: Off',
                         'changed preview=Paper theme / hints off', 'changed hint=Hints are off.',
                         'reset theme=Theme: Night', 'reset hints=Hints: On',
                         'reset preview=Night theme / hints on',
                         'reset hint=Enter or Space changes a setting.']:
            self.assertIn('JS SETTINGS TEST ' + expected, out)

    def test_labels_survive_heap_collection(self):
        def prepare(root): (root / 'PRESSURE.TXT').write_text('Z' * 300000)
        out = self.probe('SETTINGS', '''
const pressure = fs.readFile('/sd/PRESSURE.TXT').slice(0, 16000);
setTimeout(() => {
  console.log('SETTINGS TEST nodes=' + document.body.children.length);
  console.log('SETTINGS TEST notice=' + document.getElementById('save-gap').textContent);
  sys.exit(0);
}, 300);
''', prepare=prepare)
        self.assertIn('JS SETTINGS TEST nodes=10', out)
        self.assertIn('restart resets settings.', out)

    def test_120_frame_smoke_and_memory_only_notice(self):
        out = self.probe('SETTINGS', '''
let frames = 0;
function observe() {
  if (++frames === 120) {
    console.log('SETTINGS TEST frames=' + frames);
    console.log('SETTINGS TEST theme=' + document.getElementById('theme').textContent);
    console.log('SETTINGS TEST hints=' + document.getElementById('hints').textContent);
    console.log('SETTINGS TEST gap=' + document.getElementById('save-gap').textContent);
    sys.exit(0);
  } else requestAnimationFrame(observe);
}
requestAnimationFrame(observe);
''')
        for expected in ['frames=120', 'theme=Theme: Night', 'hints=Hints: On',
                         'gap=Memory only: no file-write API; restart resets settings.']:
            self.assertIn('JS SETTINGS TEST ' + expected, out)


if __name__ == '__main__':
    unittest.main()
