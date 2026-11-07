# SPDX-License-Identifier: MIT
"""Host tests for the SND log lines (src/sound.h): what a game's sounds say on
the USB log for a page at the other end to play, on the Starter "Roll the
Dice" (tests/games/DICE) and on a copy of it whose .SND names clips."""
import ctypes as C
import re, shutil, tempfile, pathlib, unittest
import test_play as P

def lib():
    c = P.lib()
    c.gmos_session_sound_dir.argtypes = [C.c_void_p, C.c_char_p]
    return c

class Sounded(P.Session):
    """A session that logs SND lines for the folder name `dir`; ticks[i] is the tick of log line i."""
    def __init__(self, folder=P.DICE, dir=b'DICE'):
        c = lib(); self.c = c; self.s = c.gmos_session_new(); self.log = []; self.folder = str(folder).encode()
        self.say = C.CFUNCTYPE(None, C.c_char_p)(lambda line: self.log.append(line.decode()))
        c.gmos_session_say(C.c_void_p(self.s), self.say)
        c.gmos_session_sound_dir(self.s, dir)
        self.read = C.cast(c.gmos_host_read, C.c_void_p)
        self.result = c.gs_start(self.s, self.read, self.folder, b'DICE.GAM', 0)
    def jump(self, ticks=200):
        """Plays `ticks` ticks walking right and jumping every 30: (tick, SND line) pairs."""
        t0 = self.to_play(); out = []; self.c.gp_key(77, 1)
        for k in range(1, ticks + 1):
            if k % 30 == 5: self.c.gp_key(57, 1)
            if k % 30 == 8: self.c.gp_key(57, 0)
            n = len(self.log); self.step(t0 + P.tick_ns(k))
            out += [(k, l) for l in self.log[n:] if l.startswith('SND')]
        self.t0 = t0; self.k = ticks
        return out
    def snd(self): return [l for l in self.log if l.startswith('SND')]

class SoundLines(unittest.TestCase):
    def test_game_title_song_then_level_song(self):
        s = Sounded(); s.to_play()
        self.assertEqual(s.snd(), ['SND game DICE DICE.GAM', 'SND music start DICE/DICE.CMF', 'SND music start DICE/DICE.CMF'])
        self.assertLess(s.log.index('SND music start DICE/DICE.CMF', 2), s.log.index('GMOS level 1') + 1)
    def test_effects_on_the_pc_speaker(self):
        got = Sounded().jump()
        self.assertTrue(got)
        for k, l in got: self.assertRegex(l, r'^SND beep DICE/DICE\.SND ([0-9]|[12][0-9])$')
    def test_one_line_a_tick_and_repeats_held_back(self):
        got = Sounded().jump(400)
        ticks = [k for k, _ in got]
        self.assertEqual(len(ticks), len(set(ticks)))
        for (a, la), (b, lb) in zip(got, got[1:]):
            if la == lb: self.assertGreaterEqual(b - a, 3)
    def test_clips_from_the_snd_table(self):
        with tempfile.TemporaryDirectory() as d:
            game = pathlib.Path(d) / 'DICE'; shutil.copytree(P.DICE, game)
            snd = bytearray((game / 'DICE.SND').read_bytes())
            n = int(re.search(r' (\d+)$', Sounded().jump()[0][1]).group(1))
            snd[2160 + 9 * n:2169 + 9 * n] = b'jump\0\0\0\0\0'
            (game / 'DICE.SND').write_bytes(snd)
            got = [l for _, l in Sounded(game).jump()]
            self.assertIn('SND voc DICE/JUMP.VOC', got)
            snd[2160 + 9 * n:2169 + 9 * n] = b' jump\0\0\0\0'  # a name starting with a space: the PC speaker
            (game / 'DICE.SND').write_bytes(snd)
            got = [l for _, l in Sounded(game).jump()]
            self.assertIn('SND beep DICE/DICE.SND %d' % n, got); self.assertFalse([l for l in got if 'voc' in l])
    def test_esc_stops_everything_once(self):
        s = Sounded(); s.jump(40)
        self.assertEqual(s.key(P.ESC, 1), P.GS_EXIT)
        s.step(s.t0 + P.tick_ns(s.k + 10)); s.key(P.ESC, 1)
        self.assertEqual(s.snd()[-2:], ['SND stopall', 'SND end'])
        self.assertEqual(s.snd().count('SND end'), 1)
    def test_no_folder_and_short_lines(self):
        s = Sounded(dir=b''); got = s.jump(100)
        self.assertEqual(s.snd()[0], 'SND game - DICE.GAM')
        for l in s.snd(): self.assertTrue(len(l) < 80 and l.isascii() and l.isprintable(), l)
        self.assertTrue(got[0][1].startswith('SND beep -/DICE.SND '))
    def test_no_lines_for_a_missing_game(self):
        s2 = P.Session(gam=b'NONE.GAM')
        self.assertEqual([l for l in s2.log if l.startswith('SND')], [])

if __name__ == '__main__': P.unittest.main()
