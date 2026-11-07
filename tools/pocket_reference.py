#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compares GMOS's game player with the Pocket Player's, which runs the same
play core (third_party/gmcore) on the games' packed files. With a
Game-Maker: Sandbox Edition checkout (built, so games/<id>/disk exists):

  tools/pocket_reference.py <checkout> compare [id ...]   every game both can play: tick-by-tick states, one line each
  tools/pocket_reference.py <checkout> write              regenerate tests/data/dice-pocket.txt and dice-cards.txt

The key script is tests/data/dice.keys. The Pocket Player's packed data is
hardware/pocket-player/data in that checkout; a game changed since it was
packed differs for that reason alone."""
import pathlib, struct, subprocess, sys, zlib
ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/pocket'

def pocket_host(gm):
    pp = gm / 'hardware/pocket-player'
    BUILD.mkdir(parents=True, exist_ok=True)
    exe = BUILD / 'hosttest'
    subprocess.run(['c++', '-std=c++17', '-O1', '-I', str(pp / 'src'), str(pp / 'host/hosttest.cpp'), '-o', str(exe), '-lz'], check=True)
    return exe, pp / 'data'

def states(cmd):
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if l.startswith('S ')]

def pak_entry(data, name):
    count = struct.unpack_from('<I', data, 4)[0]
    for i in range(count):
        n, off, ln = struct.unpack_from('<32sII', data, 8 + 40 * i)
        if n.rstrip(b'\0').decode() == name: return data[off:off + ln]
    raise KeyError(name)

def card_crcs(data_dir, game):
    """CRC-32 of each GIF card's palette and pixels (768 + 64000 bytes) as the Pocket Player packed them."""
    cfg = pak_entry((data_dir / 'small.pak').read_bytes(), game + '.cfg').decode('latin-1')
    cards = (data_dir / 'cards.pak').read_bytes()
    out = []
    for line in cfg.splitlines():
        f = line.split()
        if len(f) >= 4 and f[0] == 'card' and f[2] == 'g':
            off, ln = map(int, f[3].split('.'))
            raw = zlib.decompress(cards[off + 4:off + ln], -15)  # a u32 length, then raw deflate
            out.append(f'{f[1]} {zlib.crc32(raw[:768 + 64000])}')
    return out

def main():
    gm = pathlib.Path(sys.argv[1]).resolve(); what = sys.argv[2]
    exe, data = pocket_host(gm)
    subprocess.run(['make', '-s', '-C', str(ROOT), 'host'], check=True)
    keys = ROOT / 'tests/data/dice.keys'
    if what == 'write':
        (ROOT / 'tests/data/dice-pocket.txt').write_text('\n'.join(states([str(exe), str(data), 'dice', str(keys)])) + '\n')
        (ROOT / 'tests/data/dice-cards.txt').write_text('\n'.join(card_crcs(data, 'dice')) + '\n')
        return 0
    ids = sys.argv[3:] or [l.split('|')[0] for l in (data / 'games.txt').read_text().split('\n') if l]
    bad = 0
    for gid in ids:
        disk = gm / 'games' / gid / 'disk'
        gams = sorted(disk.glob('*.gam')) if disk.is_dir() else []
        if not gams: print(f'{gid:24} no disk'); continue
        a = states([str(exe), str(data), gid, str(keys)])
        b = states([str(ROOT / 'build/host/gmplay'), str(disk), gams[0].name, str(keys)])
        first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), None)
        same = first is None and len(a) == len(b)
        bad += not same
        print(f'{gid:24} ' + (f'same, {len(a)} ticks' if same else f'differs from tick {(first if first is not None else min(len(a), len(b))) + 1}'))
    return 1 if bad else 0

if __name__ == '__main__': sys.exit(main())
