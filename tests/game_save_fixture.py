"""Independent synthetic DOS 3.00 fwrite-order oracle; no historical save data.

Layout from public code/GM at 7f614cfbfa5c8c15c83d586a46edf57af79d1afc:
PLAYGAME.C:113-172, 207-265, 1874-1890, CHAR.H:18-51, MAP.H:15-30.
Widths independently corroborated against public cd/gm/playgame.exe. These are
structural fixtures, not playable DOS sessions or proof of runtime interchange.
Copyright (c) 2026 Lewis Moten. MIT. Hermes OpenAI Codex GPT-6.1 Sol.
"""
from struct import pack


def pattern(n, seed):
    return bytes((i * 37 + seed) & 255 for i in range(n))


def dos_save(blocks, records):
    """Return the bytes in PLAYGAME's individual fwrite calls, not a host struct."""
    chunks = [
        pack('<BhhhI', blocks, -32768, 32767, -2, 0xFEDCBA98),
        b''.join(pack('<I', 0x12345678 ^ i) for i in range(150)),
        pattern(150, 1), pattern(135, 2), pattern(3784, 3),
        pack('<hh', -12345, 23456),
        pattern(200 * 55, 4), pattern(200 * 10, 5),
        pattern(100 * 100 * 2, 6), pack('<B', 200),
    ]
    chunks.extend(pack('<BI', i, 0x87654321 ^ i) for i in range(blocks))
    # Recording: index, last key index, filename, far-pointer TOKEN, timer,
    # flag, last key time, 16-bit enum boolean. No host pointers are encoded.
    chunks.append(pack('<hh51sIIBIH', records, -123, pattern(51, 7),
                       0x1234FEDC, 0xABCDEF01, 1, 0x80000000, 0x8001))
    chunks.extend(pack('<HB', (i * 97) & 65535, i & 255) for i in range(records))
    wire = b''.join(chunks)
    assert len(wire) == 37755 + blocks * 5 + records * 3
    return wire
