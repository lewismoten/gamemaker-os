/* SPDX-License-Identifier: MIT; a game packed in one .GMP file (or a set of
 * volumes, NAME.GMP then NAME.G01, NAME.G02 ...), the Pocket Player's one-file
 * game from Game-Maker: Sandbox Edition (web/src/export/pocketFile.ts). It is
 * a "GMS1" bundle: "GMS1", a count, 40-byte entries sorted by name (name[32],
 * offset, length; little endian), then the data. The bundle's index is read
 * with the Pocket Player's own code (third_party/gmcore/gmsmall.h), so both
 * devices parse the same bytes. Game-Maker OS plays the game's DOS files,
 * stored as entries "dos/<NAME>" (the upper-case 8.3 name): each is its length
 * (4 bytes, little endian), then the raw deflate, inflated by puff
 * (third_party/puff). The Pocket Player's own entries (game.txt, <id>.cfg,
 * cards.pak, its packed files) are left alone.
 * Volumes: the bundle's offsets count through the volumes as one file, in
 * order, so an entry may start in one volume and end in the next. Today every
 * .GMP is one volume. Pure: bytes come through read_at, which the platform
 * gives (the kernel's /sd on the board, stdio in the host tests). */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GMP_VOLUMES_MAX 16u
#define GMP_ENTRIES_MAX 4096u        /* a bundle with more entries is refused */
#define GMP_FILE_MAX (1024u * 1024u) /* the largest DOS file it inflates (the largest in the Sandbox games is 658 KB) */
#define GMP_PACKED_MAX (GMP_FILE_MAX + 65536u) /* the largest stored entry (deflate can grow a file a little) */
/* Reads n bytes at off of volume v (0 is NAME.GMP, 1 NAME.G01 ...): 0 when all
   n were read. Never asked for bytes past size[v]. */
typedef int (*gmp_read_at_fn)(void *user, unsigned volume, uint32_t off, void *buf, uint32_t n);
struct gmp {
  gmp_read_at_fn read_at; void *user;
  unsigned volumes; uint32_t size[GMP_VOLUMES_MAX]; /* the caller fills these in before gmp_open */
  uint32_t total, entries;                         /* gmp_open fills in the rest */
  char gam[13];                                    /* the game's .GAM (its first dos/ *.GAM entry) */
  uint8_t pocket;                                  /* it also holds the Pocket Player's game (game.txt) */
};
/* n bytes at off of the volumes taken as one file: 0, or -1 past the end or on a read error. */
int gmp_read_at(const struct gmp *g, uint32_t off, void *buf, uint32_t n);
/* Checks the bundle's header and finds its .GAM: 0, or -1 when it is not a
   bundle, has too many entries, or holds no dos/ *.GAM entry. */
int gmp_open(struct gmp *g);
/* game.h's reader (user is the struct gmp): the DOS file `name`, any case,
   inflated; the bytes read (at most cap), or -1 when it is missing, too big
   or does not inflate to its stated length. */
int32_t gmp_read(void *user, const char *name, uint8_t *buf, uint32_t cap);
/* An entry as it is stored (not inflated), such as library.json, the game's
   gmp-library record: the bytes read (at most cap), or -1 when it is missing. */
int32_t gmp_entry(const struct gmp *g, const char *name, uint8_t *buf, uint32_t cap);
/* The Pocket Player's volume names: "NAME.GMP" for 0, else "NAME.G01" ...
   (base: the name without its extension, 1 to 8 characters); 0, or -1. */
int gmp_volume_name(const char *base, unsigned volume, char out[13]);
#ifdef __cplusplus
}
#endif
