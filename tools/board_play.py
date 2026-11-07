#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Plays a game on the board over USB and checks it against the host.

  tools/board_play.py <kernel checkout> <port> [--log FILE] [--put]
      (--put first installs build/GMOS.ELF and tests/games/DICE as /GAMES/DICE)
      runs GMOS.ELF, picks Roll the Dice, skips its two cards with Enter, walks
      right and jumps for a few seconds, leaves with Esc and quits with Q; every
      line the board sends goes to the log
  tools/board_play.py --check FILE
      replays the game keys the board logged ("GMOS key <scan> down before tick
      N") on the host player (build/host/gmplay) and compares the view hash of
      every tick the board logged ("GMOS t N lv L h HASH")

The kernel checkout gives scripts/usb_kernel.py (its serial helpers)."""
import pathlib, re, subprocess, sys, time
ROOT = pathlib.Path(__file__).resolve().parents[1]

def drive(kernel, port_path, log_path, put):
    sys.path.insert(0, str(pathlib.Path(kernel) / 'scripts'))
    import usb_kernel as u
    if put:
        port = u.Serial(port_path)
        try:
            u.send_file(port, ROOT / 'build/GMOS.ELF', 'GMOS.ELF')
            u.send_folder(port, ROOT / 'tests/games/DICE', 'DICE')
        finally: port.close()
    port = u.Serial(port_path); lines = []; t0 = time.monotonic()
    def note(line): lines.append(f'{time.monotonic() - t0:9.3f} {line}')
    def pump(seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            try: note(port.line(max(0, end - time.monotonic())))
            except TimeoutError: break
    def cmd(text):
        port.send((text + '\n').encode())
        end = time.monotonic() + 5
        while time.monotonic() < end:
            line = port.line(max(0, end - time.monotonic())); note(line)
            if line.startswith(('OK', 'ERR')): return line
        raise TimeoutError(text)
    def key(code, state, wait=0.0): cmd(f'KEY {code} {state}'); pump(wait)
    def press(code, wait=0.4): key(code, 'down', 0.05); key(code, 'up', wait)
    try:
        note(u.handshake(port)); cmd('RUN GMOS.ELF'); pump(3)
        press(108); press(28, 1.5)        # down to Roll the Dice, Enter
        press(28, 1.0); press(28, 2.0)    # Enter through the title and the level's intro
        key(106, 'down', 1.5)             # walk right ...
        press(57, 0.8); press(57, 0.8)    # ... jumping twice
        key(106, 'up', 1.0)
        key(105, 'down', 1.0); key(105, 'up', 6.0)   # left, then stand for the timing lines
        press(1, 1.5)                     # Esc: back to the list
        press(16, 2.0)                    # Q: quit (the kernel logs the screen's CRC)
    finally:
        port.close()
        pathlib.Path(log_path).write_text('\n'.join(lines) + '\n')
    print(f'{len(lines)} lines in {log_path}')

def check(log_path):
    text = pathlib.Path(log_path).read_text()
    keys = [(int(t), int(s) + (0 if d == 'down' else 128)) for s, d, t in re.findall(r'GMOS key (\d+) (down|up) before tick (\d+)', text)]
    board = {int(t): int(h) for t, lv, h in re.findall(r'GMOS t (\d+) lv (\d+) h (\d+)', text)}
    if not board: print('no ticks in the log'); return 1
    script = ROOT / 'build/board-keys.txt'
    script.write_text('R 1\n' + ''.join(f'K {t} {k}\n' for t, k in keys) + f'T {max(board)}\n')
    subprocess.run(['make', '-s', '-C', str(ROOT), 'host'], check=True)
    out = subprocess.run([str(ROOT / 'build/host/gmplay'), str(ROOT / 'tests/games/DICE'), 'DICE.GAM', str(script)], check=True, capture_output=True, text=True).stdout
    host = {int(l.split()[1]): int(l.split()[-1]) for l in out.splitlines() if l.startswith('S ')}
    bad = [t for t in sorted(board) if host.get(t) != board[t]]
    print(f'{len(board)} ticks logged by the board, {len(keys)} key changes; {len(board) - len(bad)} match the host' + (f'; first mismatch at tick {bad[0]}' if bad else ''))
    return 1 if bad else 0

if __name__ == '__main__':
    a = sys.argv[1:]
    if a[:1] == ['--check']: sys.exit(check(a[1]))
    log = a[a.index('--log') + 1] if '--log' in a else str(ROOT / 'build/board.log')
    sys.exit(drive(a[0], a[1], log, '--put' in a))
