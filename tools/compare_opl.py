#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compares the C synth with the web engine on real songs: RMS of the difference, 10 ms envelope
correlation and the largest envelope error. Usage: compare_opl.py <Sandbox Edition checkout> <seconds> song.cmf...
Needs vite-node in that checkout's web/ and cc here."""
import array, math, os, subprocess, sys, tempfile
root, secs, songs = sys.argv[1], sys.argv[2], sys.argv[3:]
here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tmp = tempfile.mkdtemp(prefix="oplcmp-")
exe = os.path.join(tmp, "audio_render")
subprocess.check_call(["cc", "-std=c11", "-O2", "-Isrc", "src/opl.c", "src/opl_tables.c", "src/cmf.c", "src/audio.c", "src/pcspeaker.c", "tests/host/audio_render.c", "-o", exe], cwd=here)
def pcm(path, skip):
    a = array.array("h"); a.frombytes(open(path, "rb").read()[skip:]); return a
def rms(v): return math.sqrt(sum(x * x for x in v) / max(1, len(v)))
for s in songs:
    web, c = os.path.join(tmp, "web.raw"), os.path.join(tmp, "c.wav")
    subprocess.check_call(["npx", "vite-node", os.path.join(here, "tools/web_opl_render.ts"), s, secs, web],
                          cwd=os.path.join(root, "web"), env={**os.environ, "GM_ENGINE": os.path.join(root, "web/packages/engine/src")})
    subprocess.check_call([exe, s, secs, c])
    w, v = pcm(web, 0), pcm(c, 44); m = min(len(w), len(v))
    d = [w[i] - v[i] for i in range(m)]
    k = 480; ew = [rms(w[i:i + k]) for i in range(0, m - k, k)]; ec = [rms(v[i:i + k]) for i in range(0, m - k, k)]
    mw, mc = sum(ew) / len(ew), sum(ec) / len(ec)
    cov = sum((a - mw) * (b - mc) for a, b in zip(ew, ec)); den = math.sqrt(sum((a - mw) ** 2 for a in ew) * sum((b - mc) ** 2 for b in ec)) or 1
    print(f"{os.path.basename(s)}: web RMS {rms(w):.0f}, C RMS {rms(v):.0f}, difference RMS {rms(d):.0f} ({100 * rms(d) / max(1, rms(w)):.1f}% of web); "
          f"10 ms envelope correlation {cov / den:.4f}, largest envelope error {100 * max(abs(a - b) for a, b in zip(ew, ec)) / max(ew):.1f}% of the loudest")
