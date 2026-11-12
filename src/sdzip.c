/* SPDX-License-Identifier: MIT; card ZIP/SFX transport, one current game. */
#include <sandbox/syscall.h>
#include "sdzip.h"
static struct gz_zip zip;
static uint64_t fd;
static int opened, seekable;
static uint32_t pos;
static char file[256];
static uint8_t skip[4096];
uint32_t sz_io_errors;
void (*sz_beat)(void);
void sz_close(void) {if(opened)sbx_close(fd);opened=0;}
static unsigned length(const char *s) {unsigned n=0;while(s[n])n++;return n;}
static int reopen(void) {sz_close();if(sbx_open(file,length(file),SBX_O_RDONLY,&fd)){sz_io_errors++;return -1;}opened=1;pos=0;return 0;}
static int read_at(void *u,uint32_t off,void *out,uint32_t n) {
  (void)u;if(!opened&&reopen())return -1;
  if(pos!=off&&seekable){uint64_t p;int r=sbx_seek(fd,off,&p);if(!r)pos=(uint32_t)p;else if(r==-SBX_ENOSYS)seekable=0;else {sz_io_errors++;return -1;}}
  if(pos>off&&reopen())return -1;
  while(pos<off){uint32_t take=off-pos<sizeof skip?off-pos:(uint32_t)sizeof skip;uint64_t got;
    if(sbx_read(fd,skip,take,&got)||!got||got>take){sz_io_errors++;sz_close();return -1;}pos+=(uint32_t)got;if(sz_beat)sz_beat();}
  uint8_t *p=out;
  while(n){uint32_t take=n<16384?n:16384; /* a beat (and the audio pump) after each 16 KiB */uint64_t got;
    if(sbx_read(fd,p,take,&got)||!got||got>take){sz_io_errors++;sz_close();return -1;}pos+=(uint32_t)got;p+=got;n-=(uint32_t)got;if(sz_beat)sz_beat();}
  return 0;
}
struct gz_zip *sz_open(const char *path,uint32_t size,unsigned minor) {
  sz_close();unsigned n=length(path);if(n>255)return 0;
  for(unsigned i=0;i<=n;i++)file[i]=path[i];
  seekable=minor>=3;
  if(gz_open(&zip,read_at,0,size)){sz_close();return 0;}
  return &zip;
}
