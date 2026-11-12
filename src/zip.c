/* SPDX-License-Identifier: MIT; ZIP central-directory reader (ZIP32, stored/deflate).
 * Offsets are bounded before every read. Never execute an EXE or extract files. */
#include "zip.h"
#include "library.h"
#include "puff.h"
#include "asset_scratch.h"

#define tail ga_packed
#define TAIL_BYTES 65557u
static uint32_t u16(const uint8_t *p) {return p[0] | (uint32_t)p[1] << 8;}
static uint32_t u32(const uint8_t *p) {return u16(p) | u16(p+2) << 16;}
static unsigned len(const char *s) {unsigned n=0;while(s[n])n++;return n;}
static void copy(char *d,const char *s) {while((*d++=*s++)){} }
static unsigned lower(unsigned c) {return c>='A'&&c<='Z'?c+32:c;}
static int same(const char *a,const char *b) {while(*a&&lower((uint8_t)*a)==lower((uint8_t)*b)){a++;b++;}return *a==*b;}
static uint32_t hash(const char *s) {uint32_t h=2166136261u;while(*s)h=(h^lower((uint8_t)*s++))*16777619u;return h;}
int gz_noise(const char *s) {
  static const char *const names[]={".DS_Store",".Trashes",".Spotlight-V100",".fseventsd","System Volume Information","Thumbs.db","desktop.ini","__MACOSX"};
  const char *p=s;
  for(;;){char part[256];unsigned n=0;while(*p&&*p!='/'){if(n==255)return 1;part[n++]=*p++;}part[n]=0;
    if(n>=2&&part[0]=='.'&&part[1]=='_')return 1;
    for(unsigned i=0;i<sizeof names/sizeof names[0];i++)if(same(part,names[i]))return 1;
    if(!*p)return 0;
    p++;
  }
}
int gz_path(const char *s,char out[256],int directory) {
  unsigned n=0,start=0;
  for(;;){unsigned char c=(unsigned char)*s++;if(c=='\\')c='/';
    if(!c||c=='/'){
      unsigned k=n-start;
      if(!k||(k==1&&out[start]=='.')||(k==2&&out[start]=='.'&&out[start+1]=='.'))return -1;
      if(!c){out[n]=0;return 0;}
      if(directory&&!*s){out[n]=0;return 0;}
      if(n==255)return -1;
      out[n++]='/';start=n;
    } else {if(c<32||c==127||c==':'||n==255)return -1;out[n++]=(char)c;}
  }
}
uint32_t gz_bytes(void) {return sizeof(struct gz_zip);}
const char *gz_gam(const struct gz_zip *z) {return z->gam;}
static int rd(struct gz_zip *z,uint32_t off,void *p,uint32_t n) {return off>z->size||n>z->size-off?-1:z->read_at(z->user,off,p,n);}
int gz_open(struct gz_zip *z,gz_read_at_fn read,void *user,uint32_t size) {
  z->read_at=read;z->user=user;z->size=size;z->count=0;z->gam[0]=z->root[0]=0;
  if(!read||size<22||size>64u*1024u*1024u)return -1;
  uint32_t tn=size<TAIL_BYTES?size:TAIL_BYTES;
  if(rd(z,size-tn,tail,tn))return -1;
  uint32_t end=tn-22;
  for(;;){if(u32(tail+end)==0x06054b50u&&u16(tail+end+20)==tn-end-22)break;if(!end)return -1;end--;}
  const uint8_t *e=tail+end;
  uint32_t count=u16(e+10),dn=u32(e+12),at=u32(e+16),eo=size-tn+end;
  if(u16(e+4)||u16(e+6)||u16(e+8)!=count||!count||count>GZ_ENTRIES_MAX||dn>eo||at>eo-dn)return -1;
  uint32_t shift=eo-dn-at;at+=shift;
  uint32_t stop=eo;
  for(uint32_t i=0;i<count;i++){
    uint8_t h[46],l[30];char name[256],local[256],rawname[256];
    if(at>stop||stop-at<46||rd(z,at,h,46)||u32(h)!=0x02014b50u)return -1;
    uint32_t nn=u16(h+28),extra=u16(h+30),comment=u16(h+32),off=u32(h+42),pn=u32(h+20),rn=u32(h+24),flags=u16(h+8),method=u16(h+10);
    if(!nn||nn>255||nn+extra+comment>stop-at-46||u16(h+34)||u16(h+6)>20||(flags&~0x080eu)||(method!=0&&method!=8)||(method==0&&pn!=rn)||rn>GZ_FILE_MAX||pn>GZ_PACKED_MAX)return -1;
    if(((u32(h+38)>>16)&0170000u)==0120000u)return -1; /* no symlinks */
    if(rd(z,at+46,rawname,nn))return -1;
    rawname[nn]=0;for(unsigned k=0;k<nn;k++)if(!rawname[k])return -1;
    int directory=rawname[nn-1]=='/'||rawname[nn-1]=='\\';
    if(gz_path(rawname,name,directory))return -1;
    if(off>0xffffffffu-shift)return -1;
    off+=shift;
    if(off>eo-dn||eo-dn-off<30||rd(z,off,l,30)||u32(l)!=0x04034b50u||u16(l+6)!=flags||u16(l+8)!=method||u16(l+26)!=nn)return -1;
    uint32_t head=30+nn+u16(l+28);
    if(head>eo-dn-off||pn>eo-dn-off-head||rd(z,off+30,local,nn))return -1;
    local[nn]=0;
    for(unsigned k=0;k<nn;k++)if(local[k]!=rawname[k])return -1;
    if(!(flags&8u)&&(u32(l+14)!=u32(h+16)||u32(l+18)!=pn||u32(l+22)!=rn))return -1;
    at+=46+nn+extra+comment;
    if(directory||gz_noise(name))continue;
    uint32_t nh=hash(name);
    for(unsigned j=0;j<z->count;j++)if(nh==z->entries[j].hash){
      char other[256];const struct gz_entry *old=&z->entries[j];
      if(rd(z,old->name_at,other,old->name_bytes))return -1;
      other[old->name_bytes]=0;
      if(gz_path(other,local,0)||same(name,local))return -1;
    }
    struct gz_entry *f=&z->entries[z->count++];f->name_at=at-(46+nn+extra+comment)+46;f->name_bytes=(uint16_t)nn;f->hash=nh;f->at=off+head;f->packed=pn;f->raw=rn;f->crc=u32(h+16);f->method=(uint16_t)method;
    unsigned k=len(name);
    if(k>4&&same(name+k-4,".gam")){
      unsigned base=k;while(base&&name[base-1]!='/')base--;
      for(unsigned j=0;j+1<base;j++)if(name[j]=='/')return -1;
      if(z->gam[0]){if(len(z->root)!=base)return -1;for(unsigned j=0;j<base;j++)if(lower((uint8_t)z->root[j])!=lower((uint8_t)name[j]))return -1;}
      else {for(unsigned j=0;j<base;j++)z->root[j]=name[j];z->root[base]=0;copy(z->gam,name+base);}
    }
  }
  return at==stop&&z->gam[0]?0:-1;
}
int32_t gz_read(void *user,const char *name,uint8_t *buf,uint32_t cap) {
  struct gz_zip *z=user;char clean[256],key[256];
  if(gz_path(name,clean,0)||gz_noise(clean)||len(z->root)+len(clean)>255)return -1;
  copy(key,z->root);copy(key+len(key),clean);
  for(unsigned i=0;i<z->count;i++){
    const struct gz_entry *f=&z->entries[i];if(hash(key)!=f->hash)continue;
    char rawname[256],entry[256];if(rd(z,f->name_at,rawname,f->name_bytes))return -1;rawname[f->name_bytes]=0;
    if(gz_path(rawname,entry,0))return -1;
    if(!same(key,entry))continue; /* hash collisions never alias files */
    if(rd(z,f->at,ga_packed,f->packed))return -1;
    if(f->method==0){for(unsigned k=0;k<f->raw;k++)ga_whole[k]=ga_packed[k];}
    else {unsigned long dn=f->raw,sn=f->packed;
      if(puff_yield(ga_whole,&dn,ga_packed,&sn,ga_yield)||dn!=f->raw||sn!=f->packed)return -1;
    }
    uint32_t crc=0;
    for(uint32_t at=0;at<f->raw;){
      uint32_t take=f->raw-at<4096u?f->raw-at:4096u;
      if(ga_yield)ga_yield();
      crc=gm_crc32(crc,ga_whole+at,take);at+=take;
    }
    if(crc!=f->crc)return -1;
    uint32_t n=f->raw<cap?f->raw:cap;for(unsigned k=0;k<n;k++)buf[k]=ga_whole[k];return (int32_t)n;
  }
  return -1;
}
