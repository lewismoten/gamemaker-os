/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten */
#include "pcspeaker.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    struct pcspeaker p;
    uint8_t snd[2430]={0},out[7680];
    for(unsigned i=0;i<36;i++) {snd[i*2]=0xb8;snd[i*2+1]=1;} /* 440 LE */
    pc_init(&p);
    assert(pc_start(&p,snd,2160,0)==0);
    assert(pc_render(&p,out,100)==0);
    assert(out[0] || out[1]);
    for(unsigned i=0;i<100;i++) {
        assert(out[i*4]==out[i*4+2] && out[i*4+1]==out[i*4+3]);
        int v=(int16_t)((unsigned)out[i*4]|(unsigned)out[i*4+1]<<8);
        assert(v==PC_AMPLITUDE || v==-PC_AMPLITUDE);
    }
    puts("PASS LE tune, stereo phase, amplitude");
    struct pcspeaker before=p;
    assert(pc_start(&p,snd,2159,0)==-22);
    assert(pc_start(&p,snd,2161,0)==-22);
    assert(pc_start(&p,snd,2429,0)==-22);
    assert(pc_start(&p,snd,2431,0)==-22);
    assert(pc_start(&p,snd,2430,30)==-22);
    assert(pc_start(&p,NULL,2160,0)==-22);
    assert(pc_render(&p,NULL,1)==-22);
    assert(pc_render(&p,out,1921)==-22);
    assert(pc_render(&p,NULL,0)==0);
    assert(memcmp(&before,&p,sizeof(p))==0);
    for (unsigned tune=0;tune<30;tune++) {
        for(unsigned i=0;i<36;i++) {unsigned h=20+tune*36+i;
            snd[tune*72+i*2]=(uint8_t)h; snd[tune*72+i*2+1]=(uint8_t)(h>>8);}
        assert(pc_start(&p,snd,2430,tune)==0);
        for(unsigned i=0;i<36;i++) assert(p.hz[i]==20+tune*36+i);
    }
    puts("PASS 30x36 LE layout, exact sizes, atomic errors");
    for(unsigned h=0;h<4;h++) {
        unsigned hz=(unsigned[]){0,19,20000,65535}[h];
        for(unsigned i=0;i<36;i++) {snd[i*2]=(uint8_t)hz;snd[i*2+1]=(uint8_t)(hz>>8);}
        assert(pc_start(&p,snd,2160,0)==0);
        for(unsigned n=0;n<13000;n++) {
            assert(pc_render(&p,out,1)==0);
            unsigned step=(unsigned)((uint64_t)n*PC_PIT/(48000ull*8191));
            int expected=0;
            if(step<36 && hz>=20) {
                unsigned f=hz>20000?20000:hz;
                expected=((uint64_t)n*f%48000)<24000?3146:-3146;
            }
            int sample=(int16_t)((unsigned)out[0]|(unsigned)out[1]<<8);
            assert(sample==expected);
            assert(p.step==(unsigned)(((uint64_t)(n+1)*PC_PIT/(48000ull*8191))<36?
                         ((uint64_t)(n+1)*PC_PIT/(48000ull*8191)):36));
        }
        assert(!p.active && p.step==36);
    }
    puts("PASS zero/low/high/clamp, rational 36-step oracle, output bounds");
    for(unsigned i=0;i<36;i++) {snd[i*2]=0xb8;snd[i*2+1]=1;}
    struct pcspeaker a,b;
    uint8_t one[7680],split[7680];
    assert(pc_start(&a,snd,2160,0)==0); b=a;
    for(unsigned block=0;block<1000;block++) {
        assert(pc_render(&a,one,1920)==0);
        assert(pc_render(&b,split,117)==0);
        assert(pc_render(&b,split+117*4,1803)==0);
        assert(!memcmp(one,split,sizeof(one)) && !memcmp(&a,&b,sizeof(a)));
    }
    pc_start(&p,snd,2160,0); pc_render(&p,out,77);
    pc_start(&p,snd,2160,0); pc_start(&a,snd,2160,0);
    pc_render(&p,one,1920);pc_render(&a,split,1920);
    assert(!memcmp(one,split,sizeof(one)));
    pc_stop(&p);pc_render(&p,one,1920);
    for(unsigned i=0;i<sizeof(one);i++) assert(one[i]==0);
    puts("PASS split-buffer/long idle continuity, retrigger and stop");
    struct pc_tick_clock clock={0}; uint64_t total=0;
    for(unsigned tick=0;tick<1000000;tick++) {
        total+=pc_tick_frames(&clock);
        assert(total==(uint64_t)(tick+1)*2636+((uint64_t)(tick+1)*4199536000ull)/10000000000ull);
        assert(clock.remainder<10000000000ull);
    }
    puts("PASS 1000000 literal GMOS tick fractional-accounting oracle");
    return 0;
}
