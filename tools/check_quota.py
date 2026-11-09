#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Fail when an ELF's loaded image (highest LOAD p_vaddr+p_memsz, from the image base) is within
MARGIN of the kernel's per-program quota: the kernel rejects the program ("OS reject ... ELF QUOTA")
when pages*4096 > U_IMAGE_QUOTA. Usage: check_quota.py ELF... [--quota BYTES] [--margin BYTES]."""
import struct, sys

# One place for the number: sandbox-kernel include/uspace.h:13, U_IMAGE_QUOTA (8u<<20). The vendored
# SDK headers (sdk/include) do not carry it.
QUOTA = 8 << 20
MARGIN = 64 << 10  # headroom to keep for the next feature
BASE = 0x08000000  # sdk/lib/sandbox.ld


def span(path):
    d = open(path, "rb").read()
    if d[:4] != b"\x7fELF":
        raise SystemExit(f"{path}: not an ELF")
    phoff, = struct.unpack_from("<I", d, 28)
    phentsize, phnum = struct.unpack_from("<HH", d, 42)
    end = BASE
    for i in range(phnum):
        t, off, va, pa, fsz, msz = struct.unpack_from("<6I", d, phoff + i * phentsize)
        if t == 1:
            end = max(end, va + msz)
    return -(-(end - BASE) // 4096) * 4096  # whole pages, as the kernel counts them


def main(argv):
    quota, margin, files = QUOTA, MARGIN, []
    it = iter(argv)
    for a in it:
        if a == "--quota": quota = int(next(it), 0)
        elif a == "--margin": margin = int(next(it), 0)
        else: files.append(a)
    bad = 0
    for f in files:
        s = span(f)
        ok = s <= quota - margin
        print(f"{f}: {s / 1048576:.2f} MiB of {quota / 1048576:.0f} MiB, headroom {(quota - s) // 1024} KiB: {'ok' if ok else 'TOO BIG'}")
        bad |= not ok
    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
