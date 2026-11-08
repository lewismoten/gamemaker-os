/* SPDX-License-Identifier: MIT; optional real-kernel host smoke transport.
 * Same SDK records, but a callback instead of ARM svc. No ARM execution. */
#include <sandbox/syscall.h>
static int32_t (*dispatch)(uint32_t, void *, uint32_t);
void sdk_bridge(int32_t (*fn)(uint32_t, void *, uint32_t)) {dispatch = fn;}
#define H(T) {sizeof(T), 0, 0, 0}
int32_t sbx_open(const char *p, uint32_t n, uint32_t flags, uint64_t *fd) {
  sbx_open_rec r = {H(sbx_open_rec), (uintptr_t)p, n, flags, 0, 0};
  int32_t s = dispatch(SBX_CALL_OPEN, &r, sizeof r); if (!s) *fd = r.fd; return s;
}
int32_t sbx_read(uint64_t fd, void *p, uint32_t n, uint64_t *done) {
  sbx_io_rec r = {H(sbx_io_rec), fd, (uintptr_t)p, n, 0};
  int32_t s = dispatch(SBX_CALL_READ, &r, sizeof r); if (!s) *done = r.transferred; return s;
}
int32_t sbx_getdents(uint64_t fd, void *p, uint32_t n, uint64_t *done) {
  sbx_dirents_rec r = {H(sbx_dirents_rec), fd, (uintptr_t)p, n, 0};
  int32_t s = dispatch(SBX_CALL_GETDENTS, &r, sizeof r); if (!s) *done = r.transferred; return s;
}
int32_t sbx_close(uint64_t fd) {
  sbx_close_rec r = {H(sbx_close_rec), fd}; return dispatch(SBX_CALL_CLOSE, &r, sizeof r);
}
int32_t sbx_seek(uint64_t fd, int64_t off, uint64_t *position) {
  sbx_seek_rec r = {H(sbx_seek_rec), fd, off, SBX_SEEK_SET, 0};
  int32_t s = dispatch(SBX_CALL_SEEK, &r, sizeof r); if (!s) *position = r.position; return s;
}
