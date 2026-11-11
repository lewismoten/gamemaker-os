/* SPDX-License-Identifier: MIT; sdpack.h: .GMP volume sets on the card. */
#include <sandbox/syscall.h>
#include "sdpack.h"
static uint8_t list[2048], skip[64 * 1024], table[8 + 40 * GMP_ENTRIES_MAX];
/* The open pack: its file (at pos of volume), and its header and index read once (table_n bytes). */
static struct {struct sp_pack pack; struct gmp g; uint64_t fd; int open; unsigned volume; uint32_t pos, table_n;} cur;
void (*sp_beat)(void);
uint32_t sp_io_errors;
static int have_seek; /* opt in only after inspecting the running kernel */
static int have_long;
void sp_set_abi(unsigned major, unsigned minor) {have_seek = major == 1 && minor >= 3;have_long=major==1&&minor>=5;}
static char upper(char c) {return c >= 'a' && c <= 'z' ? (char)(c - 32) : c;}
void sp_path(const struct sp_pack *p, unsigned volume, char out[40]) {
  const char *f = p->folder == SP_FOLDER_POCKET ? "/sd/POCKET/" : "/sd/GAMES/";
  char name[13]; unsigned k = 0;
  if (gmp_volume_name(p->base, volume, name)) name[0] = 0;
  for (; *f; f++) out[k++] = *f;
  for (const char *s = name; *s; s++) out[k++] = *s;
  out[k] = 0;
}
/* Is n (len bytes) BASE.GMP or BASE.Gnn (nn 01..15)? Its base into base[9] and the volume, else -1. */
static int volume_of(const char *n, unsigned len, char base[9]) {
  unsigned dot = 0;
  while (dot < len && n[dot] != '.') dot++;
  if (!dot || dot > 8 || len != dot + 4 || upper(n[dot + 1]) != 'G') return -1;
  for (unsigned i = 0; i < dot; i++) base[i] = upper(n[i]);
  base[dot] = 0;
  char a = upper(n[dot + 2]), b = upper(n[dot + 3]);
  if (a == 'M' && b == 'P') return 0;
  if (a < '0' || a > '9' || b < '0' || b > '9') return -1;
  int v = (a - '0') * 10 + (b - '0');
  return v >= 1 && v < (int)GMP_VOLUMES_MAX ? v : -1;
}
static int same(const char *a, const char *b) {while (*a && *a == *b) {a++; b++;} return *a == *b;}
/* Use long names directly for ordinary volumes; only a renamed long GMP
 * needs a lazy alias cursor. This avoids doubling every directory scan. */
struct aliases {uint64_t fd,got;uint32_t at,index;int opened;};
static int alias_volume(const char *path,unsigned pn,unsigned ordinal,const sbx_dirent *entry,struct aliases *c,char base[9]) {
  _Alignas(8) static uint8_t names[2048];
  if(!c->opened){if(sbx_open(path,pn,SBX_O_RDONLY,&c->fd))return -2;c->opened=1;}
  while(c->index<ordinal){
    if(c->at==c->got){c->at=0;if(sbx_getdents(c->fd,names,sizeof names,&c->got)||!c->got)return -2;}
    if(c->got-c->at<8)return -2;
    const sbx_dirent *e=(const sbx_dirent *)(const void *)(names+c->at);
    if(e->length<8||e->length>c->got-c->at||e->name_bytes>e->length-8)return -2;
    c->at+=e->length;c->index++;
    if(c->index==ordinal){if(e->type!=entry->type||e->size!=entry->size)return -2;return volume_of((const char *)e+8,e->name_bytes,base);}
  }
  return -2;
}
unsigned sp_list(uint8_t folder, struct sp_pack *packs, unsigned max, void (*say)(const char *)) {
  static struct {char base[9]; uint8_t v; uint32_t size;} vols[256];
  unsigned nv = 0, np = 0; uint64_t fd, got;
  const char *path = folder == SP_FOLDER_POCKET ? "/sd/POCKET" : "/sd/GAMES";
  if (sbx_open(path, folder == SP_FOLDER_POCKET ? 10 : 9, SBX_O_RDONLY, &fd)) return 0;
  struct aliases aliases={0};unsigned ordinal=0,head=have_long?24:8;int failed=0,status;
  while (!(status=have_long?sbx_getdents_long_meta(fd,list,sizeof list,&got):sbx_getdents(fd, list, sizeof list, &got)) && got) {
    for (uint32_t at = 0; at < got;) {
      const sbx_dirent *e = (const sbx_dirent *)(const void *)(list + at);
      if(got-at<head||e->length<head||e->length>got-at||e->name_bytes>e->length-head){failed=1;break;}
      const char *n = (const char *)(list + at + head);ordinal++;
      if(e->name_bytes>=2&&n[0]=='.'&&n[1]=='_'){at+=e->length;continue;}
      char base[9]={0}; int v = e->type == SBX_DT_FILE ? volume_of(n, e->name_bytes, base) : -1;
      unsigned nb=e->name_bytes;
      if(have_long&&v<0&&e->type==SBX_DT_FILE&&nb>4&&n[nb-4]=='.'&&upper(n[nb-3])=='G'&&
         ((upper(n[nb-2])=='M'&&upper(n[nb-1])=='P')||(n[nb-2]>='0'&&n[nb-2]<='9'&&n[nb-1]>='0'&&n[nb-1]<='9'))){
        v=alias_volume(path,folder==SP_FOLDER_POCKET?10:9,ordinal,e,&aliases,base);if(v==-2){failed=1;break;}
      }
      if (v == 0 && np < max) {
        struct sp_pack *p = &packs[np++];
        p->folder = folder; p->volumes = 1; p->size[0] = e->size;
        for (unsigned i = 0; i < 9; i++) p->base[i] = base[i];
      } else if (v > 0 && nv < sizeof vols / sizeof vols[0]) {
        for (unsigned i = 0; i < 9; i++) vols[nv].base[i] = base[i];
        vols[nv].v = (uint8_t)v; vols[nv++].size = e->size;
      }
      at += e->length;
    }
    if(failed)break;
  }
  sbx_close(fd);
  if(aliases.opened)sbx_close(aliases.fd);
  if(failed||status){sp_io_errors++;return 0;}
  for (unsigned i = 0; i < np; i++) /* further volumes, in order while there is no gap */
    for (int found = 1; found && packs[i].volumes < GMP_VOLUMES_MAX;) {
      found = 0;
      for (unsigned j = 0; j < nv; j++)
        if (vols[j].v == packs[i].volumes && same(vols[j].base, packs[i].base)) {packs[i].size[packs[i].volumes++] = vols[j].size; found = 1; break;}
    }
  if (np == max && say) say("GMOS packs: the list is full; the rest are skipped");
  return np;
}
static void close_cur(void) {if (cur.open) sbx_close(cur.fd); cur.open = 0;}
static int open_volume(unsigned v) {
  char path[40]; close_cur(); sp_path(&cur.pack, v, path);
  unsigned k = 0; while (path[k]) k++;
  if (sbx_open(path, k, SBX_O_RDONLY, &cur.fd)) {sp_io_errors++;return -1;}
  cur.open = 1; cur.volume = v; cur.pos = 0;
  return 0;
}
static int read_at(void *user, unsigned v, uint32_t off, void *buf, uint32_t n) {
  (void)user;
  if (v >= cur.pack.volumes || v >= GMP_VOLUMES_MAX ||
      off > cur.pack.size[v] || n > cur.pack.size[v] - off) return -1;
  if (!v && off <= cur.table_n && n <= cur.table_n - off) { /* the index, from memory */
    for (uint32_t i = 0; i < n; i++) ((uint8_t *)buf)[i] = table[off + i];
    return 0;
  }
  if (!cur.open || cur.volume != v) {
    if (open_volume(v)) return -1;
  }
  if (have_seek && off != cur.pos) {
    uint64_t position;
    int32_t status = sbx_seek(cur.fd, off, &position);
    if (!status) cur.pos = (uint32_t)position;
    else if (status == -SBX_ENOSYS) have_seek = 0; /* defensive old-kernel fallback */
    else {sp_io_errors++;return -1;} /* failed seek leaves the actual and cached cursor unchanged */
  }
  if (off < cur.pos && open_volume(v)) return -1; /* ABI <= 1.2: rewind by reopening */
  uint64_t got;
  while (cur.pos < off) { /* forward to off */
    uint32_t take = off - cur.pos < sizeof skip ? off - cur.pos : (uint32_t)sizeof skip;
    if (sbx_read(cur.fd, skip, take, &got) || !got || got > take) {sp_io_errors++;close_cur(); return -1;}
    cur.pos += (uint32_t)got;
    if (sp_beat) sp_beat();
  }
  uint8_t *out = (uint8_t *)buf;
  while (n) {
    uint32_t take = n < 16384u ? n : 16384u;
    if (sbx_read(cur.fd, out, take, &got) || !got || got > take) {sp_io_errors++;close_cur(); return -1;}
    out += got; n -= (uint32_t)got; cur.pos += (uint32_t)got;
    if (sp_beat) sp_beat();
  }
  return 0;
}
struct gmp *sp_open(const struct sp_pack *p) {
  close_cur();
  cur.pack = *p; cur.table_n = 0;
  /* the header, then the whole index (a bigger one than gmp.h takes is refused there) */
  uint32_t want = 8;
  if (read_at(0, 0, 0, table, 8)) return 0;
  uint32_t count = table[4] | table[5] << 8 | table[6] << 16 | (uint32_t)table[7] << 24;
  if (count <= GMP_ENTRIES_MAX && 8 + count * 40 <= p->size[0]) want = 8 + count * 40;
  if (want > 8 && read_at(0, 0, 8, table + 8, want - 8)) return 0;
  cur.table_n = want;
  cur.g.read_at = read_at; cur.g.user = 0; cur.g.volumes = p->volumes;
  for (unsigned v = 0; v < GMP_VOLUMES_MAX; v++) cur.g.size[v] = v < p->volumes ? p->size[v] : 0;
  return gmp_open(&cur.g) ? 0 : &cur.g;
}
