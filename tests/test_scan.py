# SPDX-License-Identifier: MIT
"""Exercise the board scanner itself through the host profiling transport.

Scan exercises the legacy fallback; PersistentScan exercises actual cache
read/rebuild/replay through a POSIX shim. Neither establishes FAT order or
physical SD durability; the real FAT/kernel harnesses cover separate layers.
"""
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Scan(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.work = pathlib.Path(cls.temp.name)
        empty = cls.work/'empty'
        (empty/'GAMES').mkdir(parents=True)
        env = dict(os.environ)
        if 'asan' in os.environ.get('GMOS_HOST', ''):
            env['GMOS_SCAN_CFLAGS'] = '-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined'
        subprocess.run([sys.executable, str(ROOT/'tools/profile_scan.py'), str(ROOT),
                        str(empty), str(cls.work/'probe'), '1'], env=env,
                       check=True, stdout=subprocess.DEVNULL)
        cls.probe = cls.work/'probe/scan-probe'

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.card = pathlib.Path(tempfile.mkdtemp(dir=self.work))
        (self.card/'GAMES').mkdir()

    def dice(self, name='DICE'):
        target = self.card/'GAMES'/name
        shutil.copytree(ROOT/'tests/games/DICE', target)
        return target

    def scan(self):
        return json.loads(subprocess.check_output([str(self.probe), str(self.card)]))

    def index(self, text):
        (self.card/'GAMES/LIBRARY.JSL').write_text(text)

    def test_long_nested_folder_zip_sfx_and_noise_are_discovered(self):
        import zipfile
        import io
        self.dice('Long folder name with spaces')
        self.assertEqual(self.scan()['games'], 1)
        shutil.rmtree(self.card/'GAMES/Long folder name with spaces')
        (self.card/'GAMES/Collection').mkdir()
        self.dice('Collection/Long nested folder')
        self.assertEqual(self.scan()['games'], 1)
        shutil.rmtree(self.card/'GAMES/Collection')
        for suffix, stub in [('ZIP', b''), ('EXE', b'MZ'+bytes(1022))]:
            b=io.BytesIO()
            with zipfile.ZipFile(b,'w',zipfile.ZIP_DEFLATED) as z:
                for p in (ROOT/'tests/games/DICE').iterdir(): z.writestr('Wrapper folder/'+p.name,p.read_bytes())
            path=self.card/'GAMES'/('Long archive name.'+suffix)
            path.write_bytes(stub+b.getvalue())
            self.assertEqual(self.scan()['games'], 1, suffix)
            path.unlink()
        for noise in ('._DICE', '.Trashes', '.Spotlight-V100', '.fseventsd', 'System Volume Information', '__MACOSX'):
            self.dice(noise)
        self.assertEqual(self.scan()['games'], 0)

    def test_gam_titles_and_extensionless_long_name_fallback(self):
        import io, zipfile
        original = bytearray((ROOT/'tests/games/DICE/DICE.GAM').read_bytes())
        offset, width = (285, 20) if original[:2] == b'GM' else (6, 15)
        def gam(title):
            data = bytearray(original)
            data[offset:offset+width] = title.ljust(width, b'\0')
            return data
        def archive(path, payload, member='Metadata.gam', stub=b''):
            b = io.BytesIO()
            with zipfile.ZipFile(b, 'w', zipfile.ZIP_DEFLATED) as z:
                z.writestr(member, payload)
            path.write_bytes(stub+b.getvalue())
        for filename, title, expected, stub in (
            ('Misleading archive.zip', b'Actual GAM Title', 'Actual GAM Title', b''),
            ('BREAKOUT.ZIP', b'', 'BREAKOUT', b''),
            ('Breakout The Rescue.zip', b'', 'Breakout The Rescue', b''),
            ('Another.long.name.ExE', b'', 'Another.long.name', b'MZ'+bytes(1022))):
            with self.subTest(filename=filename):
                path = self.card/'GAMES'/filename
                archive(path, gam(title), stub=stub)
                cold, warm = self.scan(), self.scan()
                self.assertEqual(cold['games'], 1)
                self.assertEqual(cold['list'][0]['title'], expected)
                self.assertEqual(warm['list'], cold['list'])
                self.assertEqual(warm['list'][0]['source_path'], '/sd/GAMES/'+filename)
                path.unlink()
        for folder_name, title, expected in (
            ('Collection/Long Raw Folder.game', b'', 'Long Raw Folder'),
            ('Another long raw folder', b'Real Raw Title', 'Real Raw Title')):
            folder = self.card/'GAMES'/folder_name
            folder.mkdir(parents=True)
            (folder/'Metadata.gam').write_bytes(gam(title))
            cold, warm = self.scan(), self.scan()
            self.assertEqual(cold['games'], 1)
            self.assertEqual(cold['list'][0]['title'], expected)
            self.assertEqual(warm['list'], cold['list'])
            shutil.rmtree(folder)
        path = self.card/'GAMES/Broken.zip'
        for payload, member in ((b'bad', 'Metadata.gam'), (gam(b''), 'NoGame.txt')):
            archive(path, payload, member)
            self.assertEqual(self.scan()['games'], 0)
            self.assertEqual(self.scan()['games'], 0)
        archive(path, gam(b'Recovered'))
        self.assertEqual(self.scan()['list'][0]['title'], 'Recovered')

    def test_depth_two_game_is_found_beneath_an_existing_game(self):
        self.dice('PARENT')
        nested=self.dice('PARENT/NESTED')
        p=nested/'DICE.GAM';data=bytearray(p.read_bytes());data[-1]^=53;p.write_bytes(data)
        d=self.scan()
        self.assertEqual(d['games'],2)
        self.assertEqual({g['source_path'] for g in d['list']},{'/sd/GAMES/PARENT','/sd/GAMES/PARENT/NESTED'})

    def test_per_entry_logs_are_bounded_by_default(self):
        self.dice()
        for i in range(140):
            (self.card/'GAMES'/f'not-a-dos-folder-{i}').mkdir()
        for i in range(20): self.dice(f'DUP{i:05}')
        d = self.scan()
        self.assertEqual(d['games'], 1)
        self.assertEqual(d['crc_calls'], 21)  # duplicates still CRC checked
        self.assertLessEqual(d['logs'], 3)
        self.assertLessEqual(d['log_bytes'], 256)
        self.assertGreaterEqual(d['healths'], 161)

    def test_missing_corrupt_and_stale_metadata_still_discover(self):
        self.dice()
        expected = self.scan()['list']
        for text in ('{bad JSON\n', '', '{"schema":"gmp-library","title":"Absent","locator":"MISSING"}\n'):
            self.index(text)
            self.assertEqual(self.scan()['list'], expected)

    def test_index_metadata_is_fresh_and_later_records_win(self):
        self.dice()
        record = lambda t: json.dumps(dict(schema='gmp-library', title=t, locator='DICE', category=['sandbox']))+'\n'
        self.index(record('First')+record('Second'))
        d = self.scan()
        self.assertEqual((d['list'][0]['title'], d['list'][0]['cat']), ('Second', 1))
        self.index(record('Updated'))
        self.assertEqual(self.scan()['list'][0]['title'], 'Updated')

    def test_add_remove_and_new_game_inside_previously_empty_folder(self):
        (self.card/'GAMES/EMPTY').mkdir()
        self.assertEqual(self.scan()['games'], 0)
        shutil.copyfile(ROOT/'tests/games/DICE/DICE.GAM', self.card/'GAMES/EMPTY/DICE.GAM')
        self.assertEqual(self.scan()['list'][0]['dir'], 'EMPTY')
        (self.card/'GAMES/EMPTY/DICE.GAM').unlink()
        self.assertEqual(self.scan()['games'], 0)
        self.dice()
        self.assertEqual(self.scan()['games'], 1)
        shutil.rmtree(self.card/'GAMES/DICE')
        self.assertEqual(self.scan()['games'], 0)

    def test_duplicates_reappear_after_winner_removed(self):
        self.dice('FIRST'); self.dice('SECOND')
        d = self.scan()
        self.assertEqual((d['games'], d['crc_calls']), (1, 2))
        winner = d['list'][0]['dir']  # native encounter order, not alphabetical
        shutil.rmtree(self.card/'GAMES'/winner)
        d = self.scan()
        self.assertEqual(d['games'], 1)
        self.assertNotEqual(d['list'][0]['dir'], winner)

    def test_pack_priority_and_launch_mapping_preserved(self):
        self.dice()
        (self.card/'POCKET').mkdir()
        for folder in ('POCKET', 'GAMES'):
            shutil.copyfile(ROOT/'tests/data/dice.gmp', self.card/folder/'DICE.GMP')
        d = self.scan()
        self.assertEqual((d['games'], d['crc_calls']), (1, 3))
        self.assertEqual((d['list'][0]['packed'], d['list'][0]['pack_folder'], d['list'][0]['pack_base']), (1, 2, 'DICE'))
        self.assertEqual(len(d['list'][0]['volume_sizes']), 1)
        (self.card/'POCKET/DICE.GMP').unlink()
        self.assertEqual(self.scan()['list'][0]['pack_folder'], 1)
        (self.card/'GAMES/DICE.GMP').unlink()
        self.assertEqual(self.scan()['list'][0]['packed'], 0)

    def test_invalid_pack_and_zip_do_not_hide_loose_game(self):
        self.dice()
        (self.card/'GAMES/BAD.GMP').write_bytes(b'not a pack')
        shutil.copyfile(ROOT/'tests/data/dice.gmp', self.card/'GAMES/DICE.ZIP')
        self.assertEqual(self.scan()['list'][0]['packed'], 0)
        self.assertEqual(self.scan()['games'], 1)

    def test_same_size_payload_edit_is_not_trusted_as_unchanged(self):
        folder = self.dice()
        old = self.scan()['list'][0]['crc']
        p = folder/'DICE.GAM'
        data = bytearray(p.read_bytes()); data[-1] ^= 1; p.write_bytes(data)
        self.assertNotEqual(self.scan()['list'][0]['crc'], old)


class PersistentScan(Scan):
    """Same real scanner with opt-in cache writes to disposable card copies.
    Host POSIX durability is not evidence of SD/FAT atomicity (kernel tests are).
    """
    def scan(self):
        env = dict(os.environ, GMOS_SCAN_CACHE='1')
        return json.loads(subprocess.check_output([str(self.probe), str(self.card)], env=env))

    def test_zip_cache_changes_duplicates_and_invalid_recovery(self):
        import io, zipfile
        def make_zip(gam):
            b=io.BytesIO()
            with zipfile.ZipFile(b,'w',zipfile.ZIP_DEFLATED) as z: z.writestr('DICE.GAM',gam)
            return b.getvalue()
        gam=(ROOT/'tests/games/DICE/DICE.GAM').read_bytes()
        archive=self.card/'GAMES/A long archive.ZIP';archive.write_bytes(make_zip(gam))
        self.assertEqual(self.scan()['games'],1)
        warm=self.scan();self.assertEqual(warm['crc_calls'],0)
        self.assertEqual(warm['list'][0]['source_path'],'/sd/GAMES/A long archive.ZIP')
        self.dice('Long loose duplicate')
        self.assertEqual(self.scan()['list'][0]['packed'],0)
        shutil.rmtree(self.card/'GAMES/Long loose duplicate')
        self.assertEqual(self.scan()['list'][0]['packed'],2)
        changed=bytearray(gam);changed[-1]^=77;archive.write_bytes(make_zip(changed))
        self.assertEqual(self.scan()['crc_calls'],1)
        self.assertEqual(self.scan()['crc_calls'],0)
        archive.write_bytes(b'broken ZIP')
        self.assertEqual(self.scan()['games'],0)
        archive.write_bytes(make_zip(gam))
        self.assertEqual(self.scan()['games'],1)
        archive.unlink();self.assertEqual(self.scan()['games'],0)

    def test_long_gam_cache_exact_paths_and_no_truncated_join(self):
        folder=self.dice('Long Game Folder')
        (folder/'DICE.GAM').rename(folder/'Long Metadata Filename.gam')
        cold=self.scan();warm=self.scan()
        self.assertEqual(cold['list'],warm['list'])
        self.assertEqual(warm['crc_calls'],0)
        self.assertEqual(warm['list'][0]['source_gam'],'Long Metadata Filename.gam')
        self.dice('x'*240) # folder path fits, joined GAM path does not
        self.assertEqual(self.scan()['games'],1)
        cache=self.card/'OS/CACHE/GAMES.IDX'
        old=bytearray(cache.read_bytes());old[:4]=b'GMI1'
        import struct,zlib
        struct.pack_into('<I',old,12,zlib.crc32(old[:12]+old[16:]));cache.write_bytes(old)
        self.assertEqual(self.scan()['crc_calls'],1) # old namespace is not replayed

    def test_warm_restart_reuses_candidates_without_gam_reads(self):
        self.dice('FIRST'); self.dice('SECOND')
        cold = self.scan(); warm = self.scan()
        self.assertTrue((self.card/'OS/CACHE/GAMES.IDX').exists(), 'runtime cache was not persisted')
        self.assertEqual(warm['list'], cold['list'])
        self.assertEqual((warm['menu_crc'], warm['where_crc']), (cold['menu_crc'], cold['where_crc']))
        self.assertEqual(warm['crc_calls'], 0, 'unchanged candidates should not reopen/CRC GAM payloads')
        self.assertLess(warm['file_opens'], cold['file_opens'])

    def test_failed_pack_metadata_read_is_not_cached_as_absent(self):
        import struct
        pack = (ROOT/'tests/data/dice.gmp').read_bytes()
        count = struct.unpack_from('<I', pack, 4)[0]
        record = json.dumps(dict(schema='gmp-library', title='Cached pack title', locator='DICE', category=['sandbox'])).encode()
        entries = bytearray(pack[8:8+count*40])
        for i in range(count): struct.pack_into('<I', entries, i*40+32, struct.unpack_from('<I', entries, i*40+32)[0]+40)
        entries += b'library.json'.ljust(32, b'\0')+struct.pack('<II', len(pack)+40, len(record))
        pack = b'GMS1'+struct.pack('<I', count+1)+entries+pack[8+count*40:]+record
        count += 1
        (self.card/'GAMES/DICE.GMP').write_bytes(pack)
        offsets = {pack[8+i*40:40+i*40].split(b'\0')[0]:struct.unpack_from('<I', pack, 40+i*40)[0] for i in range(count)}
        self.assertIn(b'library.json', offsets)
        env = dict(os.environ, GMOS_SCAN_CACHE='1', GMOS_SCAN_FAIL_FILE='DICE.GMP', GMOS_SCAN_FAIL_AT=str(offsets[b'library.json']))
        subprocess.check_output([str(self.probe), str(self.card)], env=env)
        recovered = self.scan()
        uncached = json.loads(subprocess.check_output([str(self.probe), str(self.card)]))
        self.assertEqual(recovered['list'], uncached['list'], 'transient metadata failure poisoned persistent cache')
        self.assertEqual(recovered['crc_calls'], 1, 'failed pack metadata must be retried')

    def test_corrupt_truncated_oversized_and_stale_cache_rebuild(self):
        self.dice()
        expected = self.scan()['list']
        cache = self.card/'OS/CACHE/GAMES.IDX'
        valid = cache.read_bytes()
        for data in (b'', valid[:15], valid[:-1], b'bad magic'+valid[9:], valid+bytes(256*1024)):
            with self.subTest(size=len(data)):
                cache.write_bytes(data)
                self.assertEqual(self.scan()['list'], expected)
                self.assertEqual(self.scan()['crc_calls'], 0)  # rebuilt and usable on next launch
        cache.unlink()
        self.assertEqual(self.scan()['crc_calls'], 1)

    def test_one_changed_candidate_rescans_only_it_and_no_warm_writes(self):
        first = self.dice('FIRST'); self.dice('SECOND')
        self.scan(); self.assertEqual(self.scan()['crc_calls'], 0)
        cache = self.card/'OS/CACHE/GAMES.IDX'
        before = cache.stat().st_mtime_ns
        self.assertEqual(self.scan()['crc_calls'], 0)
        self.assertEqual(cache.stat().st_mtime_ns, before)
        gam = first/'DICE.GAM'; data = bytearray(gam.read_bytes()); data[-1] ^= 1; gam.write_bytes(data)
        changed = self.scan()
        self.assertEqual((changed['games'], changed['crc_calls']), (2, 1))
        self.assertEqual(self.scan()['crc_calls'], 0)

    def test_invalid_sources_are_negatively_cached_without_hiding_new_content(self):
        self.dice()
        (self.card/'GAMES/BAD').mkdir()
        (self.card/'GAMES/BAD/BAD.GAM').write_bytes(b'bad')
        (self.card/'GAMES/BAD.GMP').write_bytes(b'not a pack')
        self.scan()
        warm = self.scan()
        self.assertEqual(warm['file_opens'], 1, 'unchanged invalid candidates should only read the cache')
        shutil.copyfile(ROOT/'tests/games/DICE/DICE.GAM', self.card/'GAMES/BAD/BAD.GAM')
        changed = self.scan()
        self.assertEqual(changed['crc_calls'], 1)
        self.assertEqual(changed['games'], 1)  # duplicate still suppressed

    def test_nested_first_gam_change_and_os_library_fallback(self):
        folder = self.dice(); self.scan()
        (folder/'DICE.GAM').rename(folder/'OTHER.GAM')
        self.assertEqual(self.scan()['list'][0]['gam'], 'OTHER.GAM')
        (self.card/'OS/LIBRARY.JSL').write_text(json.dumps(dict(schema='gmp-library', title='OS fallback', locator='DICE', category=['sandbox']))+'\n')
        self.assertEqual(self.scan()['list'][0]['title'], 'OS fallback')
        warm = self.scan()
        self.assertEqual(warm['list'][0]['title'], 'OS fallback')
        self.assertEqual(warm['crc_calls'], 0)
