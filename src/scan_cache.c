/* SPDX-License-Identifier: MIT; GMI2 cache is expendable, not a game index.
 * Header: magic, total bytes, entry count, CRC32 (header[0:12]+payload).
 * Entries: u16 path bytes, u16 exact key bytes, u32 value bytes, then bytes.
 * Bump magic when scanner interpretation/duplicate semantics change. */
#include "scan_cache.h"
static uint32_t u16(const uint8_t *p) {return p[0]|(uint32_t)p[1]<<8;}
static uint32_t u32(const uint8_t *p) {return u16(p)|u16(p+2)<<16;}
static void w16(uint8_t *p,uint32_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void w32(uint8_t *p,uint32_t v) {w16(p,v);w16(p+2,v>>16);}
static void copy(uint8_t *d,const uint8_t *s,uint32_t n) {while(n--)*d++=*s++;}
static int equal(const uint8_t *a,const uint8_t *b,uint32_t n) {while(n--)if(*a++!=*b++)return 0;return 1;}
static uint32_t pathlen(const char *p) {uint32_t n=0;if(!p)return 0;while(n<256&&p[n])n++;return n<256?n:0;}
static uint32_t crc(uint32_t c,const uint8_t *p,uint32_t n) {
 static uint32_t table[256];
 if(!table[1])for(uint32_t i=0;i<256;i++){uint32_t v=i;for(int j=0;j<8;j++)v=v>>1^(0xedb88320u&(0u-(v&1)));table[i]=v;}
 c=~c;while(n--)c=table[(c^*p++)&255u]^(c>>8);return ~c;
}
static uint32_t checksum(const uint8_t *p,uint32_t n) {return crc(crc(0,p,12),p+16,n-16);}
void gc_init(struct gc_cache *c,const void *old,uint32_t n,void *out,uint32_t cap) {
 c->old=old;c->oldbytes=n;c->out=out;c->capacity=cap;c->used=16;c->valid=0;c->overflow=cap<16||!out;
 if(!c->overflow){for(unsigned i=0;i<16;i++)c->out[i]=0;copy(c->out,(const uint8_t *)"GMI2",4);}
 if(!old||n<16||n>GC_CAPACITY)return;
 const uint8_t *p=old;
 if(!equal(p,(const uint8_t *)"GMI2",4)||u32(p+4)!=n||u32(p+8)>1024||u32(p+12)!=checksum(p,n))return;
 uint32_t at=16,count=0;
 while(at<n){
  if(n-at<8)return;
  uint32_t pn=u16(p+at),kn=u16(p+at+2),vn=u32(p+at+4);at+=8;
  if(!pn||pn>255||!kn||kn>256||pn>n-at)return;
  at+=pn;if(kn>n-at)return;at+=kn;if(vn>n-at)return;at+=vn;
  if(++count>1024)return;
 }
 if(count==u32(p+8))c->valid=1;
}
const uint8_t *gc_get(const struct gc_cache *c,const char *path,const void *key,uint32_t kn,uint32_t *bytes) {
 uint32_t pn=pathlen(path);*bytes=0;
 if(!c->valid||!pn||!kn||kn>256||!key)return 0;
 for(uint32_t at=16;at<c->oldbytes;){
  const uint8_t *p=c->old+at;uint32_t a=u16(p),b=u16(p+2),n=u32(p+4);p+=8;
  if(a==pn&&b==kn&&equal(p,(const uint8_t *)path,pn)&&equal(p+a,key,kn)){*bytes=n;return p+a+b;}
  at+=8+a+b+n;
 }
 return 0;
}
int gc_add(struct gc_cache *c,const char *path,const void *key,uint32_t kn,const void *value,uint32_t n) {
 uint32_t pn=pathlen(path);
 if(c->overflow)return 0;
 if(!pn||!kn||kn>256||!key||(!value&&n)||u32(c->out+8)>=1024||n>c->capacity-c->used||8+pn+kn>c->capacity-c->used-n){c->overflow=1;return 0;}
 uint8_t *p=c->out+c->used;w16(p,pn);w16(p+2,kn);w32(p+4,n);p+=8;
 copy(p,(const uint8_t *)path,pn);copy(p+pn,key,kn);copy(p+pn+kn,value,n);
 c->used+=8+pn+kn+n;w32(c->out+8,u32(c->out+8)+1);return 1;
}
uint32_t gc_finish(struct gc_cache *c) {
 if(c->overflow)return 0;
 w32(c->out+4,c->used);w32(c->out+12,checksum(c->out,c->used));return c->used;
}
int gc_carry(struct gc_cache *c,const char *skip) {
 uint32_t sn=pathlen(skip);char path[256];
 if(!c->valid)return 0;
 for(uint32_t at=16;at<c->oldbytes;){
  const uint8_t *p=c->old+at;uint32_t a=u16(p),b=u16(p+2),n=u32(p+4);p+=8;
  at+=8+a+b+n;
  if(a>=sn&&equal(p,(const uint8_t *)skip,sn))continue;
  copy((uint8_t *)path,p,a);path[a]=0;
  if(!gc_add(c,path,p+a,b,p+a+b,n))return 0;
 }
 return 1;
}
