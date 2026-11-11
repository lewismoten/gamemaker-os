# SPDX-License-Identifier: MIT
"""Compile exact main.c report helpers and call sites against fake host I/O.

Mirrors run624's bounded source fixtures: exercises the actual successful-
present gate and five-second capture block, without an ARM syscall or board.
"""
import os
import pathlib
import shlex
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def block(source, start):
    at = source.index(start)
    brace = source.index('{', at)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[at:end]


class PlayReport(unittest.TestCase):
    def test_present_budget_and_snapshot_retention(self):
        source = (ROOT / 'src/main.c').read_text()
        fixture = (ROOT / 'tests/host/report_check.c.in').read_text()
        fixture = fixture.replace('@FORMAT@', block(source, 'static char *put(') + '\n' +
                                  block(source, 'static char *num(') + '\n' +
                                  block(source, 'static char *num64('))
        fixture = fixture.replace('@PERF@', source[source.index('enum {PF_TICK'):
                                                   source.index('/* Plays the chosen game')])
        fixture = fixture.replace('@PRESENT@', block(source, 'if (height) {'))
        fixture = fixture.replace('@SNAPSHOT@', block(source, 'if (clock_ns() - since >= 5000000000ull)'))
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp)
            (path / 'report.c').write_text(fixture)
            subprocess.run(shlex.split(os.environ.get('HOSTCC', 'cc')) +
                           ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                            str(path / 'report.c'), '-o', str(path / 'report')], check=True)
            p = subprocess.run([str(path / 'report')], check=True, capture_output=True, text=True)
            self.assertIn('no presentation, no report drain PASS', p.stdout)
            self.assertIn('unfinished snapshot retained PASS', p.stdout)
            self.assertIn('drop count and audio share frame budget PASS', p.stdout)
