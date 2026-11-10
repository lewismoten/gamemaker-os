/* SPDX-License-Identifier: MIT */
#include "collections.h"
#include <string.h>
static int valid(const char *s, unsigned n) {
  if (!n || n > 8) return 0;
  for (unsigned i=0;i<n;i++) if (!((s[i]>='A'&&s[i]<='Z')||(s[i]>='0'&&s[i]<='9')||s[i]=='_'||s[i]=='-')) return 0;
  return 1;
}
int gm_list_has(const struct gm_list *l, const char *key) {
  for (unsigned i=0;i<l->count;i++) if (!strcmp(l->key[i],key)) return (int)i;
  return -1;
}
static void remove_at(struct gm_list *l, unsigned at) {
  for (unsigned i=at+1;i<l->count;i++) memcpy(l->key[i-1],l->key[i],9);
  l->count--;
}
static void push(struct gm_list *l, const char *key) {
  for (unsigned i=l->count;i;i--) memcpy(l->key[i],l->key[i-1],9);
  memcpy(l->key[0],key,strlen(key)+1); l->count++;
}
int gm_list_toggle(struct gm_list *l, const char *key) {
  if (!valid(key,(unsigned)strlen(key))) return -1;
  int at=gm_list_has(l,key);
  if (at>=0) {remove_at(l,(unsigned)at); return 0;}
  if (l->count==GM_FAVORITES_MAX) return -1;
  push(l,key); return 1;
}
int gm_list_recent(struct gm_list *l, const char *key) {
  if (!valid(key,(unsigned)strlen(key))) return 0;
  int at=gm_list_has(l,key);
  if (!at) return 0;
  if (at>=0) remove_at(l,(unsigned)at);
  else if (l->count==GM_RECENT_MAX) l->count--;
  push(l,key); return 1;
}
int gm_list_load(struct gm_list *l, const struct save_ops *o, int recent) {
  char raw[320]; uint32_t n=0; l->count=0;
  int e=save_get(o,"GMOS",recent?"RECENT.TXT":"FAVS.TXT",raw,sizeof raw,&n);
  if (e) return e==SAVE_ENOENT?0:e;
  if (n<7 || memcmp(raw,"GMOS 1\n",7)) return SAVE_EINVAL;
  unsigned cap=recent?GM_RECENT_MAX:GM_FAVORITES_MAX;
  for (unsigned at=7;at<n;) {
    unsigned end=at; while (end<n && raw[end]!='\n') end++;
    unsigned len=end-at;
    if (end<n && valid(raw+at,len) && l->count<cap) {
      char key[9]; memcpy(key,raw+at,len); key[len]=0;
      if (gm_list_has(l,key)<0) memcpy(l->key[l->count++],key,len+1);
    }
    at=end+1;
  }
  return 0;
}
int gm_list_save(const struct gm_list *l, const struct save_ops *o, int recent) {
  char raw[320]; unsigned n=7; memcpy(raw,"GMOS 1\n",7);
  for (unsigned i=0;i<l->count;i++) {
    unsigned len=(unsigned)strlen(l->key[i]); memcpy(raw+n,l->key[i],len); n+=len; raw[n++]='\n';
  }
  return save_put(o,"GMOS",recent?"RECENT.TXT":"FAVS.TXT",raw,n);
}
