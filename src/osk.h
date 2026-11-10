/* SPDX-License-Identifier: MIT; shared remote text-entry navigation and layout. */
#pragma once
#include <stdint.h>
#include "text.h"
#define OSK_KEYS "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
/* 4 rows of 9 characters, then Space/Delete/Clear/Done. */
static inline int osk_press(unsigned *pos, uint16_t code, int32_t value) {
  unsigned k=*pos;
  if(code==103) k=k>=36?27+((k-36)*3>8?8:(k-36)*3):k>=9?k-9:k;
  else if(code==108) {if(k<27)k+=9;else if(k<36)k=36+(k-27)/3;}
  else if(code==105) {if(k && (k>=36?k>36:k%9!=0))k--;}
  else if(code==106) {if(k<39 && (k>=36 || k%9!=8))k++;}
  else if(code==28 && value==1) return k<36?OSK_KEYS[k]:k==36?' ':k==37?8:k==38?127:13;
  *pos=k;return 0;
}
static inline const char *osk_label(unsigned k, char letter[2]) {
  static const char *const actions[]={"Space","Delete","Clear","Done"};
  if(k>=36)return actions[k-36];
  letter[0]=OSK_KEYS[k];letter[1]=0;return letter;
}
static inline void osk_edit(char *text, unsigned cap, int c) {
  unsigned n=0;while(text[n])n++;
  if(c==8) {if(n)text[n-1]=0;}
  else if(c==127)text[0]=0;
  else if(c>=32 && c<127 && n<cap) {text[n]=(char)c;text[n+1]=0;}
}
/* Compact indexed-palette rendering for the 320x200 DOS screens. */
static inline void osk_draw(uint8_t *px, unsigned pos, const char *text, uint8_t fg, uint8_t bg) {
  for(unsigned y=88;y<200;y++)for(unsigned x=0;x<320;x++)px[y*320+x]=bg;
  unsigned len=0;while(text[len] && len<24)len++;
  gp_write(px,4,90,fg,text,len);
  for(unsigned k=0;k<40;k++) {
    int x=k<36?4+(int)(k%9)*35:4+(int)(k-36)*79;
    int y=k<36?104+(int)(k/9)*16:170;
    int w=k<36?30:74;
    char letter[2];const char *label=osk_label(k,letter);unsigned n=0;while(label[n])n++;
    if(k==pos)for(int j=y-2;j<y+11;j++)for(int i=x;i<x+w;i++)px[j*320+i]=fg;
    gp_write(px,x+2,y,k==pos?bg:fg,label,n);
  }
  gp_write(px,4,182,fg,"Arrows choose  OK type  Done accepts",35);
  gp_write(px,4,192,fg,"Back leave; keyboard types / edits",33);
}
