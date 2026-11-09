/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten; see cmf.h for what it ports. */
#include "cmf.h"
static const uint8_t MOD_OFF[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
static const uint8_t RBITS[5] = {0x10, 0x08, 0x04, 0x02, 0x01};      /* MIDI channels 11..15 */
static const uint8_t RCH[5] = {6, 7, 8, 8, 7};                        /* the FM channel each uses */
static const uint8_t ROPS[5] = {3, 2, 1, 2, 1};                       /* 1 modulator, 2 carrier, 3 both */
static unsigned rd16(const uint8_t *b, uint32_t at, uint32_t len) {return at + 1 < len ? (unsigned)b[at] | (unsigned)b[at + 1] << 8 : 0;}
static void init(struct cmf *c) {
  opl_reset(c->opl); opl_write(c->opl, 0x01, 0x20); opl_write(c->opl, 0x08, 0x00); opl_write(c->opl, 0xbd, 0xc0);
}
void cmf_init(struct cmf *c, struct opl *o) {
  uint8_t *b = (uint8_t *)c;
  for (unsigned i = 0; i < sizeof *c; i++) b[i] = 0;
  c->opl = o; init(c);
}
/* PlayIt read past the music with no check; the end marker 0xFF restarts it, as in the web port. */
static unsigned byte(const struct cmf *c, uint32_t i) {uint32_t at = c->music + i; return at < c->len ? c->data[at] : 0xffu;}
static uint32_t paws(struct cmf *c) {
  uint32_t v = byte(c, c->off);
  if (v & 0x80) {
    unsigned x; v &= 0x7f;
    do {c->off++; x = byte(c, c->off); v = v << 7 | (x & 0x7f);} while ((x & 0x80) && c->music + c->off < c->len);
  }
  c->off++;
  return v;
}
/* The 16-byte SBI record of a program (zeros past the file's end), 0 when the program has none. */
static int inst(const struct cmf *c, unsigned program, uint8_t *rec) {
  if (program >= c->ninst) return 0;
  uint32_t at = c->inst_at + program * 16u;
  for (unsigned i = 0; i < 16; i++) rec[i] = at + i < c->len ? c->data[at + i] : 0;
  return 1;
}
static void load_op(struct cmf *c, unsigned reg, const uint8_t *r, unsigned half) {
  opl_write(c->opl, 0x20 + reg, r[half]); opl_write(c->opl, 0x40 + reg, r[2 + half]);
  opl_write(c->opl, 0x60 + reg, r[4 + half]); opl_write(c->opl, 0x80 + reg, r[6 + half]); opl_write(c->opl, 0xe0 + reg, r[8 + half]);
}
static void load_voice(struct cmf *c, unsigned ch, const uint8_t *r) {
  load_op(c, MOD_OFF[ch], r, 0); load_op(c, MOD_OFF[ch] + 3u, r, 1); opl_write(c->opl, 0xc0 + ch, r[10]);
}
static unsigned voices(const struct cmf *c) {return c->rhythm ? 6u : 9u;}
static void program(struct cmf *c, unsigned ch, unsigned prog) {
  uint8_t r[16]; c->programs[ch] = (uint8_t)prog;
  if (!inst(c, prog, r)) return;
  if (c->rhythm && ch >= 11) {
    unsigned i = ch - 11;
    if (ROPS[i] == 3) load_voice(c, RCH[i], r); else load_op(c, MOD_OFF[RCH[i]] + (ROPS[i] == 2 ? 3u : 0u), r, 0);
    return;
  }
  if (ch < voices(c)) load_voice(c, ch, r);
}
static void all_off(struct cmf *c) {
  for (unsigned i = 0; i < 9; i++) opl_write(c->opl, 0xb0 + i, opl_read(c->opl, 0xb0 + i) & ~0x20u);
  c->rhythm_keys = 0; opl_write(c->opl, 0xbd, 0xc0 | (c->rhythm ? 0x20u : 0));
  for (unsigned i = 0; i < 16; i++) c->notes[i] = -1;
}
int cmf_load(struct cmf *c, const uint8_t *d, uint32_t len) {
  cmf_stop(c);
  if (len < 40 || d[0] != 'C' || d[1] != 'T' || d[2] != 'M' || d[3] != 'F') return -1;
  unsigned n = rd16(d, 36, len);
  if ((n * 16u) & 0x8000u || rd16(d, 12, len) != 146) return -1;
  init(c);
  c->data = d; c->len = len; c->inst_at = rd16(d, 6, len); c->music = rd16(d, 8, len); c->ninst = (uint16_t)n;
  c->rhythm = 0; c->rhythm_keys = 0;
  for (unsigned i = 0; i < 16; i++) program(c, i, i);
  c->off = 0; c->playing = c->music < len;
  return 0;
}
void cmf_stop(struct cmf *c) {all_off(c); c->off = 0; c->playing = 0;}
static void set_mode(struct cmf *c, unsigned r) {
  all_off(c); c->rhythm = r != 0; opl_write(c->opl, 0xbd, 0xc0 | (c->rhythm ? 0x20u : 0));
  if (c->rhythm) for (unsigned ch = 11; ch < 16; ch++) program(c, ch, c->programs[ch]);
}
/* MIDI note to F-number and block: the table is the web port's lowest-block rule, rounded. */
static void pitch(struct cmf *c, unsigned ch, unsigned note, unsigned key) {
  unsigned v = opl_note[note & 127], fnum = v & 0x3ff, block = v >> 10;
  opl_write(c->opl, 0xa0 + ch, fnum & 0xff); opl_write(c->opl, 0xb0 + ch, (key ? 0x20u : 0) | block << 2 | fnum >> 8);
}
static void velocity(struct cmf *c, unsigned ch, unsigned vel) {
  uint8_t r[16]; if (!inst(c, c->programs[ch], r)) return;
  unsigned reg, level, drum = c->rhythm && ch >= 11 && ch < 16;
  if (drum && ROPS[ch - 11] != 3) {reg = MOD_OFF[RCH[ch - 11]] + (ROPS[ch - 11] == 2 ? 3u : 0u); level = r[2];}
  else {
    unsigned x = drum ? RCH[ch - 11] : ch;
    if (!drum && x >= voices(c)) return;
    reg = MOD_OFF[x] + 3u; level = r[3];
  }
  unsigned tl = 63u - ((vel > 127 ? 127u : vel) + 128u) * (63u - (level & 0x3f)) / 256u;
  opl_write(c->opl, 0x40 + reg, (level & 0xc0) | tl);
}
static void note_on(struct cmf *c, unsigned ch, unsigned note, unsigned vel) {
  velocity(c, ch, vel);
  if (c->rhythm && ch >= 11 && ch < 16) {
    unsigned bit = RBITS[ch - 11];
    pitch(c, RCH[ch - 11], note, 0);
    c->rhythm_keys &= (uint8_t)~bit; opl_write(c->opl, 0xbd, 0xe0u | c->rhythm_keys);
    c->rhythm_keys |= (uint8_t)bit; opl_write(c->opl, 0xbd, 0xe0u | c->rhythm_keys);
    c->notes[ch] = (int16_t)note; return;
  }
  if (ch >= voices(c)) return;
  if (c->notes[ch] >= 0) opl_write(c->opl, 0xb0 + ch, opl_read(c->opl, 0xb0 + ch) & ~0x20u);
  pitch(c, ch, note, 1); c->notes[ch] = (int16_t)note;
}
static void note_off(struct cmf *c, unsigned ch, unsigned note) {
  if (c->notes[ch] != (int)note) return;
  c->notes[ch] = -1;
  if (c->rhythm && ch >= 11 && ch < 16) {c->rhythm_keys &= (uint8_t)~RBITS[ch - 11]; opl_write(c->opl, 0xbd, 0xe0u | c->rhythm_keys); return;}
  if (ch < voices(c)) opl_write(c->opl, 0xb0 + ch, opl_read(c->opl, 0xb0 + ch) & ~0x20u);
}
void cmf_tick(struct cmf *c) {
  if (!c->playing) return;
  c->ticks++;
  if (c->off == 0) {c->wait = 1; paws(c);}
  c->wait--;
  unsigned guard = 0;
  while (c->wait <= 0 && guard++ < 100000) {
    unsigned b = byte(c, c->off);
    if ((b & 0xf0) == 0x90) {c->channel = b & 15; c->off++; c->funct = 9;}
    else if ((b & 0xf0) == 0x80) {c->channel = b & 15; c->off++; c->funct = 8;}
    unsigned a = byte(c, c->off);
    if (a < 0x80) {
      unsigned vel = byte(c, c->off + 1);
      if (c->funct == 8 || vel == 0) note_off(c, c->channel, a); else note_on(c, c->channel, a, vel);
      c->off += 2;
    } else switch (a & 0xf0) {
      case 0xa0: c->off += 3; break;
      case 0xb0: {if (byte(c, c->off + 1) == 0x67) set_mode(c, byte(c, c->off + 2));} c->off += 3; break;
      case 0xc0: program(c, a & 15, byte(c, c->off + 1)); c->off += 2; break;
      case 0xd0: c->off += 2; break;
      case 0xe0: c->off += 3; break;
      case 0xf0:
        if (a == 0xf7 || a == 0xf0) {c->off++; c->off += paws(c); c->off++;}
        else {all_off(c); c->off = 0;}
        break;
      default: all_off(c); c->off = 0;
    }
    c->wait = (int32_t)paws(c);
  }
}
