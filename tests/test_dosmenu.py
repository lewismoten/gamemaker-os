# SPDX-License-Identifier: MIT
"""DOS launcher integration: real DICE assets, synthetic 3.00 menu slots and patches.
No executable from the reference checkout is required by the host suite.
"""
import ctypes as C
import json
import struct
import os
import subprocess
import tempfile
from pathlib import Path
import unittest
from test_play import lib, Session, READ, DICE, retrace, tick_ns, GS_CARDS, GS_PLAY, GS_DONE

MENU, SCORES = 3, 4


class Launcher(Session):
    def __init__(self, patches=None, title=False, ending=None, missing=False, exe=None):
        self.c = lib()
        self.c.gs_launch.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_char_p, C.c_uint64]
        self.c.gs_menu_return.argtypes = [C.c_void_p, C.c_uint64]
        self.s = self.c.gmos_session_new()
        self.log = []
        self.say = C.CFUNCTYPE(None, C.c_char_p)(lambda line: self.log.append(line.decode()))
        self.c.gmos_session_say(C.c_void_p(self.s), self.say)
        self.files = {p.name.upper(): p.read_bytes() for p in DICE.iterdir() if p.is_file()}
        self.reader = READ(self.read_file)
        # Read the fixture through the production loader.
        assert self.c.gp_open(self.reader, None, b'DICE.GAM') > 0
        n = C.c_uint32()
        ptr = self.c.gp_gam_bytes(C.byref(n))
        gam = bytearray(C.string_at(ptr, n.value))
        scenes = struct.unpack_from('<H', gam, 256)[0]
        pool = 262 + 283 * scenes

        def slot(scene, index, name):
            off = 0
            if name:
                off = len(gam) - pool - 2
                gam.extend(name.encode() + b'\0')
                struct.pack_into('<H', gam, pool, len(gam) - pool - 2)
            struct.pack_into('<H', gam, 262 + 283 * scene + 43 + 2 * index, off)

        slot(0, 0, 'OPEN.LST' if title else '')
        slot(0, 2, 'MENU.GIF')
        for i, name in ((6, 'HELP.TXT'), (4, 'STORY.LST'), (7, 'CREDIT.TXT')):
            slot(0, i, '' if missing else name)
        # Remove the level intro/end pages to make game-end tests deterministic.
        for scene in range(2, scenes):
            slot(scene, 9, '')
            slot(scene, 10, '')
        slot(1, 0, '')
        slot(1, 2, '')
        gam[285:305] = b'TEST MENU'.ljust(20, b'\0')
        if ending == 'win':
            sx, sy = struct.unpack_from('<hh', gam, 262 + 83 + 6)
            struct.pack_into('<Hhhhh', gam, 262 + 2 * 283 + 83, 1, sx, sy, 0, 0)
        self.files['DICE.GAM'] = bytes(gam)
        self.files.update({'OPEN.LST': b'TITLE.GIF 65535\n', 'HELP.TXT': b'Instructions\n' * 22,
                           'STORY.LST': b'STORY.TXT 65535\nCREDIT.TXT 65535\n',
                           'STORY.TXT': b'Storyline\n', 'CREDIT.TXT': b'Credits\n'})
        if patches:
            self.files['PLAYER.JSON'] = json.dumps({'base': 'xferplay-3.00', 'patches': patches}).encode()
        if exe:
            self.files['DICE.EXE'] = exe
        self.result = self.c.gs_launch(self.s, self.reader, None, b'DICE.GAM', 0)
        self.t = 0

    def read_file(self, user, name, buf, cap):
        data = self.files.get(name.decode().upper())
        if data is None:
            return -1
        data = data[:cap]
        C.memmove(buf, data, len(data))
        return len(data)

    def advance(self, ns=1_000_000_000):
        self.t += ns
        return self.step(self.t)

    def menu(self):
        for _ in range(8):
            self.advance()
            if self.phase() == MENU:
                return
        raise AssertionError(self.log)

    def select(self, index):
        # Return selection is retained; tests set it explicitly with wraparound.
        for _ in range(index):
            self.key(108, 1)
        return self.key(28, 1)


class DosMenuTests(unittest.TestCase):
    def test_game_end_name_entry_and_return(self):
        lib()  # Build the same library used by the board's pure components.
        root = Path(__file__).resolve().parents[1]
        host = root / os.environ.get('GMOS_HOST', 'build/host')
        with tempfile.TemporaryDirectory() as tmp:
            exe = str(Path(tmp) / 'dosmenu-end')
            subprocess.run(['c++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror',
                            '-Isrc', '-Itests/host', '-isystem', 'third_party/gmcore',
                            'tests/host/dosmenu_end.cpp', str(host / 'libgmos.so'), '-o', exe],
                           cwd=root, check=True)
            subprocess.run([exe], cwd=root, check=True, timeout=20)

    def test_intro_menu_pages_and_quit(self):
        s = Launcher(title=True)
        self.assertEqual(s.result, 0)
        s.advance(); self.assertEqual(s.phase(), GS_CARDS)
        s.key(28, 1); s.menu()
        self.assertEqual(s.ticks(), 0)  # Enter on the title did not start play.
        s.key(103, 1)  # Up wraps Play -> Quit.
        self.assertEqual(s.key(28, 1), 2)
        self.assertEqual(s.phase(), GS_DONE)

        for row in (1, 2, 3):
            s = Launcher(); s.menu(); original = s.px()
            s.select(row); s.advance()
            self.assertEqual(s.phase(), GS_CARDS)
            self.assertNotEqual(s.px(), original)
            # Multi-page text and a two-slide .LST must both finish at the menu.
            for _ in range(5):
                s.key(57, 1); s.advance()
            self.assertEqual(s.phase(), MENU)
            s.key(1, 1); self.assertEqual(s.phase(), GS_DONE)

    def test_missing_pages_and_escape(self):
        s = Launcher(missing=True); s.menu()
        s.select(1); self.assertEqual(s.phase(), GS_CARDS)
        s.advance(11_000_000_000); self.assertEqual(s.phase(), MENU)
        s.key(28, 1); s.key(1, 1)  # Esc on a page returns to menu.
        self.assertEqual(s.phase(), MENU)

    def test_play_scores_menu_and_restart(self):
        s = Launcher(); s.menu(); s.select(0)
        s.advance(); s.advance(); self.assertEqual(s.phase(), GS_PLAY)
        s.advance(tick_ns(1)); self.assertGreater(s.ticks(), 0)
        self.assertEqual(s.key(1, 1), 0)
        self.assertEqual(s.phase(), SCORES)
        self.assertEqual(s.c.gs_menu_return(s.s, s.t), 1)
        s.menu(); s.key(28, 1); s.advance(); s.advance()
        self.assertEqual(s.phase(), GS_PLAY)
        self.assertLessEqual(s.ticks(), 4)
        self.assertEqual(s.c.gp_score(), 0)
        s.key(1, 1); s.c.gs_menu_return(s.s, s.t); s.menu()
        s.select(4); self.assertEqual(s.phase(), SCORES)
        s.c.gs_menu_return(s.s, s.t); self.assertEqual(s.phase(), MENU)

    def test_win_routes_to_scores(self):
        s = Launcher(ending='win'); s.menu(); s.select(0)
        for _ in range(10):
            s.advance()
            if s.phase() == SCORES:
                break
        self.assertEqual(s.phase(), SCORES, s.log)
        self.assertTrue(any('won' in line for line in s.log), s.log)
        s.c.gs_menu_return(s.s, s.t); s.menu()
        self.assertEqual(s.phase(), MENU)

    def test_custom_words_positions_colors_and_marker(self):
        s = Launcher(patches={'str.menu.play': 'GO', 'layout.menu.play.x': 8,
                              'layout.menu.play.y': 8, 'layout.menu.markerX': 2,
                              'layout.menu.firstY': 10, 'border.marker.width': 3,
                              'color.rgb.blue': [63, 0, 0]})
        s.menu(); px, pal = s.px(), s.pal()
        c = lib(); c.gp_match.argtypes = [C.c_char_p, C.c_int, C.c_int, C.c_int]
        c.gp_match.restype = C.c_uint8
        col = c.gp_match(pal, 63, 0, 0)
        c.gp_write.argtypes = [C.c_void_p, C.c_int32, C.c_int32, C.c_uint8, C.c_char_p, C.c_uint32]
        # The actual glyph pixels at the patched position must match gp_write.
        blank = C.create_string_buffer(64000)
        c.gp_write(blank, 8, 8, col, b'GO', 2)
        count = 0
        for y in range(8, 16):
            for x in range(8, 24):
                if blank.raw[y * 320 + x] == col and col:
                    self.assertEqual(px[y * 320 + x], col); count += 1
        self.assertGreater(count, 0)
        self.assertEqual(px[10 * 320 + 2:10 * 320 + 6], bytes([col]) * 4)
        self.assertNotEqual(px[10 * 320 + 6], col)

    def test_patched_transfer_player(self):
        # Synthetic executable with the published 3.00 patch map and opcode guards.
        exe = bytearray(0x257a5)
        exe[:2] = b'MZ'
        for off in (0xb6be, 0xb6bc, 0xb6cf, 0xb6e0, 0xb6f1, 0xb705, 0xb716, 0xb6ae, 0xb6ac):
            exe[off - 1] = 0x6a
        for off in (0xb6cc, 0xb6dd, 0xb6ee, 0xb702, 0xb713):
            exe[off - 1] = 0x68
        for off, word in ((0x25700, b'No instructions'), (0x2571f, b'No story'),
                          (0x2573b, b'No credits'), (0x2575b, b'GO'),
                          (0x25760, b'Help'), (0x25772, b'Story'), (0x25781, b'Cast'),
                          (0x2578d, b'Scores'), (0x257a0, b'Exit')):
            exe[off:off + len(word)] = word
        exe[0xb6be] = 8; exe[0xb6bc] = 8
        for off, rgb in ((0x16336, [63,63,0]), (0x16345, [63,10,10]),
                         (0x16354, [10,10,63]), (0x16363, [50,10,50])):
            for k, v in enumerate(rgb): exe[off + 5 * k] = v
        struct.pack_into('<H', exe, 0xaf84, 0x275)
        s = Launcher(exe=bytes(exe)); s.menu()
        # The synthetic EXE loaded the same visible Play patch as the JSON record.
        px, pal = s.px(), s.pal()
        c = lib(); c.gp_match.argtypes = [C.c_char_p, C.c_int, C.c_int, C.c_int]
        c.gp_match.restype = C.c_uint8
        col = c.gp_match(pal, 10, 10, 63)
        c.gp_write.argtypes = [C.c_void_p, C.c_int32, C.c_int32, C.c_uint8, C.c_char_p, C.c_uint32]
        glyphs = C.create_string_buffer(64000)
        c.gp_write(glyphs, 8, 8, col, b'GO', 2)
        self.assertNotEqual(col, 0)
        for y in range(8, 16):
            for x in range(8, 24):
                if glyphs.raw[y * 320 + x] == col:
                    self.assertEqual(px[y * 320 + x], col)

    def test_bad_patch_record_is_atomic(self):
        class Look(C.Structure):
            _fields_ = [('words', (C.c_char * 40) * 9), ('nums', C.c_int * 20),
                        ('rgb', (C.c_uint8 * 3) * 7), ('color', C.c_int), ('message', C.c_int)]
        c = lib(); c.dm_defaults.argtypes = [C.POINTER(Look)]
        c.dm_json.argtypes = [C.POINTER(Look), C.c_char_p, C.c_uint32]
        look = Look(); c.dm_defaults(C.byref(look)); original = bytes(look)
        for record in (b'{"base":"wrong","patches":{"str.menu.play":"GO"}}',
                       b'{"base":"xferplay-3.00","patches":{"str.menu.play":"GO"}', b'{}'):
            self.assertEqual(c.dm_json(C.byref(look), record, len(record)), -1)
            self.assertEqual(bytes(look), original)


if __name__ == '__main__':
    unittest.main()
