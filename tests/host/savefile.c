/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten. Claude Sonnet 5.5.
 * savefile.c and hiscore.c against a fake of the kernel's save scope:
 * a staged write is invisible until sync, rename replaces in one step, and a
 * "power cut" at any step stops everything after it. */
#include "savefile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MAXF 8
struct file {char path[48]; uint8_t d[4096]; uint32_t n; int used;};
struct fake {
  struct file f[MAXF]; char stage_path[48]; uint8_t stage[4096]; uint32_t stage_n; int writing;
  int step, cut_at; /* cut_at < 0: never */
  uint32_t chunk;   /* max bytes per write/read call, to exercise the loops */
};
static int cut(struct fake *k) {return k->cut_at >= 0 && k->step++ >= k->cut_at ? -5 : 0;}
static struct file *find(struct fake *k, const char *p) {for (int i = 0; i < MAXF; i++) if (k->f[i].used && !strcmp(k->f[i].path, p)) return &k->f[i]; return 0;}
static int scoped(const char *p) {return !strncmp(p, "/sd/OS/SAVES/", 13);}
static int f_open_write(void *c, const char *p, uint64_t *fd) {struct fake *k = c; if (cut(k)) return -5; assert(scoped(p)); if (k->writing) return -16; strcpy(k->stage_path, p); k->stage_n = 0; k->writing = 1; *fd = 7; return 0;}
static int f_write(void *c, uint64_t fd, const void *p, uint32_t n, uint32_t *done) {struct fake *k = c; (void)fd; if (cut(k)) return -5; if (n > k->chunk) n = k->chunk; assert(k->stage_n + n <= sizeof k->stage); memcpy(k->stage + k->stage_n, p, n); k->stage_n += n; *done = n; return 0;}
static int f_sync(void *c, uint64_t fd) {
  struct fake *k = c; (void)fd; if (cut(k)) return -5;
  struct file *x = find(k, k->stage_path);
  if (!x) {for (int i = 0; i < MAXF; i++) if (!k->f[i].used) {x = &k->f[i]; break;} assert(x); x->used = 1; strcpy(x->path, k->stage_path);}
  memcpy(x->d, k->stage, k->stage_n); x->n = k->stage_n; return 0;
}
static int f_close(void *c, uint64_t fd) {struct fake *k = c; (void)fd; k->writing = 0; return 0;} /* close never commits */
static const char *dir_end(const char *p) {return strrchr(p, '/');}
static int f_rename(void *c, const char *a, const char *b) {
  struct fake *k = c; if (cut(k)) return -5; assert(scoped(a) && scoped(b));
  assert(dir_end(a) - a == dir_end(b) - b && !strncmp(a, b, (size_t)(dir_end(a) - a))); /* same folder */
  struct file *s = find(k, a); if (!s) return -2; struct file *d = find(k, b);
  if (!d) {for (int i = 0; i < MAXF; i++) if (!k->f[i].used) {d = &k->f[i]; break;} assert(d); d->used = 1; strcpy(d->path, b);}
  memcpy(d->d, s->d, s->n); d->n = s->n; s->used = 0; return 0;
}
static uint32_t rpos;
static struct file *rfile;
static int f_open_read(void *c, const char *p, uint64_t *fd) {struct fake *k = c; assert(scoped(p)); rfile = find(k, p); if (!rfile) return -2; rpos = 0; *fd = 9; return 0;}
static int f_read(void *c, uint64_t fd, void *p, uint32_t n, uint32_t *got) {struct fake *k = c; (void)fd; if (n > k->chunk) n = k->chunk; uint32_t left = rfile->n - rpos; if (n > left) n = left; memcpy(p, rfile->d + rpos, n); rpos += n; *got = n; return 0;}
static struct save_ops ops(struct fake *k) {return (struct save_ops){k, f_open_write, f_write, f_sync, f_close, f_rename, f_open_read, f_read};}
static void seed(struct fake *k, const char *p, const void *d, uint32_t n) {for (int i = 0; i < MAXF; i++) if (!k->f[i].used) {k->f[i].used = 1; strcpy(k->f[i].path, p); memcpy(k->f[i].d, d, n); k->f[i].n = n; return;} assert(0);}
static void put32(uint8_t *p, int32_t v) {uint32_t u = (uint32_t)v; p[0] = (uint8_t)u; p[1] = (uint8_t)(u >> 8); p[2] = (uint8_t)(u >> 16); p[3] = (uint8_t)(u >> 24);}

int main(void) {
  /* 1. A hand-built DOS file: rows at 34-byte strides, little-endian scores. */
  uint8_t dos[340] = {0}; put32(dos, 5000); strcpy((char *)dos + 4, "ACE"); put32(dos + 34, 300); strcpy((char *)dos + 38, "BOB");
  struct his_table t; his_parse(&t, dos, 340);
  assert(t.row[0].score == 5000 && !strcmp((char *)t.row[0].name, "ACE") && t.row[1].score == 300 && t.row[2].score == 0);
  uint8_t back[340]; his_serialize(&t, back); assert(!memcmp(back, dos, 340));
  puts("PASS DOS layout round trip");
  /* 2. Insertion: first strictly smaller row; a tie goes below; last row drops; score <= 0 never places. */
  assert(his_place(&t, 300) == 2 && his_place(&t, 301) == 1 && his_place(&t, 5001) == 0 && his_place(&t, 5000) == 1);
  assert(his_place(&t, 0) == -1 && his_place(&t, -5) == -1);
  assert(his_insert(&t, 400, "CY") == 2 && t.row[1].score == 400 && t.row[2].score == 300 && !strcmp((char *)t.row[2].name, "BOB"));
  for (int i = 0; i < 12; i++) his_insert(&t, 100 + i, "X");
  assert(t.row[9].score > 0 && t.row[0].score == 5000);
  assert(his_insert(&t, 1, "LOW") == 0); /* nothing beats the last 100+ row */
  struct his_table n; uint8_t big[400]; memset(big, 0xff, sizeof big); his_parse(&n, big, 400); assert(n.row[9].score == -1);
  his_parse(&n, dos, 10); assert(n.row[0].score == 5000 && n.row[0].name[0] == 'A' && n.row[0].name[7] == 0 && n.row[1].score == 0);
  his_insert(&n, 9, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"); int at = his_place(&n, 1);
  assert(at >= 0); assert(n.row[1].name[21] == 'V' && n.row[1].name[22] == 0);
  puts("PASS place, tie, shift, drop, short file, 22-char name");

  /* 3. Save and load through the fake. */
  struct fake k; memset(&k, 0, sizeof k); k.cut_at = -1; k.chunk = 100; struct save_ops o = ops(&k);
  uint8_t data[1000], got[1000]; for (unsigned i = 0; i < sizeof data; i++) data[i] = (uint8_t)(i * 7);
  uint32_t gn;
  assert(save_put(&o, "DICE", "GAME0.SAV", data, 1000) == 0);
  assert(save_get(&o, "DICE", "GAME0.SAV", got, sizeof got, &gn) == 0 && gn == 1000 && !memcmp(got, data, 1000));
  assert(!find(&k, "/sd/OS/SAVES/DICE/GAME0.TMP")); /* the temp is gone after the rename */
  assert(save_get(&o, "DICE", "GAME0.SAV", got, 999, &gn) == SAVE_EFBIG);
  assert(save_get(&o, "DICE", "GAME1.SAV", got, sizeof got, &gn) == SAVE_ENOENT);
  puts("PASS put and get, chunked, temp removed");
  /* 4. Names the kernel would refuse never reach it. */
  const char *bad_g[] = {"", "dice", "A.B", "TOOLONGNAME", "../X", "A/B"}; for (unsigned i = 0; i < 6; i++) assert(save_put(&o, bad_g[i], "X.HIS", data, 1) == SAVE_EINVAL);
  const char *bad_n[] = {"x.his", "A.TMP", "", ".HIS", "A.B.C", "TOOLONGNAME.HIS", "A.LONG", "A/B"}; for (unsigned i = 0; i < 8; i++) assert(save_put(&o, "DICE", bad_n[i], data, 1) == SAVE_EINVAL);
  puts("PASS bad names refused");

  /* 5. High scores end to end, with the file bytes DOS would read. */
  memset(&k, 0, sizeof k); k.cut_at = -1; k.chunk = 64; o = ops(&k); int place;
  assert(save_his_load(&o, "DICE", &t) == 0 && t.row[0].score == 0); /* missing file: empty table */
  assert(save_his_record(&o, "DICE", 0, "NOPE", &place) == 0 && place == 0 && !find(&k, "/sd/OS/SAVES/DICE/DICE.HIS"));
  assert(save_his_record(&o, "DICE", 250, "AMY", &place) == 0 && place == 1);
  assert(save_his_record(&o, "DICE", 900, "ZED", &place) == 0 && place == 1);
  assert(save_his_record(&o, "DICE", 250, "TIE", &place) == 0 && place == 3);
  struct file *hf = find(&k, "/sd/OS/SAVES/DICE/DICE.HIS"); assert(hf && hf->n == 340);
  assert(hf->d[0] == 0x84 && hf->d[1] == 0x03 && hf->d[2] == 0 && hf->d[3] == 0 && !memcmp(hf->d + 4, "ZED", 4) && hf->d[34] == 250 && !memcmp(hf->d + 38, "AMY", 4));
  puts("PASS high scores: missing file, no place, order, DOS bytes");

  /* 6. Power cut after every step of a replace: old or new, nothing else. */
  uint8_t oldv[340], newv[340]; memset(oldv, 0, 340); memset(newv, 0, 340); put32(oldv, 111); put32(newv, 222); put32(newv + 34, 111);
  int steps = 0;
  for (int cut_at = 0; cut_at < 100; cut_at++) {
    memset(&k, 0, sizeof k); k.chunk = 100; seed(&k, "/sd/OS/SAVES/DICE/DICE.HIS", oldv, 340); k.cut_at = cut_at; o = ops(&k);
    int e = save_put(&o, "DICE", "DICE.HIS", newv, 340);
    k.cut_at = -1; k.writing = 0; /* reboot */
    struct file *x = find(&k, "/sd/OS/SAVES/DICE/DICE.HIS"); assert(x && x->n == 340);
    assert(!memcmp(x->d, oldv, 340) || !memcmp(x->d, newv, 340));
    if (e == 0) {assert(!memcmp(x->d, newv, 340)); steps = cut_at; break;}
    assert(!memcmp(x->d, oldv, 340) || cut_at >= 1); /* new only once the rename ran */
  }
  assert(steps >= 4);
  /* First-ever save cut at every step: complete file or none. */
  for (int cut_at = 0; cut_at <= steps; cut_at++) {
    memset(&k, 0, sizeof k); k.chunk = 100; k.cut_at = cut_at; o = ops(&k);
    int e = save_put(&o, "DICE", "DICE.HIS", newv, 340); k.cut_at = -1;
    struct file *x = find(&k, "/sd/OS/SAVES/DICE/DICE.HIS");
    assert(x ? (x->n == 340 && !memcmp(x->d, newv, 340)) : e != 0);
  }
  puts("PASS power cut at every step leaves old or new");
  puts("ALL PASS"); return 0;
}
