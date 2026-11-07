/* SPDX-License-Identifier: MIT; setjmp.h for the board build, which links no C
 * library: puff (third_party/puff) longjmps out of its decoder when the input
 * runs out, within one call of puff(), which GCC's builtins cover. */
#pragma once
typedef void *jmp_buf[5];
#define setjmp(env) __builtin_setjmp(env)
#define longjmp(env, val) __builtin_longjmp(env, 1)
