/* SPDX-License-Identifier: MIT; bounded, little-endian scan cache codec.
 * Not an authority for discovery. Look up only current live source paths and
 * exact metadata keys; replay candidates in current native encounter order.
 * Caller-owned old/output buffers must be disjoint; no allocation or syscalls. */
#pragma once
#include <stdint.h>
#define GC_CAPACITY (256u * 1024u)
struct gc_cache {
 const uint8_t *old; uint8_t *out;
 uint32_t oldbytes,used,capacity,valid,overflow;
};
void gc_init(struct gc_cache *,const void *old,uint32_t bytes,void *out,uint32_t capacity);
const uint8_t *gc_get(const struct gc_cache *,const char *path,const void *key,uint32_t keybytes,uint32_t *bytes);
int gc_add(struct gc_cache *,const char *path,const void *key,uint32_t keybytes,const void *value,uint32_t bytes);
uint32_t gc_finish(struct gc_cache *); /* 0 on overflow: do not publish */
