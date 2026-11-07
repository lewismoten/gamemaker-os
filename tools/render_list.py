#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Renders the games page on the host, as GMOS.ELF draws it on the board, to a
PNG, and prints the frame's CRC-32 (the board logs "GMOS list frame crc N" for
the same frame once every card on it is drawn).

  tools/render_list.py --card <folder> [--index LIBRARY.JSL] [--keys tab,right,enter] --out page.png
  tools/render_list.py --game DIR=<folder> --game DIR=<folder> ... (in the board's listing order)

--card takes a card folder's GAMES/ (or a folder of game folders) in name
order; --game names each folder and its DIR as the board lists them (its
getdents order decides only ties). The index defaults to the card's
GAMES/LIBRARY.JSL when there is one."""
import argparse, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tests'))
from gmos_page import Page, png, crc  # noqa: E402

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--card', type=pathlib.Path); ap.add_argument('--game', action='append', default=[])
    ap.add_argument('--index', type=pathlib.Path); ap.add_argument('--keys', default='')
    ap.add_argument('--out', type=pathlib.Path, required=True)
    a = ap.parse_args()
    games = []
    if a.card:
        root = a.card / 'GAMES' if (a.card / 'GAMES').is_dir() else a.card
        games += [(d.name, d) for d in sorted(root.iterdir()) if d.is_dir()]
        if not a.index and (root / 'LIBRARY.JSL').exists(): a.index = root / 'LIBRARY.JSL'
    for g in a.game:
        name, _, path = g.partition('='); games.append((name, pathlib.Path(path)))
    p = Page()
    for name, folder in games: p.add_folder(folder, name)
    matched = p.index(a.index.read_text()) if a.index else 0
    p.finish()
    for k in filter(None, a.keys.split(',')): p.key(k)
    px = p.render(); png(a.out, px)
    print(f'{len(games)} games, {matched} in the index; frame crc {crc(px)}; {a.out}')

if __name__ == '__main__': main()
