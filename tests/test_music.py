# SPDX-License-Identifier: MIT
"""Pinned kernel keyboard decoder -> production input/menu/game music and audio.

Compile exact board helpers with native fake syscalls, like test_play_report.
No hardware, network, sanitizer, ARM syscall, or intentionally crashing program.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest
from test_play_report import block

ROOT = Path(__file__).resolve().parents[1]


class Music(unittest.TestCase):
    def test_keyboards_and_device_failures(self):
        source = (ROOT / 'src/main.c').read_text()
        sdk = (ROOT / 'sdk/include/sandbox/syscall.h').read_text()
        helpers = source[source.index('static struct au aud;'):
                         source.index('static int32_t au_query(')]
        for name in ['au_query', 'au_write', 'au_now', 'audio_disable', 'audio_poll', 'au_event', 'audio_log', 'audio_open',
                     'music_feedback', 'play_music_event', 'audio_take', 'bg_return', 'bg_leave']:
            signature = re.search(r'^static [^\n]*\b' + name +
                                  r'\([^;\n]*\) \{', source, re.M).group()
            helpers += '\n' + block(source, signature)
        # The sink is between au_now and audio_poll in the board source.
        sink = 'static const struct au_sink au_sink = {0, au_query, au_write, au_now};'
        helpers = helpers.replace('static void audio_poll(void)', sink + '\nstatic void audio_poll(void)', 1)
        fixture = (ROOT / 'tests/host/music_check.c.in').read_text()
        fixture = fixture.replace('@FORMAT@', block(source, 'static char *put(') + '\n' +
                                  block(source, 'static char *num('))
        fixture = fixture.replace('@QUERY@', block(sdk, 'static inline int32_t sbx_audio_query('))
        fixture = fixture.replace('@HELPERS@', helpers)
        fixture = fixture.replace('@MENU_MUSIC@', block(source, 'if (a == GM_MUSIC) {'))
        fixture = fixture.replace('@MENU_APPLY@', block(source, 'if (music_changed) {'))
        host = ROOT / os.environ.get('GMOS_HOST', 'build/host')
        with tempfile.TemporaryDirectory(prefix='music-') as tmp:
            path = Path(tmp)
            (path / 'music.c').write_text(fixture)
            subprocess.run(shlex.split(os.environ.get('HOSTCC', 'cc')) +
                           ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                            '-Isrc', '-Itests/host/input', '-Isdk/include',
                            '-Itests/host/keyboard', str(path / 'music.c'), 'src/input.c',
                            'tests/host/keyboard/hidboot.c', 'tests/host/keyboard/usbkbd.c',
                            str(host / 'libgmos.so'), '-o', str(path / 'music')], cwd=ROOT, check=True)
            result = subprocess.run([str(path / 'music')], cwd=ROOT, check=False,
                                    capture_output=True, text=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('two keyboards, reports, menu and game M PASS', result.stdout)
            self.assertIn('ABI, open, query, write, recovery and note PASS', result.stdout)
            self.assertIn('muted music advances; effects audible PASS', result.stdout)
