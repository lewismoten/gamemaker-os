# SPDX-License-Identifier: MIT
"""Host tests for games packed in a .GMP (src/gmp.h): the Pocket Player's
"GMS1" bundle with the game's DOS files as dos/<NAME> entries (its length,
then the raw deflate), as web/src/export/pocketFile.ts writes it. A pack is
made here from tests/games/DICE the way that writer makes one; its files read
back byte for byte, it plays tick for tick and card for card like the folder,
split into volumes it reads the same, and broken packs are refused. With
GMOS_GMP=<folder of .GMP files> and GMOS_GAMES=<folder of game folders> it
also checks real packs from pocketData.ts --gmp against the loose games."""
import ctypes as C
import os, pathlib, struct, subprocess, unittest, zlib
from test_play import lib, Session, gmplay, OUT, DICE, DATA, TITLE_AT, INTRO_AT, LEVEL_AT, PLAY_AT, tick_ns

def stored(b):
    z = zlib.compressobj(9, zlib.DEFLATED, -15); return struct.pack('<I', len(b)) + z.compress(b) + z.flush()

def bundle(entries):
    """GMS1: entries sorted by name (byte order), 40 bytes each, then the data (pocketFile.ts writeBundle)."""
    items = sorted(entries.items(), key=lambda kv: kv[0].encode())
    start = 8 + 40 * len(items); head = b'GMS1' + struct.pack('<I', len(items)); table = b''; data = b''
    for name, b in items:
        table += name.encode().ljust(32, b'\0') + struct.pack('<II', start + len(data), len(b)); data += b
    return head + table + data

def dice_pack(pocket=True):
    e = {'dos/' + f.name.upper(): stored(f.read_bytes()) for f in DICE.iterdir()}
    if pocket:  # stand-ins for the Pocket Player's own entries, which the reader leaves alone
        e.update({'game.txt': b'GMP 1\ndice|Roll the Dice|1\n', 'dice.cfg': b'L 0\n', 'cards.pak': b'', '0a1b2c3.b': stored(b'x' * 99)})
    return bundle(e)

def gmp_lib():
    c = lib()
    if not getattr(c, '_gmp', False):
        c.gmos_gmp_open.argtypes = [C.c_char_p]; c.gmos_gmp_open.restype = C.c_void_p
        for f in ('gmos_gmp_gam',): getattr(c, f).argtypes = [C.c_void_p]; getattr(c, f).restype = C.c_char_p
        for f in ('gmos_gmp_pocket', 'gmos_gmp_volumes'): getattr(c, f).argtypes = [C.c_void_p]
        c.gmos_gmp_close.argtypes = [C.c_void_p]
        c.gmp_read.argtypes = [C.c_void_p, C.c_char_p, C.c_void_p, C.c_uint32]; c.gmp_read.restype = C.c_int32
        c._gmp = True
    return c

class Pack:
    def __init__(self, path):
        self.c = gmp_lib(); self.g = self.c.gmos_gmp_open(str(path).encode())
    def __enter__(self): return self
    def __exit__(self, *a): self.c.gmos_gmp_close(self.g)
    def read(self, name, cap=2 << 20):
        buf = (C.c_uint8 * cap)(); n = self.c.gmp_read(self.g, name.encode(), buf, cap)
        return None if n < 0 else bytes(buf[:n])

def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True); p = OUT / name; p.write_bytes(data); return p

class PackedSession(Session):
    """test_play's Session, its files read from a .GMP."""
    def __init__(self, path, now=0):
        self.pack = Pack(path); self.c = lib(); self.s = self.c.gmos_session_new(); self.log = []
        self.say = C.CFUNCTYPE(None, C.c_char_p)(lambda line: self.log.append(line.decode()))
        self.c.gmos_session_say(C.c_void_p(self.s), self.say)
        self.read = C.cast(self.c.gmp_read, C.c_void_p)
        self.result = self.c.gs_start(self.s, self.read, C.cast(C.c_void_p(self.pack.g), C.c_char_p), self.pack.c.gmos_gmp_gam(self.pack.g), now)

class Reading(unittest.TestCase):
    def test_every_file_byte_for_byte(self):
        with Pack(write('DICE.GMP', dice_pack())) as p:
            self.assertTrue(p.g); self.assertEqual(p.c.gmos_gmp_gam(p.g), b'DICE.GAM'); self.assertEqual(p.c.gmos_gmp_pocket(p.g), 1)
            for f in DICE.iterdir():
                self.assertEqual(p.read(f.name), f.read_bytes(), f.name)
                self.assertEqual(p.read(f.name.lower()), f.read_bytes(), f.name)       # DOS names are case-blind
            self.assertIsNone(p.read('NOPE.GIF')); self.assertIsNone(p.read('0a1b2c3.b'))  # only dos/ entries are the game's
            whole = (DICE / 'STARTER.FLI').read_bytes()
            self.assertEqual(p.read('STARTER.FLI', 64), whole[:64])                   # a read in part: the first cap bytes
    def test_dos_files_alone(self):
        with Pack(write('DICEOS.GMP', dice_pack(pocket=False))) as p:
            self.assertEqual(p.c.gmos_gmp_gam(p.g), b'DICE.GAM'); self.assertEqual(p.c.gmos_gmp_pocket(p.g), 0)
    def test_volumes(self):
        whole = dice_pack(); cuts = [0, 1000, 1001 + len(whole) // 3, len(whole)]   # one cut inside the index, one inside data
        names = ['VOLS.GMP', 'VOLS.G01', 'VOLS.G02']
        for name, a, b in zip(names, cuts, cuts[1:]): write(name, whole[a:b])
        with Pack(OUT / 'VOLS.GMP') as p:
            self.assertEqual(p.c.gmos_gmp_volumes(p.g), 3)
            for f in DICE.iterdir(): self.assertEqual(p.read(f.name), f.read_bytes(), f.name)
        (OUT / 'VOLS.G01').unlink()                                                  # a gap: the set ends at NAME.GMP, which is cut short
        with Pack(OUT / 'VOLS.GMP') as p: self.assertFalse(p.g)
    def test_broken_packs(self):
        good = dice_pack(); count = struct.unpack_from('<I', good, 4)[0]
        cases = {'empty': b'', 'magic': b'GMS2' + good[4:], 'cut': good[:100],                                          # inside the index
                 'count': good[:4] + struct.pack('<I', 5000) + good[8:], 'no-gam': bundle({'dos/DICE.PAL': stored(b'p' * 768)}),
                 'pocket-only': bundle({'game.txt': b'GMP 1\nx|X|1\n', 'x.cfg': b''})}
        for k, data in cases.items():
            with self.subTest(k), Pack(write(f'BAD{k[:5].upper()}.GMP', data)) as p: self.assertFalse(p.g)
        # an index that opens, with entries that lie: past the end, a wrong length, not deflate, too big
        gam = (DICE / 'DICE.GAM').read_bytes()
        lies = {'past-end': None, 'length': struct.pack('<I', len(gam) + 1) + stored(gam)[4:], 'not-deflate': struct.pack('<I', 10) + b'\xff' * 20,
                'too-big': struct.pack('<I', (1 << 20) + 1) + stored(b'')[4:], 'short': b'\1\0'}
        for k, entry in lies.items():
            e = {'dos/DICE.GAM': stored(gam), 'dos/ONE.MAP': entry if entry is not None else stored(b'm')}
            data = bytearray(bundle(e))
            if entry is None: struct.pack_into('<I', data, 8 + 40 + 36, 1 << 30)       # ONE.MAP's length runs past the end
            with self.subTest(k), Pack(write('LIE.GMP', bytes(data))) as p:
                self.assertTrue(p.g); self.assertEqual(p.read('DICE.GAM'), gam); self.assertIsNone(p.read('ONE.MAP'))
        self.assertGreater(count, 0)

class Fixture(unittest.TestCase):
    def test_fuzz_pack_is_this_writers(self):
        # tests/data/dice.gmp, which the fuzz pass (make test-asan) mutates, holds tests/games/DICE (another zlib
        # may deflate it to other bytes, so the files are compared; GMOS_WRITE_FIXTURE=1 writes it again)
        fixture = DATA / 'dice.gmp'
        if os.environ.get('GMOS_WRITE_FIXTURE'): fixture.write_bytes(dice_pack())
        with Pack(fixture) as p:
            for f in DICE.iterdir(): self.assertEqual(p.read(f.name), f.read_bytes(), f.name)

class Playing(unittest.TestCase):
    def test_states_tick_for_tick(self):
        # the Pocket Player host test's 1200 ticks, from the pack: the same lines as from the folder
        pack = write('DICE.GMP', dice_pack())
        self.assertEqual(gmplay(DATA / 'dice.keys', folder=pack, gam='-'), gmplay(DATA / 'dice.keys'))
    def test_session_cards_and_frames(self):
        # the title, the level's intro and play: the same pictures, palettes and frames as from the folder
        pack = write('DICE.GMP', dice_pack()); a = Session(); b = PackedSession(pack)
        self.assertEqual((a.result, b.result), (0, 0))
        for t in (TITLE_AT, INTRO_AT, LEVEL_AT, PLAY_AT, PLAY_AT + tick_ns(40)):
            a.step(t); b.step(t)
            self.assertEqual((a.phase(), a.ticks(), a.px(), a.pal()), (b.phase(), b.ticks(), b.px(), b.pal()), t)
        self.assertEqual(a.frame(), b.frame())

class RealPacks(unittest.TestCase):
    def test_pocket_data_packs_match_the_loose_games(self):
        packs, games = os.environ.get('GMOS_GMP'), os.environ.get('GMOS_GAMES')
        if not packs or not games: self.skipTest('set GMOS_GMP to a folder of .GMP files and GMOS_GAMES to the games\' folders')
        loose = {}
        for d in pathlib.Path(games).iterdir():
            for g in list(d.glob('*.[gG][aA][mM]')) + list(d.glob('disk/*.[gG][aA][mM]')):
                loose[zlib.crc32(g.read_bytes())] = g.parent
        checked = 0
        for path in sorted(pathlib.Path(packs).glob('*.GMP')):
            with Pack(path) as p:
                self.assertTrue(p.g, path.name)
                gam = p.read(p.c.gmos_gmp_gam(p.g).decode()); folder = loose.get(zlib.crc32(gam))
                if folder is None: continue
                for f in folder.iterdir():
                    if not f.name.startswith('.'): self.assertEqual(p.read(f.name), f.read_bytes(), f'{path.name} {f.name}')
                checked += 1
        print(f'{checked} packs read byte for byte like their folders'); self.assertGreater(checked, 0)

if __name__ == '__main__': unittest.main()
