/* SPDX-License-Identifier: MIT; test-first production behavior witnesses. */
#include "voc_host.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct voc_host v;
static const uint8_t tiny[]={
 'C','r','e','a','t','i','v','e',' ','V','o','i','c','e',' ','F','i','l','e',26,
 26,0,10,1,41,17, 1,6,0,0,131,0,0,128,255,64, 0};
static int s16(const uint8_t *p) {unsigned u=p[0]|(unsigned)p[1]<<8;return u>=32768?(int)u-65536:(int)u;}
static void headers(void) {
 uint8_t b[sizeof(tiny)];memcpy(b,tiny,sizeof(b));
 b[24]^=1;assert(voc_begin(&v,b,sizeof(b))==VOC_INVALID);
 memcpy(b,tiny,sizeof(b));b[0]='X';assert(voc_begin(&v,b,sizeof(b))==VOC_INVALID);
 memcpy(b,tiny,sizeof(b));b[20]=25;assert(voc_begin(&v,b,sizeof(b))==VOC_INVALID);
 b[20]=sizeof(b);assert(voc_begin(&v,b,sizeof(b))==VOC_INVALID);
 assert(voc_begin(&v,NULL,26)==VOC_INVALID);
 assert(voc_begin(&v,tiny,VOC_SOURCE_MAX+1)==VOC_LIMIT);
 for(size_t n=0;n<26;n++)assert(voc_begin(&v,tiny,n)==VOC_INVALID);
 puts("header signature/check/offset/source-length rejection: PASS");
}
static int ready(const void *b,size_t n) {
 int r=voc_begin(&v,b,n);if(r)return r;
 do {r=voc_prepare(&v,1);}while(r==VOC_PENDING);
 return r;
}
static void malformed(void) {
 uint8_t b[sizeof(tiny)+1];memcpy(b,tiny,sizeof(tiny));
 b[31]=1;assert(ready(b,sizeof(tiny))==VOC_UNSUPPORTED);
 for(unsigned codec=2;codec<256;codec++) {b[31]=(uint8_t)codec;assert(ready(b,sizeof(tiny))==VOC_UNSUPPORTED);}
 memcpy(b,tiny,sizeof(tiny));b[27]=1;assert(ready(b,sizeof(tiny))==VOC_INVALID);
 memcpy(b,tiny,sizeof(tiny));b[27]=255;assert(ready(b,sizeof(tiny))==VOC_INVALID);
 for(size_t n=26;n<sizeof(tiny);n++)assert(ready(tiny,n)==VOC_INVALID);
 memcpy(b,tiny,sizeof(tiny));b[sizeof(tiny)]=0;assert(ready(b,sizeof(b))==VOC_INVALID);
 memcpy(b,tiny,sizeof(tiny));b[26]=2;assert(ready(b,sizeof(tiny))==VOC_INVALID);
 for(unsigned type=4;type<=9;type++) {
  if(type==6||type==7)continue;
  memcpy(b,tiny,sizeof(tiny));b[26]=(uint8_t)type;
  assert(ready(b,sizeof(tiny))==VOC_UNSUPPORTED);
 }
 uint8_t empty[27];memcpy(empty,tiny,26);empty[26]=0;
 assert(ready(empty,sizeof(empty))==VOC_INVALID);
 puts("truncated blocks, absent/excess terminator, codecs and unsupported types: PASS");
}
static uint8_t graph[200000];static size_t gn;
static void graph_begin(void) {memcpy(graph,tiny,26);gn=26;}
static void block(unsigned type,const uint8_t *data,unsigned n) {
 assert(gn+4+n<sizeof(graph));graph[gn++]=(uint8_t)type;
 graph[gn++]=(uint8_t)n;graph[gn++]=(uint8_t)(n>>8);graph[gn++]=(uint8_t)(n>>16);
 if(n)memcpy(graph+gn,data,n);
 gn+=n;
}
static int graph_ready(void) {graph[gn++]=0;return ready(graph,gn);}
static void graph_cases(void) {
 const uint8_t data[]={131,0,0,255},cont[]={128,64},sil[]={1,0,131};
 graph_begin();block(1,data,4);block(2,cont,2);block(3,sil,3);
 assert(graph_ready()==0 && v.samples==6 && v.frames==36);
 uint8_t out[36*4];assert(voc_render(&v,out,36)==0);
 assert(s16(out+24*4)==0 && s16(out+35*4)==0);
 const uint8_t repeat[]={2,0};
 graph_begin();block(6,repeat,2);block(1,data,4);block(7,NULL,0);
 assert(graph_ready()==0 && v.samples==6 && v.visits==8);
 for(unsigned c=0;c<2;c++) {
  const uint8_t r[]={c?255:0,c?255:0};
  graph_begin();block(6,r,2);block(1,data,4);block(7,NULL,0);
  assert(graph_ready()==0 && v.samples==2);
 }
 graph_begin();block(6,repeat,2);block(6,repeat,2);block(1,data,4);block(7,NULL,0);
 assert(graph_ready()==VOC_UNSUPPORTED);
 graph_begin();block(7,NULL,0);assert(graph_ready()==VOC_INVALID);
 graph_begin();block(6,repeat,2);block(1,data,4);assert(graph_ready()==VOC_INVALID);
 const uint8_t rate2[]={132,0,1};
 graph_begin();block(1,data,4);block(1,rate2,3);assert(graph_ready()==VOC_UNSUPPORTED);
 graph_begin();block(1,data,4);const uint8_t sil2[]={0,0,132};block(3,sil2,3);
 assert(graph_ready()==VOC_UNSUPPORTED);
 graph_begin();block(3,sil,3);assert(graph_ready()==0 && v.samples==2);
 graph_begin();block(3,sil,3);block(2,cont,2);assert(graph_ready()==VOC_INVALID);
 puts("continuation/silence, repeat count+1/infinite-once, nesting/rate graph rejection: PASS");
}
static void limits(void) {
 uint8_t out[8];memset(out,0x5a,sizeof(out));
 assert(voc_begin(&v,tiny,sizeof(tiny))==0);
 assert(voc_render(&v,out,2)==VOC_NOT_READY);
 for(unsigned i=0;i<sizeof(out);i++)assert(out[i]==0x5a);
 assert(voc_prepare(&v,0)==VOC_INVALID && v.visits==0);
 assert(voc_prepare(&v,65)==VOC_INVALID && v.visits==0);
 const uint8_t empty[]={131,0};
 graph_begin();block(1,empty,2);
 for(unsigned i=0;i<9998;i++)block(2,NULL,0);
 graph[gn++]=0;
 assert(voc_begin(&v,graph,gn)==0);
 int r;do {unsigned old=v.visits;r=voc_prepare(&v,64);assert(v.visits-old<=64);}while(r==VOC_PENDING);
 assert(r==VOC_INVALID && v.visits==10000); /* no samples; exactly10000 visits allowed */
 graph_begin();block(1,empty,2);
 for(unsigned i=0;i<9999;i++)block(2,NULL,0);
 assert(graph_ready()==VOC_LIMIT && v.visits==10000);
 const uint8_t many[]={0xfe,0xff},one[]={131,0,128};
 graph_begin();block(6,many,2);block(1,one,3);block(7,NULL,0);
 assert(graph_ready()==VOC_LIMIT && v.visits==10000);
 const uint8_t longsil[]={255,255,0}; /*3906Hz, 65536 samples*/
 graph_begin();for(unsigned i=0;i<8;i++)block(3,longsil,3);
 assert(graph_ready()==VOC_LIMIT); /* >120seconds, without decoded allocation */
 assert(sizeof(v)<VOC_SOURCE_MAX+100000); /*fixed descriptor memory*/
 assert(ready(tiny,sizeof(tiny))==0);
 uint32_t frame=v.frame;
 assert(voc_render(&v,NULL,1)==VOC_INVALID && v.frame==frame);
 assert(voc_render(&v,out,1921)==VOC_INVALID && v.frame==frame);
 assert(voc_render(&v,NULL,0)==0 && v.frame==frame);
 for(unsigned i=0;i<sizeof(out);i++)assert(out[i]==0x5a);
 puts("pending/output atomicity, prepare budget,10000 visits,120sec and bounded-memory rejection: PASS");
}
static void mixer_cases(void) {
 uint8_t a[8]={255,127,0,128,255,127,0,128};
 uint8_t b[8]={1,0,255,255,255,127,0,128},out[8];
 assert(pcm_mix(out,a,b,2)==0);
 assert(s16(out)==32767 && s16(out+2)==-32768);
 assert(s16(out+4)==32767 && s16(out+6)==-32768);
 assert(pcm_mix(a,a,b,2)==0 && memcmp(a,out,8)==0);
 assert(pcm_mix(NULL,NULL,NULL,0)==0);
 memset(out,0x5a,8);assert(pcm_mix(out,NULL,b,1)==VOC_INVALID);
 assert(pcm_mix(out,a,b,1921)==VOC_INVALID);
 for(unsigned i=0;i<8;i++)assert(out[i]==0x5a);
 uint8_t snd[2160]={0},whole[1920*4],split[1920*4];
 for(unsigned i=0;i<36;i++){snd[2*i]=184;snd[2*i+1]=1;}
 struct pcspeaker p,saved;pc_init(&p);assert(pc_start(&p,snd,sizeof(snd),0)==0);
 assert(ready(tiny,sizeof(tiny))==0);saved=p;
 assert(voc_speaker_render(&v,&p,whole,1920)==0);
 assert(s16(whole)==-26214+PC_AMPLITUDE);
 assert(ready(tiny,sizeof(tiny))==0);p=saved;
 for(unsigned i=0;i<1920;i+=3)assert(voc_speaker_render(&v,&p,split+4*i,3)==0);
 assert(memcmp(whole,split,sizeof(whole))==0);
 voc_stop(&v);p=saved;assert(voc_speaker_render(&v,&p,out,1)==0);
 assert(s16(out)==PC_AMPLITUDE); /*stopping digital must not stop speaker*/
 pc_stop(&p);assert(ready(tiny,sizeof(tiny))==0);
 assert(voc_speaker_render(&v,&p,out,1)==0 && s16(out)==-26214);
 assert(voc_begin(&v,tiny,sizeof(tiny))==0);saved=p;
 memset(out,0x5a,sizeof(out));assert(voc_speaker_render(&v,&p,out,1)==VOC_NOT_READY);
 assert(memcmp(&saved,&p,sizeof(p))==0 && out[0]==0x5a);
 assert(voc_speaker_render(&v,&p,out,1921)==VOC_INVALID && v.frame==0);
 assert(voc_speaker_render(&v,&p,NULL,0)==0);
 puts("saturating stereo mixer, independent voices, split buffers and rejection atomicity: PASS");
}
static void rate_extremes(void) {
 const uint8_t data[]={255,0,255};
 graph_begin();block(1,data,3);assert(graph_ready()==0);
 assert(v.rate==1000000 && v.frames==1);
 uint8_t out[8];assert(voc_render(&v,out,2)==0);
 assert(s16(out)==26009 && s16(out+4)==0);
 const uint8_t slow[]={0,0,0,255};
 graph_begin();block(1,slow,4);assert(graph_ready()==0);
 assert(v.rate==3906 && v.frames==24);
 puts("time constants0/255, sub-frame clip minimum and downsampling: PASS");
}
int main(void) {
 uint8_t out[1920*4];
 headers();malformed();graph_cases();limits();mixer_cases();rate_extremes();
 assert(voc_begin(&v,tiny,sizeof(tiny))==0);
 while (voc_prepare(&v,1)==VOC_PENDING) {}
 assert(v.status==0 && v.rate==8000 && v.samples==4 && v.frames==24);
 assert(voc_render(&v,out,24)==0);
 assert(s16(out)==-26214);
 assert(s16(out+6*4)==0);
 assert(s16(out+12*4)==26009);
 assert(s16(out+18*4)==-13107);
 for (unsigned i=0;i<24;i++)assert(out[4*i]==out[4*i+2]&&out[4*i+1]==out[4*i+3]);
 voc_stop(&v);memset(out,255,sizeof(out));
 assert(voc_render(&v,out,1920)==0);
 for(unsigned i=0;i<sizeof(out);i++)assert(out[i]==0);
 puts("PCM8 decode, rational resampling, master gain and stop: PASS");return 0;
}
