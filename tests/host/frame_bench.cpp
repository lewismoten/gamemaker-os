// SPDX-License-Identifier: MIT; deterministic first-scene frame benchmark.
#include <time.h>
#include <vector>
#include "hostio.h"
extern "C" {
#include "session.h"
#include "audio.h"
}
#include "gmcore.h"
static uint64_t now_ns() {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}
// Frozen pre-optimization gs_draw: independent pixel oracle and timing baseline.
static void old_draw(const gs_session *s, uint32_t *fb, uint32_t stride) {
  uint32_t lut[256];
  for (unsigned i=0;i<256;i++) {
    uint32_t r=s->pal[i*3]&63, g=s->pal[i*3+1]&63, b=s->pal[i*3+2]&63;
    lut[i]=((r<<2|r>>4)<<16)|((g<<2|g>>4)<<8)|(b<<2|b>>4);
  }
  uint32_t x0=s->showing_card?0:GS_VIEW_X, y0=s->showing_card?GS_CARD_Y:GS_VIEW_Y;
  uint32_t w=s->showing_card?GP_W:GP_VIEW_W, h=s->showing_card?GP_H:GP_VIEW_H;
  for (uint32_t y=0;y<480;y++) {
    uint32_t *row=fb+y*stride;
    if (y<y0||y>=y0+2*h) {for (unsigned x=0;x<640;x++) row[x]=0; continue;}
    const uint8_t *src=s->px+((y-y0)>>1)*GP_W;
    for (uint32_t x=0;x<x0;x++) row[x]=0;
    for (uint32_t x=0;x<w;x++) {uint32_t c=lut[src[x]]; row[x0+2*x]=c; row[x0+2*x+1]=c;}
    for (uint32_t x=x0+2*w;x<640;x++) row[x]=0;
  }
}
// CPU-side present proxy: volatile stores and RGB/BGR swap, like fb_native.
// This is cached host RAM, NOT an HDMI/MMIO or syscall measurement.
static void present(const uint32_t *fb, volatile uint32_t *out, unsigned stride, unsigned y, unsigned h) {
  for (unsigned j=y;j<y+h;j++) for (unsigned x=0;x<640;x++) {
    uint32_t c=fb[j*stride+x];
    out[j*640+x]=(c&0xff00u)|((c&255u)<<16)|((c>>16)&255u);
  }
}
static void keys(unsigned t) {
  if (t%64==1) {gp_key(75,0); gp_key(77,1);}
  if (t%64==33) {gp_key(77,0); gp_key(75,1);}
}
struct Sample {int seq, frame, x, y; uint32_t hash;};
static int start(void *dir, const char *gam) {
  if (gp_open(host_read,dir,gam)<0) return -1;
  gp_world().seed=1;
  return gp_begin();
}
// Compare one-tick slices against eight-tick late arrivals, with/without draws.
static int animation_check(void *dir,const char *gam) {
  std::vector<Sample> expected;
  static gs_session s;
  static uint32_t fb[640*480];
  for (int pass=0;pass<2;pass++) {
    if (start(dir,gam)) return 1;
    s={}; s.phase=GS_PLAY; s.clock_ns=now_ns;
    for (unsigned t=1;t<=128;t++) {
      keys(t);
      // Input is sampled before its tick, identical in both paths. Batch only
      // within a constant-input interval, so event timing cannot change.
      if (pass==0 || t%8==0) {
        uint64_t at=((uint64_t)t*GS_TICK_NS10+9)/10;
        do {
          int result=gs_step(&s,at);
          if (s.ran>4 || (s.phase==GS_PLAY && s.total_ticks<t && (result&GS_DRAW))) return 1;
        } while (s.phase==GS_PLAY && s.total_ticks<t);
        if (s.phase!=GS_PLAY) break;
        auto &w=gp_world();
        Sample v={w.cseq,w.cframe,w.cx[0],w.cy[0],gp_pixels_hash(s.px)};
        if (pass==0) {expected.push_back(v); gs_draw(&s,fb,640);}
        else {
          const Sample &e=expected[t-1];
          if (v.seq!=e.seq||v.frame!=e.frame||v.x!=e.x||v.y!=e.y||v.hash!=e.hash) return 1;
        }
      }
    }
  }
  unsigned changes=0;
  for (size_t i=1;i<expected.size();i++) if (expected[i].frame!=expected[i-1].frame) changes++;
  printf("animation: dense vs sparse/catch-up identical; %zu ticks, %u frame changes\n",expected.size(),changes);
  return expected.size()<8 || !changes;
}
int main(int argc,char **argv) {
  if (argc!=4) {fprintf(stderr,"usage: frame_bench folder NAME.GAM ticks\n");return 2;}
  unsigned n=(unsigned)strtoul(argv[3],nullptr,10); if (!n||n>1000000) return 2;
  if (animation_check(argv[1],argv[2]) || start(argv[1],argv[2])) return 1;
  static gs_session s;
  // Padded stride + sentinels detect accidental writes beyond visible pixels.
  constexpr unsigned stride=648;
  static uint32_t oldfb[stride*480], full[stride*480], dirty[stride*480], hdmi[640*480], partial[640*480];
  for (unsigned i=0;i<stride*480;i++) oldfb[i]=full[i]=dirty[i]=0xdeadbeefu;
  enum {TICK,RENDER,OLD_SCALE,FULL_SCALE,DIRTY_SCALE,OLD_HASH,HASH,OLD_PRESENT,PRESENT,AUDIO,COUNT};
  const char *names[]={"tick","render indices","old scale","new full scale","new dirty scale","old hash (renders again)","new hash (existing pixels)","old present proxy","new present proxy","audio (one tick of PCM)"};
  uint64_t sums[COUNT]={}, maxima[COUNT]={};
  static au audio; static uint8_t music[65536];
  au_init(&audio,music,sizeof music);
  const char *song_name=gp_song(GP_SONG_LEVEL,0);
  if (song_name[0]) {
    int32_t song=host_read(argv[1],song_name,music,sizeof music);
    if (song<0) printf("audio: missing %s, measuring silence (as the session does)\n",song_name);
    else if (au_play_loaded(&audio,(uint32_t)song)) {fprintf(stderr,"invalid song %s\n",song_name);return 1;}
  }
  uint64_t rows=0; unsigned restarts=0; uint32_t checksum=0;
  for (unsigned t=1;t<=n;t++) {
    auto timed=[&](int stage,auto fn) {uint64_t a=now_ns();fn();uint64_t ns=now_ns()-a;sums[stage]+=ns;if(ns>maxima[stage])maxima[stage]=ns;};
    keys(t); char r=0;
    timed(TICK,[&]{r=gp_tick(); gp_take_sound();});
    timed(RENDER,[&]{gp_render(s.px);});
    memcpy(s.pal,gp_palette(),sizeof s.pal);
    timed(OLD_SCALE,[&]{old_draw(&s,oldfb,stride);});
    timed(FULL_SCALE,[&]{gs_draw(&s,full,stride);});
    unsigned y=0,h=0;
    timed(DIRTY_SCALE,[&]{gs_draw_changed(&s,dirty,stride,t==1,&y,&h);}); rows+=h;
    if (memcmp(oldfb,full,sizeof full)||memcmp(oldfb,dirty,sizeof dirty)) {fprintf(stderr,"pixels differ at tick %u\n",t);return 1;}
    uint32_t a=0,b=0;
    timed(OLD_HASH,[&]{a=gp_frame_hash();}); timed(HASH,[&]{b=gp_pixels_hash(s.px);});
    if(a!=b)return 1;
    checksum^=b;
    timed(OLD_PRESENT,[&]{present(oldfb,hdmi,stride,0,480);});
    timed(PRESENT,[&]{present(dirty,partial,stride,y,h);});
    if(memcmp(hdmi,partial,sizeof hdmi)) {fprintf(stderr,"present span differs at tick %u\n",t);return 1;}
    unsigned samples=(unsigned)(((uint64_t)t*GS_TICK_NS10*48000/10000000000ull)-((uint64_t)(t-1)*GS_TICK_NS10*48000/10000000000ull));
    timed(AUDIO,[&]{int16_t pcm[AU_CHUNK*2];while(samples){unsigned k=samples<AU_CHUNK?samples:AU_CHUNK;au_render(&audio,pcm,k);checksum^=(uint16_t)pcm[0];samples-=k;}});
    // Keep the workload in the first scene even if this script finds a link.
    if(r){if(start(argv[1],argv[2]))return 1;restarts++;}
  }
  printf("%u ticks, %u first-scene restarts, %.1f/480 presented rows per tick, checksum %u\n",n,restarts,(double)rows/n,checksum);
  for(unsigned i=0;i<COUNT;i++)printf("%-28s mean %9.2f us max %9.2f us\n",names[i],sums[i]/(1000.0*n),maxima[i]/1000.0);
  // Exercise card/play transitions, palette fades, identical frames, and a
  // staging-surface writer (save/high-score overlays) independent of game data.
  for(unsigned t=0;t<12;t++) {
    s.showing_card=t%2;
    for(unsigned i=0;i<768;i++)s.pal[i]=(uint8_t)((i+t*7)&63);
    if(t==4) dirty[100*stride+23]^=1;
    old_draw(&s,oldfb,stride); unsigned y,h;
    gs_draw_changed(&s,dirty,stride,t==0,&y,&h);
    if(memcmp(oldfb,dirty,sizeof dirty))return 1;
    present(oldfb,hdmi,stride,0,480); present(dirty,partial,stride,y,h);
    if(memcmp(hdmi,partial,sizeof hdmi))return 1;
    if(t==0 && (y!=0 || h!=480))return 1;
    gs_draw_changed(&s,dirty,stride,0,&y,&h); if(h)return 1;
  }
  puts("pixel oracle: all ticks, card/play transitions, palettes, padding, unchanged rows PASS");
  return 0;
}
