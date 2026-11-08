#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Plays a game on the board over USB and checks it against the host.

  tools/board_play.py <kernel checkout> <port> [--game DIR] [--folder PATH]
                      [--put] [--put-elf] [--pages] [--log FILE]
      (--put-elf installs build/GMOS.ELF; --put installs the game's folder as
      /GAMES/<DIR>; DIR defaults to DICE and PATH to tests/games/<DIR>)
      runs GMOS.ELF, picks the game by its folder name in the list, lets its
      pages play (Enter every 4 s, or none with --pages, which waits for them),
      then plays a little: right, two jumps, left, up and down, a few seconds
      standing for the timing lines; leaves with Esc and quits with Q. Every
      line the board sends goes to the log.
  tools/board_play.py --check FILE [--folder PATH]
      replays the game keys the board logged ("GMOS key <scan> down before tick
      N", "GMOS keys released before tick N") on the host player
      (build/host/gmplay) and compares the view hash of every tick the board
      logged ("GMOS t N lv L h HASH"); PATH defaults to the folder named by the
      log's "GMOS play <DIR>/..." line under tests/games

The kernel checkout gives scripts/usb_kernel.py (its serial helpers)."""
import pathlib, re, subprocess, sys, time
ROOT = pathlib.Path(__file__).resolve().parents[1]

def listed_games(lines):
    """Folder names in list order, from raw or timestamped USB log lines."""
    pattern = re.compile(r'^\s*(?:\d+\.\d+\s+)?(?:LOG OS: )?GMOS game '
                         r'([^\s:\[\]]+)(?: \[[^\s:\[\]]+\])?:[^\r\n]*$')
    return [m.group(1) for line in lines if (m := pattern.fullmatch(line.rstrip('\r\n')))]


def drive(kernel, port_path, log_path, game, folder, put, put_elf, pages):
    sys.path.insert(0, str(pathlib.Path(kernel) / 'scripts'))
    import usb_kernel as u
    if put or put_elf:
        port = u.Serial(port_path)
        try:
            if put_elf: u.send_file(port, ROOT / 'build/GMOS.ELF', 'GMOS.ELF')
            if put: u.send_folder(port, folder, game)
        finally: port.close()
    port = u.Serial(port_path); lines = []; t0 = time.monotonic()
    def note(line): lines.append(f'{time.monotonic() - t0:9.3f} {line}')
    def pump(seconds, until=None):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            try: line = port.line(max(0, end - time.monotonic()))
            except TimeoutError: break
            note(line)
            if until and until in line: return True
        return False
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
        listed = listed_games(lines)
        if game not in listed: raise SystemExit(f'{game} is not in the list: {listed}')
        for _ in range(listed.index(game)): press(108, 0.15)   # down to it
        press(28, 0.5)                                           # Enter
        end = time.monotonic() + 90
        while time.monotonic() < end and not any('GMOS level 1' in l for l in lines):
            if pump(4, 'GMOS level 1'): break
            if not pages: press(28, 0.0)                         # Enter moves a page on
        pump(1.5)
        key(106, 'down', 1.5)             # walk right ...
        press(57, 0.8); press(57, 0.8)    # ... jumping twice
        key(106, 'up', 0.5)
        key(105, 'down', 1.0); key(105, 'up', 0.5)
        key(103, 'down', 0.8); key(103, 'up', 0.3)
        key(108, 'down', 0.8); key(108, 'up', 6.0)   # then stand for the timing lines
        press(1, 1.5)                     # Esc: back to the list
        press(16, 2.0)                    # Q: quit (the kernel logs the screen's CRC)
    finally:
        port.close()
        pathlib.Path(log_path).write_text('\n'.join(lines) + '\n')
    print(f'{len(lines)} lines in {log_path}')

def check(log_path, folder):
    text = pathlib.Path(log_path).read_text()
    if folder is None:
        m = re.search(r'GMOS play (\S+)/', text)
        folder = ROOT / 'tests/games' / (m.group(1) if m else 'DICE')
    gam = next(p.name for p in pathlib.Path(folder).iterdir() if p.suffix.lower() == '.gam')
    events = [(int(t), int(s) + (0 if d == 'down' else 128)) for s, d, t in re.findall(r'GMOS key (\d+) (down|up) before tick (\d+)', text)]
    released = [int(t) for t in re.findall(r'GMOS keys released before tick (\d+)', text)]
    board = {int(t): int(h) for t, lv, h in re.findall(r'GMOS t (\d+) lv (\d+) h (\d+)', text)}
    if not board: print('no ticks in the log'); return 1
    script = ROOT / 'build/board-keys.txt'
    lines = sorted([(t, 0, i, f'K {t} {k}') for i, (t, k) in enumerate(events)] + [(t, -1, 0, f'A {t}') for t in released])
    script.write_text('R 1\n' + ''.join(l[-1] + '\n' for l in lines) + f'T {max(board)}\n')
    subprocess.run(['make', '-s', '-C', str(ROOT), 'host'], check=True)
    out = subprocess.run([str(ROOT / 'build/host/gmplay'), str(folder), gam, str(script)], check=True, capture_output=True, text=True).stdout
    host = {int(l.split()[1]): int(l.split()[-1]) for l in out.splitlines() if l.startswith('S ')}
    bad = [t for t in sorted(board) if host.get(t) != board[t]]
    print(f'{len(board)} ticks logged by the board, {len(events)} key changes; {len(board) - len(bad)} match the host' + (f'; first mismatch at tick {bad[0]}' if bad else ''))
    return 1 if bad else 0

if __name__ == '__main__':
    a = sys.argv[1:]
    opt = lambda name, default=None: a[a.index(name) + 1] if name in a else default
    if a[:1] == ['--check']: sys.exit(check(a[1], opt('--folder')))
    game = opt('--game', 'DICE')
    folder = pathlib.Path(opt('--folder', str(ROOT / 'tests/games' / game)))
    log = opt('--log', str(ROOT / 'build/board.log'))
    sys.exit(drive(a[0], a[1], log, game, folder, '--put' in a, '--put-elf' in a, '--pages' in a))
