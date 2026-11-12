# SPDX-License-Identifier: MIT
"""Real native runtime input, lifecycle and launcher regressions."""
import pathlib
import shutil
import tempfile
import unittest
from test_js import ROOT, JSRUN, card, run
import subprocess


class Launcher(unittest.TestCase):
    def test_remote_back_exits_empty_picker(self):
        with tempfile.TemporaryDirectory() as tmp:
            (pathlib.Path(tmp) / 'OS' / 'APPS').mkdir(parents=True)
            p = subprocess.run([str(JSRUN), tmp, '--picker', '--launcher',
                '--vclock', '--ms', '1000', '--keys', '100:1:1,110:1:0'],
                capture_output=True, text=True, timeout=20)
            self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
            self.assertIn('JS app picker', p.stdout)


    def test_f10_is_host_owned_during_focused_game(self):
        with tempfile.TemporaryDirectory() as t:
            app = pathlib.Path(t)
            (app / 'MAIN.JS').write_text('''
const g = document.createElement('game');
document.body.appendChild(g);
g.src = '/sd/GAMES/DICE'; g.play();
setTimeout(() => console.log('game callback leaked'), 500);
''')
            sd = card({'GAME': app})
            try:
                p = subprocess.run([str(JSRUN), str(sd), '/sd/OS/APPS/GAME',
                    '--vclock', '--ms', '1000', '--launcher', '--keys',
                    '50:106:1,100:68:1,100:28:1,110:68:2,120:68:0,130:28:0,200:68:1,210:68:0'],
                    capture_output=True, text=True, timeout=20)
                out = p.stdout + p.stderr
                self.assertEqual(p.returncode, 0, out)
                self.assertEqual(out.count('JS game /sd/GAMES/DICE/DICE.GAM'), 1, out)
                self.assertIn('JS app picker GAME', out)
                self.assertNotIn('game callback leaked', out)
                self.assertNotIn('Uncaught', out)
            finally:
                shutil.rmtree(sd)

    def test_throwing_main_returns_to_picker_without_waiting_for_f10(self):
        with tempfile.TemporaryDirectory() as t:
            app = pathlib.Path(t)
            (app / 'MAIN.JS').write_text("throw new Error('deliberate startup failure');")
            sd = card({'BAD': app})
            try:
                p = subprocess.run([str(JSRUN), str(sd), '/sd/OS/APPS/BAD',
                    '--launcher', '--vclock', '--keys',
                    '100:28:1,110:28:0,200:68:1,210:68:0'],
                    capture_output=True, text=True, timeout=20)
                self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
                self.assertEqual(p.stdout.count('JS app picker BAD'), 2, p.stdout)
            finally:
                shutil.rmtree(sd)

    def test_missing_app_returns_to_picker(self):
        sd = card({'CLOCK': ROOT / 'apps/CLOCK'})
        try:
            p = subprocess.run([str(JSRUN), str(sd), '/sd/OS/APPS/MISSING',
                '--launcher', '--vclock', '--keys', '100:68:1,110:68:0'],
                capture_output=True, text=True, timeout=20)
            self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
            self.assertIn('JS app picker CLOCK', p.stdout)
        finally:
            shutil.rmtree(sd)

    def test_return_to_picker_then_launch_fresh_app(self):
        with tempfile.TemporaryDirectory() as t:
            app = pathlib.Path(t)
            (app / 'MAIN.JS').write_text('''
console.log('fresh=' + (globalThis.oldApp === undefined));
globalThis.oldApp = true;
const canvas = document.createElement('canvas');
canvas.width = 640; canvas.height = 480;
document.body.appendChild(canvas);
canvas.getContext('2d').fillRect(0, 0, 10, 10);
setInterval(() => console.log('stale timer'), 700);
document.addEventListener('keydown', () => console.log('stale key'));
setTimeout(() => sys.exit(7), 50);
''')
            sd = card({'AGAIN': app})
            try:
                keys = ','.join(f'{ms}:28:1,{ms + 10}:28:0' for ms in range(100, 5100, 100))
                keys += ',5200:68:1,5210:68:0'
                p = subprocess.run([str(JSRUN), str(sd), '/sd/OS/APPS/AGAIN',
                    '--vclock', '--ms', '6000', '--launcher', '--keys', keys],
                    capture_output=True, text=True, timeout=20)
                out = p.stdout + p.stderr
                self.assertEqual(p.returncode, 0, out)
                self.assertEqual(out.count('JS fresh=true'), 51, out)
                self.assertEqual(out.count('JS app returned status 7'), 51, out)
                self.assertNotIn('stale timer', out)
                self.assertNotIn('stale key', out)
                self.assertNotIn('Uncaught', out)
                self.assertIn('JS app picker', out)
            finally:
                shutil.rmtree(sd)


class ExampleExit(unittest.TestCase):
    def test_remote_settings_and_back_through_picker(self):
        sd = card({name: ROOT / 'apps' / name for name in ('CLOCK', 'FILES', 'SETTINGS')})
        try:
            # Settings: change theme and hints, reset, then Back. Clock and Files
            # also return on Back; finally Back in the picker exits the runtime.
            keys = ('100:108:1,110:108:0,150:108:1,160:108:0,200:28:1,210:28:0,'
                    '300:28:1,310:28:0,350:108:1,360:108:0,400:28:1,410:28:0,'
                    '450:108:1,460:108:0,500:28:1,510:28:0,550:1:1,560:1:0,'
                    '650:28:1,660:28:0,750:1:1,760:1:0,'
                    '850:108:1,860:108:0,900:28:1,910:28:0,1000:1:1,1010:1:0,'
                    '1100:1:1,1110:1:0')
            p = subprocess.run([str(JSRUN), str(sd), '--picker', '--launcher',
                '--vclock', '--ms', '2000', '--keys', keys],
                capture_output=True, text=True, timeout=20)
            out = p.stdout + p.stderr
            self.assertEqual(p.returncode, 0, out)
            for name in ('SETTINGS', 'CLOCK', 'FILES'):
                self.assertIn('JS ' + name + ' ready', out)
            self.assertEqual(out.count('JS app picker CLOCK,FILES,SETTINGS'), 4, out)
            self.assertEqual(out.count('JS app returned status 0'), 3, out)
            self.assertNotIn('Uncaught', out)
        finally:
            shutil.rmtree(sd)

    def test_switch_clock_files_settings_in_one_host_process(self):
        sd = card({name: ROOT / 'apps' / name for name in ('CLOCK', 'FILES', 'SETTINGS')})
        try:
            p = subprocess.run([str(JSRUN), str(sd), '--picker', '--launcher',
                '--vclock', '--ms', '2000', '--keys',
                '100:28:1,110:28:0,200:16:1,210:16:0,'
                '300:108:1,310:108:0,350:28:1,360:28:0,450:16:1,460:16:0,'
                '500:108:1,510:108:0,550:108:1,560:108:0,600:28:1,610:28:0,'
                '700:16:1,710:16:0,800:68:1,810:68:0'],
                capture_output=True, text=True, timeout=20)
            out = p.stdout + p.stderr
            self.assertEqual(p.returncode, 0, out)
            for name in ('CLOCK', 'FILES', 'SETTINGS'):
                self.assertIn('JS ' + name + ' ready', out)
            self.assertEqual(out.count('JS app returned status 0'), 3, out)
            self.assertEqual(out.count('JS app picker CLOCK,FILES,SETTINGS'), 4, out)
            self.assertNotIn('Uncaught', out)
            self.assertNotIn('(it threw)', out)
        finally:
            shutil.rmtree(sd)

    def test_q_calls_api_and_f10_returns_each_example_to_picker(self):
        for name in ('CLOCK', 'FILES', 'SETTINGS'):
            for key in (16, 68):
                with self.subTest(app=name, key=key):
                    sd = card({name: ROOT / 'apps' / name})
                    try:
                        p = subprocess.run([str(JSRUN), str(sd), '/sd/OS/APPS/' + name,
                            '--vclock', '--ms', '2000', '--launcher', '--keys',
                            f'100:{key}:1,110:{key}:0,150:28:1,160:28:0,200:68:1,210:68:0,300:68:1,310:68:0'],
                            capture_output=True, text=True, timeout=20)
                        out = p.stdout + p.stderr
                        self.assertEqual(p.returncode, 0, out)
                        self.assertEqual(out.count('JS app returned status 0'), 2, out)
                        self.assertIn('JS app picker ' + name, out)
                        self.assertNotIn('Uncaught', out)
                        self.assertNotIn('(it threw)', out)
                    finally:
                        shutil.rmtree(sd)


class AppExit(unittest.TestCase):
    def probe(self, source, keys='', ms=1000):
        with tempfile.TemporaryDirectory() as t:
            app = pathlib.Path(t)
            (app / 'MAIN.JS').write_text(source)
            sd = card({'EXIT': app})
            try:
                return run(sd, 'EXIT', ms, keys)
            finally:
                shutil.rmtree(sd)

    def test_api_exit_stops_other_callbacks_and_keeps_first_status(self):
        code, out = self.probe('''
setTimeout(() => { sys.exit(7); sys.exit(9); }, 10);
setTimeout(() => console.log('timer leaked'), 10);
requestAnimationFrame(() => {
  setTimeout(() => sys.exit(8), 30);
});
''')
        self.assertEqual(code, 7, out)
        self.assertNotIn('timer leaked', out)
        self.assertNotIn('Uncaught', out)

    def test_exit_from_key_stops_default_click_and_other_listeners(self):
        code, out = self.probe('''
const b = document.createElement('button');
document.body.appendChild(b); b.focus();
b.addEventListener('click', () => console.log('click leaked'));
document.addEventListener('keydown', () => sys.exit());
document.addEventListener('keydown', () => console.log('listener leaked'));
''', '100:28:1,100:30:1,110:28:0')
        self.assertEqual(code, 0, out)
        self.assertNotIn('leaked', out)

    def test_f10_cannot_be_cancelled_by_app(self):
        code, out = self.probe('''
console.log('alive');
document.addEventListener('keydown', e => { e.preventDefault(); console.log('key leaked'); });
setTimeout(() => console.log('too late'), 500);
''', '100:68:1,110:68:0')
        self.assertEqual(code, 0, out)
        self.assertIn('JS alive', out)
        self.assertNotIn('too late', out)
        self.assertNotIn('key leaked', out)
        self.assertNotIn('Uncaught', out)
