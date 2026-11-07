#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Capture the three real apps on a disposable card; no JS source injection.

Run from any directory: python3 tools/js_example_shots.py [--output DIR].
Build build/host/jsrun first. Dump/log/PNG endpoints are kept under DIR.
"""
import argparse
import pathlib
import struct
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
# Each endpoint starts a fresh app. Paired Linux key events: Enter=28, T=20,
# Right=106, Down=108, Space=57, Escape=1.
CASES = {
    'CLOCK': [
        ('night-start', 500, ''),
        ('night-2s', 2500, ''),
        ('paper-2s', 2500, '500:20:1,520:20:0'),
        ('night-restored-2s', 2500, '500:20:1,520:20:0,1000:20:1,1020:20:0'),
    ],
    'FILES': [
        ('root', 800, ''),
        ('folder', 800, '100:28:1,120:28:0'),
        ('text', 800, '100:28:1,120:28:0,300:28:1,320:28:0'),
        ('text-page2', 800, '100:28:1,120:28:0,300:28:1,320:28:0,500:106:1,520:106:0'),
        ('root-restored', 800, '100:28:1,120:28:0,300:28:1,320:28:0,500:1:1,520:1:0,650:1:1,670:1:0'),
    ],
    'SETTINGS': [
        ('defaults', 800, ''),
        ('changed', 800, '100:28:1,120:28:0,200:108:1,220:108:0,300:57:1,320:57:0'),
        ('reset', 800, '100:28:1,120:28:0,200:108:1,220:108:0,300:57:1,320:57:0,400:108:1,420:108:0,500:28:1,520:28:0'),
    ],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'build/example-shots')
    args = parser.parse_args()
    output = args.output.resolve()
    with tempfile.TemporaryDirectory() as temp:
        card = pathlib.Path(temp)
        (card / 'OS/APPS').mkdir(parents=True)
        for app in CASES:
            (card / 'OS/APPS' / app).symlink_to(ROOT / 'apps' / app)
        (card / 'DOCS').mkdir()
        (card / 'DOCS/NOTE.TXT').write_text('\n'.join(
            'Line %02d - Read-only TXT preview. Edit this file on your card.' % i
            for i in range(30)), encoding='ascii')
        (card / 'Z.BIN').write_bytes(b'Not a text document')
        for app, cases in CASES.items():
            target = output / app
            target.mkdir(parents=True, exist_ok=True)
            for name, ms, keys in cases:
                shot = target / (name + '.shot')
                command = [str(ROOT / 'build/host/jsrun'), str(card), '/sd/OS/APPS/' + app,
                           '--vclock', '--ms', str(ms), '--shot', str(shot)]
                if keys:
                    command += ['--keys', keys]
                result = subprocess.run(command, capture_output=True, text=True, timeout=120)
                log = result.stdout + result.stderr
                (target / (name + '.log')).write_text(log)
                if result.returncode or any(marker in log for marker in
                    ('Uncaught', '(it threw)', 'FAIL', 'unsupported')):
                    raise RuntimeError('App failed: ' + app + '/' + name + '\n' + log)
                data = shot.read_bytes()
                if (struct.unpack_from('<4I', data) != (0x534a4d47, 960, 540, 16)
                        or len(data) != 16 + 768 + 960 * 540 * 2):
                    raise RuntimeError('Invalid screenshot: ' + str(shot))
                subprocess.run([sys.executable, str(ROOT / 'tools/jsshot.py'),
                                str(shot), str(target / (name + '.png'))], check=True)
                print(app, name, 'ms=' + str(ms), 'keys=' + (keys or '(none)'))
        assert (output / 'CLOCK/night-2s.shot').read_bytes() == (
            output / 'CLOCK/night-restored-2s.shot').read_bytes()
        assert (output / 'FILES/root.shot').read_bytes() == (
            output / 'FILES/root-restored.shot').read_bytes()
    print('All captures valid; CLOCK and FILES restoration dumps match exactly.')


if __name__ == '__main__':
    main()
