// Compares src/vocplay.c (through tests/host/vocplay.c's vochost) with the Sandbox Edition web engine's decodeVoc on
// real .voc files: the rate and sample count must match, and the 48 kHz output is checked against the web samples
// resampled by linear interpolation at the engine's 0.8 level (RMS and peak difference, in s16 steps).
//   GM_VOC=<formats/voc.ts> VOCHOST=<built vochost> npx vite-node tools/voc_compare.ts <folder|file>... (run from the Sandbox Edition's web/)
import { readFileSync, readdirSync, statSync, mkdtempSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
const { decodeVoc } = await import(process.env.GM_VOC!);
const host = process.env.VOCHOST!;
const tmp = mkdtempSync(join(tmpdir(), 'voccmp-'));
const files: string[] = [];
const walk = (p: string): void => {
  if (statSync(p).isDirectory()) for (const e of readdirSync(p)) walk(join(p, e));
  else if (/\.voc$/i.test(p)) files.push(p);
};
for (const a of process.argv.slice(2)) walk(a);
// Made-up clips for what the games' files don't use: the three ADPCM codecs, 16-bit, silence, repeats, cut-short blocks, the sample cap.
let seed = 12345;
const rnd = (): number => ((seed = (seed * 1103515245 + 12345) >>> 0) >> 16) & 255;
const blk = (type: number, data: number[]): number[] => [type, data.length & 255, (data.length >> 8) & 255, data.length >> 16, ...data];
const voc = (blocks: number[][], term = true): number[] => [...Buffer.from('Creative Voice File\x1a', 'latin1'), 26, 0, 0x0a, 0x01, 0x29, 0x11, ...blocks.flat(), ...(term ? [0] : [])];
const bytes = (n: number): number[] => Array.from({ length: n }, rnd);
const syn: Record<string, number[]> = {
  adpcm4: voc([blk(1, [165, 1, 128, ...bytes(300)])]),
  adpcm26: voc([blk(1, [165, 2, 120, ...bytes(300)])]),
  adpcm2: voc([blk(1, [150, 3, 140, ...bytes(300)]), blk(2, bytes(100))]),
  pcm16: voc([blk(9, [0x22, 0x56, 0, 0, 16, 1, 4, 0, 0, 0, 0, 0, ...bytes(400)])]),
  pcm8block9: voc([blk(9, [0x40, 0x1f, 0, 0, 8, 1, 0, 0, 0, 0, 0, 0, ...bytes(300)])]),
  silence: voc([blk(1, [165, 0, ...bytes(100)]), blk(3, [255, 3, 165]), blk(1, [165, 0, ...bytes(50)])]),
  repeat: voc([blk(6, [3, 0]), blk(1, [165, 0, ...bytes(40)]), blk(7, [])]),
  endless: voc([blk(6, [255, 255]), blk(1, [165, 0, ...bytes(40)]), blk(7, [])]),
  guard: voc([blk(6, [254, 255]), blk(2, bytes(3)), blk(1, [165, 0, ...bytes(5)]), blk(7, [])]),
  cutshort: voc([blk(1, [165, 0, ...bytes(100)]), [1, 255, 255, 0, 165, 0, ...bytes(60)]], false),
  extended: voc([blk(8, [0x00, 0xe0, 0, 0]), blk(1, [165, 0, ...bytes(200)])]),
  extstereo: voc([blk(8, [0x00, 0xe0, 0, 1]), blk(1, [165, 0, ...bytes(200)])]),
  empty: voc([blk(5, bytes(10))]),
  overcap: voc([blk(6, [44, 1]), blk(3, [255, 255, 165]), blk(7, [])]),
  nearcap: voc([blk(6, [243, 0]), blk(3, [255, 255, 165]), blk(7, [])]),
  adpcmcont: voc([blk(2, bytes(50)), blk(1, [165, 1, 100, ...bytes(10)])]),
  badcodec: voc([blk(1, [165, 9, ...bytes(40)]), blk(1, [165, 0, ...bytes(40)])]),
};
for (const [name, b] of Object.entries(syn)) {
  const f = join(tmp, name + '.voc');
  writeFileSync(f, Buffer.from(b));
  files.push(f);
}
const seen = new Set<string>();
let n = 0, bad = 0, sumSq = 0, count = 0, peak = 0, worst = 0, worstFile = '', refused = 0;
for (const f of files) {
  const bytes = new Uint8Array(readFileSync(f));
  const h = createHash('sha1').update(bytes).digest('hex');
  if (seen.has(h)) continue;
  seen.add(h);
  let web: { sampleRate: number; samples: Float32Array } | undefined;
  try { web = decodeVoc(bytes); } catch { web = undefined; }
  const out = join(tmp, 'o.raw');
  const [st, rate, total, frames] = execFileSync(host, [f, out], { encoding: 'utf8' }).trim().split(' ').map(Number);
  n++;
  if (!web || !web.samples.length) { if (st === 0) { bad++; console.log('MISMATCH (web refuses)', f); } else refused++; continue; }
  if (st !== 0) { bad++; console.log('MISMATCH (C refused', st, ')', f); continue; }
  if (rate !== web.sampleRate || total !== web.samples.length) { bad++; console.log('MISMATCH rate/total', f, rate, web.sampleRate, total, web.samples.length); continue; }
  const pcm = readFileSync(out);
  const s = new Int16Array(pcm.buffer, pcm.byteOffset, pcm.length >> 1);
  const N = web.samples.length, wantFrames = Math.max(1, Math.floor((N * 48000) / rate));
  if (frames !== wantFrames) { bad++; console.log('MISMATCH frames', f, frames, wantFrames); continue; }
  let sq = 0, pk = 0;
  for (let k = 0; k < wantFrames; k++) {
    const x = (k * rate) / 48000, a = Math.floor(x), fr = x - a;
    const v = ((web.samples[Math.min(a, N - 1)] * (1 - fr) + web.samples[Math.min(a + 1, N - 1)] * fr) * 0.8 * 32768);
    const d = s[k] - v; sq += d * d; pk = Math.max(pk, Math.abs(d));
  }
  const rms = Math.sqrt(sq / wantFrames);
  sumSq += sq; count += wantFrames; peak = Math.max(peak, pk);
  if (rms > worst) { worst = rms; worstFile = f; }
}
console.log(`files ${n} (unique), mismatches ${bad}, refused by both ${refused}, frames ${count}`);
console.log(`RMS difference ${Math.sqrt(sumSq / Math.max(1, count)).toFixed(3)} s16 steps overall, worst file ${worst.toFixed(3)} (${worstFile}), peak ${peak.toFixed(2)}`);
process.exit(bad ? 1 : 0);
