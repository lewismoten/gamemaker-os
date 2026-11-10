#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Open a GMOS game's details over sandbox-kernel's synthetic USB keyboard."""
import argparse
import pathlib
import re
import subprocess
import sys

LETTERS = dict(zip('QWERTYUIOPASDFGHJKLZXCVBNM',
                   (16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 30, 31, 32,
                    33, 34, 35, 36, 37, 38, 44, 45, 46, 47, 48, 49, 50)))


def request_keys(folder, play=False):
    folder = folder.upper()
    if not re.fullmatch(r'[A-Z0-9_-]{1,8}', folder):
        raise ValueError('folder must be a DOS alias: 1–8 letters, digits, _ or -')
    keys = ['53']  # slash starts (or resets) the request
    for char in folder:
        if char == '_':
            keys += ['42:down', '12', '42:up']
        else:
            code = LETTERS.get(char)
            if code is None:
                code = 12 if char == '-' else 11 if char == '0' else int(char) + 1
            keys.append(str(code))
    return keys + (['42:down', 'enter', '42:up'] if play else ['enter'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('folder')
    parser.add_argument('--play', action='store_true', help='also press Play when found')
    parser.add_argument('--usb-kernel', type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parents[2]
                        / 'sandbox-kernel/scripts/usb_kernel.py')
    args = parser.parse_args()
    try:
        keys = request_keys(args.folder, args.play)
    except ValueError as error:
        parser.error(str(error))
    return subprocess.call([sys.executable, str(args.usb_kernel), args.port,
                            'key', *keys, '--gap', '0.1', '--seconds', '2'])


if __name__ == '__main__':
    sys.exit(main())
