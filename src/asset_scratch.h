/* SPDX-License-Identifier: MIT; GMP and ZIP are mutually exclusive readers. */
#pragma once
#include <stdint.h>
#define GA_FILE_MAX (1024u * 1024u)
#define GA_PACKED_MAX (GA_FILE_MAX + 65536u)
#ifdef __cplusplus
extern "C" {
#endif
extern void (*ga_yield)(void); /* optional checkpoint while loading a menu picture */
extern uint8_t ga_packed[GA_PACKED_MAX],ga_whole[GA_FILE_MAX];
#ifdef __cplusplus
}
#endif
