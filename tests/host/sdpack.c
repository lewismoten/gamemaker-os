/* SPDX-License-Identifier: MIT; exercise the real board sdpack + gmp code. */
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sandbox/syscall.h>
#include "sdpack.h"
static char root[1024];
static struct {FILE *file; DIR *dir; char path[1200];} handles[32];
static uint64_t counts[6]; /* file opens, closes, seeks, bytes read, beats, reads */
static int seek_error;
static unsigned short_read;
static struct gmp *pack;
static void beat(void) {counts[4]++;}
int32_t sbx_open(const char *path, uint32_t n, uint32_t flags, uint64_t *fd) {
  (void)flags;
  for (unsigned i = 1; i < 32; i++) if (!handles[i].file && !handles[i].dir) {
    if (snprintf(handles[i].path, sizeof handles[i].path, "%s%.*s", root, (int)n, path) >= (int)sizeof handles[i].path) return -SBX_ENAMETOOLONG;
    struct stat st;
    if (stat(handles[i].path, &st)) return -SBX_ENOENT;
    if (S_ISDIR(st.st_mode)) handles[i].dir = opendir(handles[i].path);
    else {handles[i].file = fopen(handles[i].path, "rb"); if (handles[i].file) counts[0]++;}
    if (!handles[i].file && !handles[i].dir) return -SBX_EIO;
    *fd = i; return 0;
  }
  return -SBX_EMFILE;
}
int32_t sbx_close(uint64_t fd) {
  if (!fd || fd >= 32) return -SBX_EBADF;
  if (handles[fd].file) {fclose(handles[fd].file); counts[1]++;}
  if (handles[fd].dir) closedir(handles[fd].dir);
  handles[fd].file = 0; handles[fd].dir = 0; return 0;
}
int32_t sbx_read(uint64_t fd, void *out, uint32_t n, uint64_t *got) {
  if (fd >= 32 || !handles[fd].file) return -SBX_EBADF;
  if (short_read && n > short_read) n = short_read;
  *got = fread(out, 1, n, handles[fd].file); counts[3] += *got; counts[5]++;
  return ferror(handles[fd].file) ? -SBX_EIO : 0;
}
int32_t sbx_seek(uint64_t fd, int64_t offset, uint64_t *position) {
  counts[2]++;
  if (seek_error) {int e = seek_error; seek_error = 0; return -e;}
  if (fd >= 32 || !handles[fd].file) return -SBX_EBADF;
  struct stat st;
  if (stat(handles[fd].path, &st)) return -SBX_EIO;
  if (offset < 0 || offset > st.st_size) return -SBX_EINVAL;
  if (fseek(handles[fd].file, (long)offset, SEEK_SET)) return -SBX_EIO;
  *position = (uint64_t)offset; return 0;
}
int32_t sbx_getdents(uint64_t fd, void *out, uint32_t n, uint64_t *got) {
  if (fd >= 32 || !handles[fd].dir) return -SBX_EBADF;
  struct dirent *d; *got = 0;
  while ((d = readdir(handles[fd].dir))) {
    size_t len = strlen(d->d_name); unsigned bytes = (unsigned)(8 + len + 7) & ~7u;
    if (bytes > n - *got) return -SBX_EINVAL;
    char path[1500]; struct stat st;
    snprintf(path, sizeof path, "%s/%s", handles[fd].path, d->d_name);
    if (stat(path, &st)) return -SBX_EIO;
    sbx_dirent e = {(uint16_t)bytes, S_ISDIR(st.st_mode) ? SBX_DT_DIR : SBX_DT_FILE, (uint8_t)len, (uint32_t)st.st_size};
    memcpy((char *)out + *got, &e, sizeof e); memcpy((char *)out + *got + 8, d->d_name, len);
    memset((char *)out + *got + 8 + len, 0, bytes - 8 - len); *got += bytes;
  }
  return 0;
}
void test_sp_setup(const char *path, unsigned major, unsigned minor) {
  /* sp_open closes its own handle; keep it alive until then. */
  snprintf(root, sizeof root, "%s", path); seek_error = 0; short_read = 0;
  sp_set_abi(major, minor);
  sp_beat = beat; memset(counts, 0, sizeof counts); pack = 0;
}
unsigned test_sp_list(uint8_t folder, struct sp_pack *p, unsigned max) {return sp_list(folder, p, max, 0);}
int test_sp_open(const struct sp_pack *p) {pack = sp_open(p); return pack != 0;}
int test_sp_read(uint32_t off, void *out, uint32_t n) {return gmp_read_at(pack, off, out, n);}
int test_sp_volume(unsigned v, uint32_t off, void *out, uint32_t n) {return pack->read_at(pack->user, v, off, out, n);}
int32_t test_sp_dos(const char *name, void *out, uint32_t cap) {return gmp_read(pack, name, out, cap);}
int32_t test_sp_entry(const char *name, void *out, uint32_t cap) {return gmp_entry(pack, name, out, cap);}
uint64_t test_sp_count(unsigned i) {return i < 6 ? counts[i] : 0;}
void test_sp_error(int e) {seek_error = e;}
void test_sp_short(unsigned n) {short_read = n;}
