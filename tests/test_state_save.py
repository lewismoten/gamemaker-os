"""Save/load of a play in progress (gp_state_save / gp_state_load): a round trip restores the same state, and a damaged
or truncated save is refused by return code without changing the play. Runs under make test-asan too."""
import ctypes as C
import unittest
from test_play import Session, lib, tick_ns

INFO_AT = 762  # offset of character_info in struct save_game (4-byte aligned layout; checked by the round trip)
BUF = 128 * 1024  # a struct save_game is about 43 KiB


def play(s, a, b, keys=()):
    """Steps ticks a+1..b, the keys given as {tick: (code, value)}."""
    for n in range(a + 1, b + 1):
        if n in keys: s.key(*keys[n])
        s.step(tick_ns(n) + s.t0)


class StateSaveTests(unittest.TestCase):
    def setUp(self):
        self.c = lib()
        for f in ('gp_state_save', 'gp_state_load', 'save_game_encode', 'save_game_decode'): getattr(self.c, f).restype = C.c_int
        self.c.gp_state_save.argtypes = [C.c_void_p]; self.c.gp_state_load.argtypes = [C.c_void_p]
        self.c.save_game_encode.argtypes = [C.c_void_p, C.c_void_p, C.c_uint32, C.POINTER(C.c_uint32)]
        self.c.save_game_decode.argtypes = [C.c_void_p, C.c_void_p, C.c_uint32]
        self.s = Session(); self.s.t0 = self.s.to_play()

    def snap(self):
        g = C.create_string_buffer(BUF)
        self.assertEqual(self.c.gp_state_save(g), 0)
        return g

    def test_round_trip(self):
        keys = {5: (106, 1), 25: (106, 0), 30: (103, 1), 40: (103, 0)}  # right, then up
        self.s.t = self.s.t0; play(self.s, 0, 12, keys)
        g = self.snap(); self.c.gs_resume(C.c_void_p(self.s.s), C.c_uint64(0)); h = self.c.gp_frame_hash(); level = self.c.gp_level(); score = self.c.gp_score()
        wire = C.create_string_buffer(BUF); n = C.c_uint32()
        self.assertEqual(self.c.save_game_encode(g, wire, BUF, C.byref(n)), 0)
        play(self.s, 12, 45, keys)
        self.assertNotEqual(self.c.gp_frame_hash(), h)  # the play moved on
        back = C.create_string_buffer(BUF)
        self.assertEqual(self.c.save_game_decode(back, wire, n.value), 0)
        self.assertEqual(self.c.gp_state_load(back), 0)
        self.c.gs_resume(C.c_void_p(self.s.s), C.c_uint64(0))  # redraws the view, as main.c does after the screen
        self.assertEqual((self.c.gp_level(), self.c.gp_score()), (level, score))
        again = self.snap()
        # LoadGame forgets where the character was drawn (character_info 13..30: PLAYGAME draws it again from the next
        # frame), so those bytes and the first frame's hash differ by design; everything else is the saved state.
        lo, hi = INFO_AT + 13, INFO_AT + 31
        self.assertEqual(again.raw[:lo] + again.raw[hi:], g.raw[:lo] + g.raw[hi:])
        self.c.gs_resume(C.c_void_p(self.s.s), C.c_uint64(0))
        self.assertNotEqual(self.c.gp_frame_hash(), 0)

    def test_unsafe_sequence_keys_are_refused_before_state_changes(self):
        before = self.snap()
        wire = C.create_string_buffer(BUF); n = C.c_uint32()
        self.assertEqual(self.c.save_game_encode(before, wire, BUF, C.byref(n)), 0)
        good = wire.raw[:n.value]
        for seq in range(20):
            for scan in (128, 255):
                with self.subTest(sequence=seq, scan=scan):
                    bad = bytearray(good)
                    # Review probe: Drop active at frame -1, animation due now.
                    bad[896 + 3713] = 4
                    bad[896 + 3712] = 255
                    bad[761 + 41:761 + 45] = bytes(4)
                    bad[896 + seq * 185 + 171] = scan
                    decoded = C.create_string_buffer(BUF)
                    self.assertEqual(self.c.save_game_decode(decoded, bytes(bad), len(bad)), 0)
                    self.assertEqual(self.c.gp_state_load(decoded), -1)
                    self.assertEqual(self.snap().raw, before.raw)

    def test_sequence_key_boundary_loads_unchanged(self):
        g = self.snap()
        wire = C.create_string_buffer(BUF); n = C.c_uint32()
        self.assertEqual(self.c.save_game_encode(g, wire, BUF, C.byref(n)), 0)
        valid = bytearray(wire.raw[:n.value])
        for seq in range(20):
            valid[896 + seq * 185 + 171] = 127
        self.assertEqual(self.c.save_game_decode(g, bytes(valid), len(valid)), 0)
        self.assertEqual(self.c.gp_state_load(g), 0)
        again = C.create_string_buffer(BUF)
        self.assertEqual(self.c.save_game_encode(self.snap(), again, BUF, C.byref(n)), 0)
        # All sequence definitions, including scan 127, survive the load exactly.
        self.assertEqual(again.raw[896:896 + 3700], valid[896:896 + 3700])

    def test_damaged_and_truncated_are_refused(self):
        g = self.snap()
        wire = C.create_string_buffer(BUF); n = C.c_uint32()
        self.assertEqual(self.c.save_game_encode(g, wire, BUF, C.byref(n)), 0)
        good = wire.raw[:n.value]
        out = C.create_string_buffer(BUF)
        for cut in (0, 1, 100, len(good) // 2, len(good) - 1):  # truncated
            b = C.create_string_buffer(good[:cut], max(cut, 1))
            self.assertNotEqual(self.c.save_game_decode(out, b, cut), 0, cut)
        b = C.create_string_buffer(good + b'\0', len(good) + 1)  # a trailer
        self.assertNotEqual(self.c.save_game_decode(out, b, len(good) + 1), 0)
        # every kind of single-byte damage: decode may accept, but load must then refuse or leave a coherent play
        for at in range(0, len(good), 997):
            for val in (0x00, 0xFF, 0x7F):
                bad = bytearray(good); bad[at] = val
                b = C.create_string_buffer(bytes(bad), len(bad)); d = C.create_string_buffer(BUF)
                if self.c.save_game_decode(d, b, len(bad)) == 0:
                    r = self.c.gp_state_load(d)
                    self.assertIn(r, (0, -1, -2), (at, val))
                    if r == -2: self.assertEqual(self.c.gp_state_load(g), 0)  # put the saved play back
                self.assertEqual(self.c.gp_state_save(C.create_string_buffer(BUF)), 0)  # play still usable
        self.assertEqual(self.c.gp_state_load(g), 0)  # and the good state still loads


if __name__ == '__main__':
    unittest.main(verbosity=2)
