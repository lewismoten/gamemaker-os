/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten */
#include "pcspeaker.h"
#include <string.h>
uint32_t pc_tick_frames(struct pc_tick_clock *clock) {
    /* GS_TICK_NS10 unchanged; carry fraction, never rounded fixed blocks. */
    uint64_t scaled=clock->remainder+549254157ull*PC_RATE;
    uint32_t frames=(uint32_t)(scaled/10000000000ull);
    clock->remainder=scaled%10000000000ull;
    return frames;
}

void pc_init(struct pcspeaker *p) {memset(p,0,sizeof(*p));}
void pc_stop(struct pcspeaker *p) {pc_init(p);}
int pc_start(struct pcspeaker *p,const void *snd,size_t bytes,unsigned tune) {
 if (!snd || (bytes!=2160 && bytes!=2430) || tune>=30) return -22;
 const uint8_t *src=(const uint8_t *)snd+tune*PC_STEPS*2;
 pc_init(p);
 for (unsigned i=0;i<PC_STEPS;i++)
     p->hz[i]=(uint16_t)((unsigned)src[2*i]|(unsigned)src[2*i+1]<<8);
 p->active=1;
 return 0;
}
int pc_render(struct pcspeaker *p,void *out,size_t frames) {
 if (frames>PC_MAX_FRAMES || (frames && !out)) return -22;
 uint8_t *dst=out;
 for (size_t i=0;i<frames;i++) {
     int sample=0;
     if (p->active) {
         unsigned hz=p->hz[p->step];
         if (hz>=20) {
             if (hz>20000) hz=20000;
             sample=p->phase<PC_RATE/2?PC_AMPLITUDE:-PC_AMPLITUDE;
             p->phase+=hz;
             if (p->phase>=PC_RATE) p->phase-=PC_RATE;
         }
         p->clock+=PC_PIT;
         if (p->clock>=PC_RATE*PC_DIVISOR) {
             p->clock-=PC_RATE*PC_DIVISOR;
             if (++p->step==PC_STEPS) p->active=0;
         }
     }
     uint16_t v=(uint16_t)sample;
     dst[i*4]=dst[i*4+2]=(uint8_t)v;
     dst[i*4+1]=dst[i*4+3]=(uint8_t)(v>>8);
 }
 return 0;
}
