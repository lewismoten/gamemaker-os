// Renders a CMF song with the Sandbox Edition web engine's Opl2 and CmfPlayer, run as music.worklet.ts runs them
// (48 kHz, its tick loop), to raw mono 16-bit little-endian PCM scaled by the master gain 0.8; the reference for
// tests/test_audio.py and tools/compare_opl.py. Run from the Sandbox Edition's web/ folder:
//   GM_ENGINE=<repo>/web/packages/engine/src npx vite-node <this file> song.cmf seconds out.raw
import { readFileSync, writeFileSync } from 'node:fs';
const engine = process.env.GM_ENGINE!;
const { Opl2 } = await import(`${engine}/audio/opl`);
const { CmfPlayer, TICK_RATE } = await import(`${engine}/audio/cmfPlayer`);
const { loadSong } = await import(`${engine}/audio/cmfLoad`);
const [, , file, secs, out] = process.argv;
const song = loadSong(new Uint8Array(readFileSync(file)))!;
const rate = 48000, n = Number(secs) * rate;
const opl = new Opl2(rate), player = new CmfPlayer(opl);
player.load(song.music, song.instruments);
const spt = rate / TICK_RATE;
const buf = new Float32Array(n);
let until = 0, i = 0;
while (i < n) {
  if (until <= 0) { player.tick(); until += spt; }
  const k = Math.min(n - i, Math.ceil(until));
  opl.render(buf.subarray(i, i + k));
  until -= k; i += k;
}
const s16 = new Int16Array(n);
for (let j = 0; j < n; j++) s16[j] = Math.max(-32768, Math.min(32767, Math.round(buf[j] * 0.8 * 32768)));
writeFileSync(out, Buffer.from(s16.buffer));
