/* SPDX-License-Identifier: MIT; bounded, read-only ZIP transport. No extraction. */
#pragma once
#include <stdint.h>
#define GZ_ENTRIES_MAX 1024u
#define GZ_PATH_MAX 255u
#define GZ_FILE_MAX (1024u * 1024u)
#define GZ_PACKED_MAX (GZ_FILE_MAX + 65536u)
typedef int (*gz_read_at_fn)(void *, uint32_t, void *, uint32_t);
struct gz_entry {uint32_t name_at,hash,at,packed,raw,crc;uint16_t name_bytes,method;};
struct gz_zip {
  gz_read_at_fn read_at; void *user; uint32_t size, count;
  char gam[256], root[256];
  struct gz_entry entries[GZ_ENTRIES_MAX];
};
uint32_t gz_bytes(void);
int gz_open(struct gz_zip *, gz_read_at_fn, void *, uint32_t);
const char *gz_gam(const struct gz_zip *);
int32_t gz_read(void *, const char *, uint8_t *, uint32_t);
/* Shared sandbox-root path policy; rejects absolute/drive/traversal/empty components. */
int gz_path(const char *, char out[256], int directory);
int gz_noise(const char *);
