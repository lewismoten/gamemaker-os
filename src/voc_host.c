/* SPDX-License-Identifier: MIT; Copyright (c) 2026 Lewis Moten
 * UNLINKED. Original bounded C implementation by Hermes GPT-6.1 Sol.
 * PCM/block behavior: Sandbox MIT formats/voc.ts; linear interpolation:
 * audio/engine.ts vocBuffer. RSD SOUNDC.C supplies stop/retrigger semantics.
 */
#include "voc_host.h"
#include <string.h>
static unsigned le16(const uint8_t *s) {return s[0]|(unsigned)s[1]<<8;}
static uint32_t le24(const uint8_t *s) {return le16(s)|(uint32_t)s[2]<<16;}
void voc_stop(struct voc_host *v) {
    v->bytes=v->pos=v->visits=v->count=v->samples=v->rate=0;
    v->repeat_start=v->repeat_left=v->repeating=v->have_data=0;
    v->frame=v->frames=0;v->status=0;
}
int voc_begin(struct voc_host *v,const void *source,size_t bytes) {
    voc_stop(v);
    if(bytes>VOC_SOURCE_MAX)return v->status=VOC_LIMIT;
    if(!source || bytes<26)return v->status=VOC_INVALID;
    const uint8_t *s=source;
    if(memcmp(s,"Creative Voice File\032",20)!=0 || le16(s+20)<26 ||
       le16(s+20)>=bytes || le16(s+24)!=((~le16(s+22)+0x1234u)&0xffffu))
        return v->status=VOC_INVALID;
    memcpy(v->source,source,bytes);
    v->bytes=(uint32_t)bytes;v->pos=le16(v->source+20);
    v->status=VOC_PENDING;return 0;
}
int voc_prepare(struct voc_host *v,unsigned budget) {
    if(!budget || budget>VOC_PREPARE_MAX)return VOC_INVALID;
    while (budget-- && v->status==VOC_PENDING) {
        if(v->visits==VOC_VISITS_MAX)return v->status=VOC_LIMIT;
        if(v->pos>=v->bytes)return v->status=VOC_INVALID;
        uint32_t p=v->pos;unsigned type=v->source[p++];v->visits++;
        if (!type) {
            if(p!=v->bytes || !v->samples || !v->rate || v->repeating)return v->status=VOC_INVALID;
            v->frames=(uint32_t)((uint64_t)v->samples*PC_RATE/v->rate);
            v->status=0;break;
        }
        if(v->bytes-p<3)return v->status=VOC_INVALID;
        uint32_t size=le24(v->source+p),d=p+3;
        if(size>v->bytes-d)return v->status=VOC_INVALID;
        uint32_t n=0,offset=VOC_SILENCE,rate=0;
        switch(type) {
        case 1:
            if(size<2)return v->status=VOC_INVALID;
            if(v->source[d+1]!=0)return v->status=VOC_UNSUPPORTED;
            rate=(1000000u+(256u-v->source[d])/2)/(256u-v->source[d]);
            n=size-2;offset=d+2;v->have_data=1;break;
        case 2:
            if(!v->have_data)return v->status=VOC_INVALID;
            n=size;offset=d;break;
        case 3:
            if(size!=3)return v->status=VOC_INVALID;
            rate=(1000000u+(256u-v->source[d+2])/2)/(256u-v->source[d+2]);
            n=le16(v->source+d)+1;break;
        case 6:
            if(size!=2)return v->status=VOC_INVALID;
            if(v->repeating)return v->status=VOC_UNSUPPORTED;
            v->repeating=1;v->repeat_start=d+size;
            v->repeat_left=le16(v->source+d);
            if(v->repeat_left==65535)v->repeat_left=0;
            break;
        case 7:
            if(size || !v->repeating)return v->status=VOC_INVALID;
            if(v->repeat_left) {
                v->repeat_left--;v->pos=v->repeat_start;continue;
            }
            v->repeating=0;break;
        default:return v->status=VOC_UNSUPPORTED;
        }
        if(rate) {
            if(v->rate && v->rate!=rate)return v->status=VOC_UNSUPPORTED;
            v->rate=rate;
        }
        if(n) {
            if(n>v->rate*VOC_SECONDS_MAX-v->samples || v->count==VOC_VISITS_MAX)
                return v->status=VOC_LIMIT;
            v->samples+=n;
            v->spans[v->count++]=(struct voc_span){v->samples,offset};
        }
        v->pos=d+size;
    }
    return v->status;
}
static int sample(const struct voc_host *v,uint32_t index) {
    uint32_t lo=0,hi=v->count;
    if(index>=v->samples)index=v->samples-1;
    while(lo<hi) {uint32_t mid=lo+(hi-lo)/2;
        if(v->spans[mid].end<=index)lo=mid+1;else hi=mid;
    }
    uint32_t start=lo?v->spans[lo-1].end:0;
    if(v->spans[lo].offset==VOC_SILENCE)return 0;
    return ((int)v->source[v->spans[lo].offset+index-start]-128)*256;
}
static void put(uint8_t *out,int s) {
    uint16_t u=(uint16_t)s;
    out[0]=out[2]=(uint8_t)u;out[1]=out[3]=(uint8_t)(u>>8);
}
int voc_render(struct voc_host *v,void *output,size_t frames) {
    if(frames>PC_MAX_FRAMES || (frames && !output))return VOC_INVALID;
    if(!frames)return 0;
    if(v->status)return v->status==VOC_PENDING?VOC_NOT_READY:v->status;
    uint8_t *out=output;
    for(size_t i=0;i<frames;i++) {
        int s=0;
        if(v->frame<v->frames) {
            uint64_t x=(uint64_t)v->frame*v->rate;
            uint32_t a=(uint32_t)(x/PC_RATE),f=(uint32_t)(x%PC_RATE);
            int64_t weighted=(int64_t)sample(v,a)*(PC_RATE-f)+(int64_t)sample(v,a+1)*f;
            s=(int)(weighted*4/(PC_RATE*5));v->frame++;
        }
        put(out+4*i,s);
    }
    return 0;
}
