/* SPDX-License-Identifier: MIT; Autodesk Animator .FLI (and .FLC) animations
 * for the pages between scenes, played as PLAYGAME's FLI::View plays them:
 * the first `frames` frames once, `speed` vertical retraces (1/70 s) apart,
 * over whatever the screen holds. */
#pragma once
#include <stdint.h>
#define GP_FLI_PIXELS 64000u /* the largest picture played: 320 x 200 */
struct gp_fli {
  const uint8_t *d; uint32_t n;
  uint32_t w, h, frames, speed; /* speed: 1/70 s a frame (an FLC's milliseconds converted) */
  uint32_t next;                /* where the search for the next frame record starts */
  uint32_t found;               /* frame records found so far */
};
/* Reads the header; 0, or -1 when it is not an FLI or FLC no bigger than 320 x 200. */
int gp_fli_open(struct gp_fli *f, const uint8_t *d, uint32_t n);
/* Applies the next frame to the canvas (w x h indices, a 6-bit palette).
   Returns 1 when it changed the palette, 0 otherwise (also when the file has no
   more frame records: the frame shows unchanged). */
int gp_fli_frame(struct gp_fli *f, uint8_t *px, uint8_t *pal);
