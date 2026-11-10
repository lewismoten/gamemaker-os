/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten; see opl.h for what it ports. */
#include "opl.h"
enum {OFF, ATTACK, DECAY, SUSTAIN, RELEASE};
#define SILENT_Q8 (96u * 256u)
static const uint8_t MULT2[16] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30};
/* ch * 3.. : operator register offsets to a channel and operator (see opl.ts SLOT_OFFSETS). */
static struct opl_op *op_of(struct opl *o, unsigned off, struct opl_ch **chp) {
  unsigned g = off >> 3, i = off & 7;
  if (g > 2 || i > 5) return 0;
  *chp = &o->ch[g * 3 + i % 3];
  return &(*chp)->op[i / 3];
}
/* Phase step of a full-scale cycle per sample: fnum * mult * 2^block * 49716 / 48000 / 2^20. */
static void op_freq(const struct opl_ch *c, struct opl_op *p) {
  uint64_t x = (uint64_t)((uint32_t)c->fnum * p->mult2 << c->block);
  p->inc = (uint32_t)(x * 139016012ull >> 16); /* 2121.2 in Q16 */
}
static void op_level(const struct opl_ch *c, struct opl_op *p) {
  int v = p->ksl ? (int)opl_ksl[c->fnum >> 6] - 48 * (7 - (int)c->block) : 0;
  if (v < 0) v = 0;
  unsigned k = p->ksl == 1 ? 16u : p->ksl == 2 ? 8u : 32u; /* shifts 0.5, 0.25, 1 */
  p->base = p->tl * 192u + (p->ksl ? (unsigned)v * k : 0u);
}
static void key_on(struct opl_op *p) {p->stage = ATTACK; p->phase = 0;}
static void key_off(struct opl_op *p) {if (p->stage != OFF) p->stage = RELEASE;}
void opl_reset(struct opl *o) {
  unsigned i;
  uint8_t *b = (uint8_t *)o;
  for (i = 0; i < sizeof *o; i++) b[i] = 0;
  o->noise = 1;
  for (i = 0; i < 9; i++) for (unsigned k = 0; k < 2; k++) {o->ch[i].op[k].env = (int32_t)(96u << 24); o->ch[i].op[k].mult2 = 1;}
  for (i = 0; i < 256; i++) opl_write(o, i, 0);
  for (i = 0; i < 9; i++) for (unsigned k = 0; k < 2; k++) {o->ch[i].op[k].stage = OFF; o->ch[i].op[k].env = (int32_t)(96u << 24);}
}
unsigned opl_read(const struct opl *o, unsigned reg) {return o->regs[reg & 0xff];}
static void rhythm_write(struct opl *o, unsigned v) {
  o->am_depth = (v & 0x80) != 0; o->vib_depth = (v & 0x40) != 0; o->rhythm = (v & 0x20) != 0;
  unsigned keys = o->rhythm ? v & 0x1f : 0, changed = keys ^ o->rhythm_keys;
  struct opl_ch *bd = &o->ch[6], *sh = &o->ch[7], *tc = &o->ch[8];
  struct opl_op *list[6] = {&bd->op[0], &bd->op[1], &sh->op[1], &tc->op[0], &tc->op[1], &sh->op[0]};
  static const uint8_t bit[6] = {0x10, 0x10, 0x08, 0x04, 0x02, 0x01};
  for (unsigned i = 0; i < 6; i++) if (changed & bit[i]) {if (keys & bit[i]) key_on(list[i]); else key_off(list[i]);}
  o->rhythm_keys = (uint8_t)keys;
}
void opl_write(struct opl *o, unsigned reg, unsigned v) {
  reg &= 0xff; v &= 0xff; o->regs[reg] = (uint8_t)v;
  if (reg == 0x01) {o->wave_select = (v & 0x20) != 0; return;}
  if (reg == 0x08) {o->note_sel = (v & 0x40) != 0; return;}
  if (reg == 0xbd) {rhythm_write(o, v); return;}
  unsigned group = reg & 0xe0;
  if ((group >= 0x20 && group <= 0x80) || group == 0xe0) {
    struct opl_ch *c; struct opl_op *p = op_of(o, reg & 0x1f, &c);
    if (!p) return;
    switch (group) {
      case 0x20: p->am = (v & 0x80) != 0; p->vib = (v & 0x40) != 0; p->hold = (v & 0x20) != 0; p->ksr = (v & 0x10) != 0;
                 p->mult2 = MULT2[v & 15]; op_freq(c, p); break;
      case 0x40: p->ksl = (uint8_t)(v >> 6); p->tl = (uint8_t)(v & 63); op_level(c, p); break;
      case 0x60: p->ar = (uint8_t)(v >> 4); p->dr = (uint8_t)(v & 15); break;
      case 0x80: p->sl = (uint8_t)(v >> 4); p->rr = (uint8_t)(v & 15); break;
      default: p->wave = o->wave_select ? (uint8_t)(v & 3) : 0; break;
    }
    return;
  }
  unsigned ci = reg & 0x0f;
  if (ci > 8) return;
  struct opl_ch *c = &o->ch[ci];
  if (group == 0xa0 && reg < 0xb0) c->fnum = (uint16_t)((c->fnum & 0x300) | v);
  else if (reg >= 0xb0 && reg <= 0xb8) {
    c->fnum = (uint16_t)((c->fnum & 0xff) | (v & 3) << 8); c->block = (uint8_t)(v >> 2 & 7);
    unsigned key = (v & 0x20) != 0;
    if (key && !c->key) {key_on(&c->op[0]); key_on(&c->op[1]);}
    if (!key && c->key) {key_off(&c->op[0]); key_off(&c->op[1]);}
    c->key = (uint8_t)key;
  } else if (reg >= 0xc0 && reg <= 0xc8) {c->fb = (uint8_t)(v >> 1 & 7); c->add = (uint8_t)(v & 1); return;}
  else return;
  for (unsigned k = 0; k < 2; k++) {op_freq(c, &c->op[k]); op_level(c, &c->op[k]);}
}
/* The sine of a 32-bit phase, 12-bit signed (4095 = 1.0), from the quarter table. */
static int sin12(uint32_t p) {
  unsigned i = p >> 22, q = i >> 8, k = i & 255;
  int s = opl_sine[q & 1 ? 255 - k : k];
  return q & 2 ? -s : s;
}
static int wave(unsigned w, uint32_t p) {
  int s = sin12(p);
  switch (w) {
    case 1: return p >> 31 ? 0 : s;
    case 2: return s < 0 ? -s : s;
    case 3: {unsigned q = p >> 30; return (q & 1) ? 0 : (s < 0 ? -s : s);}
    default: return s;
  }
}
static unsigned rate_of(unsigned r, unsigned rof, unsigned ksr) {
  if (!r) return 0;
  unsigned v = r * 4 + (ksr ? rof : rof >> 2);
  return v > 63 ? 63 : v;
}
static void envelope(struct opl_op *p, unsigned rof) {
  switch (p->stage) {
    case ATTACK: {
      unsigned rate = rate_of(p->ar, rof, p->ksr);
      if (!rate) return;
      if (rate >= 60) p->env = 0;
      else p->env -= (int32_t)(((int64_t)p->env + (8ll << 24)) * opl_attack[rate] >> 31);
      if (p->env <= 0) {p->env = 0; p->stage = DECAY;}
      return;
    }
    case DECAY: {
      int32_t target = (int32_t)((p->sl == 15 ? 93u : p->sl * 3u) << 24);
      p->env += (int32_t)opl_decay[rate_of(p->dr, rof, p->ksr)];
      if (p->env >= target) {p->env = target; p->stage = SUSTAIN;}
      return;
    }
    case SUSTAIN: if (!p->hold) p->env += (int32_t)opl_decay[rate_of(p->rr, rof, p->ksr)]; break;
    case RELEASE: p->env += (int32_t)opl_decay[rate_of(p->rr, rof, p->ksr)]; break;
    default: return;
  }
  if (p->env >= (int32_t)(96u << 24)) {p->env = (int32_t)(96u << 24); p->stage = OFF;}
}
/* Linear amplitude (15-bit, 32768 = 1.0) of the summed attenuation. */
static int level(const struct opl_op *p, unsigned trem) {
  unsigned db = (unsigned)(p->env >> 16) + p->base + (p->am ? trem : 0);
  if (db >= SILENT_Q8) return 0;
  unsigned x = db * 2721u >> 16, sh = x >> 6; /* 10^(-dB/20) = 2^(-dB * 0.1661) */
  return sh > 15 ? 0 : opl_exp2[x & 63] >> sh;
}
static void advance(const struct opl *o, const struct opl_ch *c, struct opl_op *p, uint32_t vibf) {
  p->phase += p->vib ? (uint32_t)((uint64_t)p->inc * vibf >> 16) : p->inc;
  envelope(p, (unsigned)c->block * 2u + ((c->fnum >> (o->note_sel ? 8 : 9)) & 1u));
}
static int chan(struct opl *o, struct opl_ch *c, unsigned trem, uint32_t vibf) {
  struct opl_op *m = &c->op[0], *k = &c->op[1];
  if (m->stage == OFF && k->stage == OFF) return 0;
  advance(o, c, m, vibf); advance(o, c, k, vibf);
  uint32_t fb = c->fb ? (uint32_t)((int32_t)m->out + m->prev) << (13 + c->fb) : 0;
  int mo = wave(m->wave, m->phase + fb) * level(m, trem) >> 15;
  m->prev = m->out; m->out = (int16_t)mo;
  int cl = level(k, trem);
  if (c->add) return mo + (wave(k->wave, k->phase) * cl >> 15);
  return wave(k->wave, k->phase + ((uint32_t)mo << 22)) * cl >> 15;
}
/* The five drums, with the web port's approximate weights (opl.ts rhythmSample). */
static int drums(struct opl *o, unsigned trem, uint32_t vibf, int noise) {
  struct opl_ch *c7 = &o->ch[7], *c8 = &o->ch[8];
  int sum = chan(o, &o->ch[6], trem, vibf) * 2;
  struct opl_op *hh = &c7->op[0], *sd = &c7->op[1], *tom = &c8->op[0], *cy = &c8->op[1];
  advance(o, c7, hh, vibf); advance(o, c7, sd, vibf); advance(o, c8, tom, vibf); advance(o, c8, cy, vibf);
  if (hh->stage != OFF) {int l = level(hh, trem) >> 3; sum += noise * (wave(hh->wave, hh->phase) > 0 ? l : l * 2458 >> 12);}
  if (sd->stage != OFF) sum += ((wave(sd->wave, sd->phase) * 2458 >> 12) + noise * 1638) * (level(sd, trem) >> 3) >> 11;
  if (tom->stage != OFF) sum += wave(tom->wave, tom->phase) * level(tom, trem) >> 14;
  if (cy->stage != OFF) {int l = level(cy, trem) >> 3; if (noise <= 0) l = l * 2867 >> 12; sum += wave(cy->wave, cy->phase) > 0 ? l : -l;}
  return sum;
}
void opl_render(struct opl *o, int32_t *acc, unsigned n) {
  for (unsigned i = 0; i < n; i++) {
    o->lfo_t += 331077u; o->lfo_v += 545818u; /* 3.7 Hz tremolo, 6.1 Hz vibrato */
    unsigned trem = (unsigned)(4096 - sin12(o->lfo_t + (1u << 30))) * (o->am_depth ? 1229u : 256u) >> 13;
    uint32_t vibf = (uint32_t)(65536 + (sin12(o->lfo_v) * (o->vib_depth ? 14 : 7) * 38 >> 12));
    o->noise = o->noise >> 1 | ((o->noise ^ o->noise >> 14) & 1u) << 22;
    int sum = 0, melodic = o->rhythm ? 6 : 9;
    for (int c = 0; c < melodic; c++) sum += chan(o, &o->ch[c], trem, vibf);
    if (o->rhythm) sum += drums(o, trem, vibf, o->noise & 1u ? 1 : -1);
    acc[i] += sum * 786 >> 10;
  }
}
