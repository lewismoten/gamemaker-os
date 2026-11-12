/* SPDX-License-Identifier: MIT; menu patch data (dosmenu.h). Offsets and stock
 * values follow the web engine's patchTable.ts and DOS XFERPLAY 3.00. */
#include "dosmenu.h"
struct word {const char *id, *stock; uint32_t off; unsigned max;};
static const struct word words[] = {
  {"str.menu.play", "Play", 0x2575b, 4},
  {"str.menu.instructions", "Read Instructions", 0x25760, 17},
  {"str.menu.storyline", "Read Storyline", 0x25772, 14},
  {"str.menu.credits", "See Credits", 0x25781, 11},
  {"str.menu.highScores", "See Highest Scores", 0x2578d, 18},
  {"str.menu.quit", "Quit", 0x257a0, 4},
  {"str.menu.noInstructions", "No game-specific instructions.", 0x25700, 30},
  {"str.menu.noStoryline", "No game-specific storyline.", 0x2571f, 27},
  {"str.menu.noCredits", "No game-specific credits given.", 0x2573b, 31},
};
struct number {const char *id; uint32_t off; int size, min, max, stock;};
static const struct number numbers[] = {
  {"border.marker.width", 0xaa90, 2, 0, 40, 5},
  {"layout.menu.play.x", 0xb6be, 1, 0, 127, 80},
  {"layout.menu.play.y", 0xb6bc, 1, 0, 127, 120},
  {"layout.menu.instructions.x", 0xb6cf, 1, 0, 127, 80},
  {"layout.menu.instructions.y", 0xb6cc, 2, 0, 199, 128},
  {"layout.menu.storyline.x", 0xb6e0, 1, 0, 127, 80},
  {"layout.menu.storyline.y", 0xb6dd, 2, 0, 199, 136},
  {"layout.menu.credits.x", 0xb6f1, 1, 0, 127, 80},
  {"layout.menu.credits.y", 0xb6ee, 2, 0, 199, 144},
  {"layout.menu.highScores.x", 0xb705, 1, 0, 127, 80},
  {"layout.menu.highScores.y", 0xb702, 2, 0, 199, 152},
  {"layout.menu.quit.x", 0xb716, 1, 0, 127, 80},
  {"layout.menu.quit.y", 0xb713, 2, 0, 199, 160},
  {"layout.menu.titleX", 0xb6ae, 1, 0, 127, 96},
  {"layout.menu.titleY", 0xb6ac, 1, 0, 127, 104},
  {"layout.menu.markerX", 0xafc4, 1, 0, 127, 70},
  {"layout.menu.firstY", 0xafc2, 1, 0, 127, 121},
  {"layout.menu.spacing", 0xafc0, 1, 1, 20, 8},
  {"layout.menu.noFile.x", 0xb362, 1, 0, 127, 40},
  {"layout.menu.noFile.y", 0xb360, 1, 0, 127, 80},
};
static const char *const colors[] = {"yellow", "red", "blue", "purple", "hsRed", "hsBlue", "hsGreen"};
static const uint32_t rgb_off[7][3] = {{0x16336,0x1633b,0x16340},{0x16345,0x1634a,0x1634f},{0x16354,0x16359,0x1635e},{0x16363,0x16368,0x1636d},{0x1c098,0x1c09d,0x1c0a2},{0x1c0a7,0x1c0ac,0x1c0b1},{0x1c0b6,0x1c0bb,0x1c0c0}};
static int same(const char *a,const char *b) {while (*a && *a == *b) {a++; b++;} return *a == *b;}
static void str(char *d,const char *s,unsigned max) {unsigned i=0;for (;s[i] && i<max;i++) d[i]=s[i];d[i]=0;}
void dm_defaults(struct dm_look *d) {
  static const uint8_t rgb[7][3]={{63,63,0},{63,10,10},{10,10,63},{50,10,50},{63,10,10},{10,10,63},{10,63,10}};
  for(unsigned i=0;i<9;i++) str(d->words[i],words[i].stock,39);
  for(unsigned i=0;i<DM_NUMS;i++) d->num[i]=numbers[i].stock;
  for(unsigned i=0;i<7;i++) for(unsigned j=0;j<3;j++) d->rgb[i][j]=rgb[i][j];
  d->color=2;d->message=1;
}
/* Bounded JSON tokenizer: only strings, integers and RGB triples are applied.
 * Everything else is skipped recursively, with a depth limit. */
struct json {const uint8_t *p,*end; int bad;};
static void ws(struct json *j) {while(j->p<j->end && *j->p<=32) j->p++;}
static int eat(struct json *j,int c) {ws(j);if(j->p<j->end && *j->p==c){j->p++;return 1;}return 0;}
static int hex(int c) {return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static void string(struct json *j,char *out,unsigned cap) {
  unsigned n=0;if(!eat(j,'"')) {j->bad=1;return;}
  while(j->p<j->end && *j->p!='"') {
    int c=*j->p++;
    if(c=='\\') {
      if(j->p==j->end){j->bad=1;return;}c=*j->p++;
      if(c=='u'){c=0;for(int i=0;i<4;i++){int v=j->p<j->end?hex(*j->p++):-1;if(v<0){j->bad=1;return;}c=c*16+v;}}
      else if(c!='"'&&c!='\\'&&c!='/'){j->bad=1;return;}
    }
    if(c<32 || c>126){j->bad=1;return;}
    if(out && n<cap) out[n++]=(char)c;
  }
  if(out)out[n]=0;
  if(!eat(j,'"')) j->bad=1;
}
static int integer(struct json *j) {
  ws(j);int neg=eat(j,'-'),v=0,n=0;
  while(j->p<j->end && *j->p>='0'&&*j->p<='9'){if(v>1000000){j->bad=1;return 0;}v=v*10+(*j->p++-'0');n++;}
  if(!n)j->bad=1;
  return neg?-v:v;
}
static void value(struct json *j,struct dm_look *d,const char *id,int depth) {
  ws(j);if(j->p==j->end||depth>8){j->bad=1;return;}
  if(*j->p=='{') {
    j->p++;if(eat(j,'}'))return;
    do {char key[80]={0};string(j,key,79);if(!eat(j,':')){j->bad=1;return;}value(j,d,key,depth+1);}while(!j->bad&&eat(j,','));
    if(!eat(j,'}'))j->bad=1;
    return;
  }
  if(*j->p=='"') {
    char s[80]={0};string(j,s,79);
    for(unsigned i=0;i<9;i++) if(same(id,words[i].id)) str(d->words[i],s,words[i].max);
    for(int i=0;i<7;i++) if(same(s,colors[i])) {if(same(id,"color.select.menu"))d->color=i;if(same(id,"color.select.menuMessages"))d->message=i;}
    return;
  }
  if(*j->p=='[') {
    j->p++;int rgb[3]={0},n=0;
    if(!eat(j,']')) {do {if(n<3)rgb[n]=integer(j);else value(j,d,"",depth+1);n++;}while(!j->bad&&eat(j,','));if(!eat(j,']'))j->bad=1;}
    if(n==3&&!j->bad&&rgb[0]>=0&&rgb[0]<=63&&rgb[1]>=0&&rgb[1]<=63&&rgb[2]>=0&&rgb[2]<=63) {
      for(unsigned i=0;i<7;i++){char key[40]="color.rgb.";str(key+10,colors[i],20);if(same(id,key))for(unsigned k=0;k<3;k++)d->rgb[i][k]=(uint8_t)rgb[k];}
    }return;
  }
  if(*j->p=='-'||(*j->p>='0'&&*j->p<='9')) {int v=integer(j);for(unsigned i=0;i<DM_NUMS;i++)if(same(id,numbers[i].id)&&v>=numbers[i].min&&v<=numbers[i].max)d->num[i]=v;return;}
  const uint8_t *at=j->p;while(j->p<j->end&&*j->p>='a'&&*j->p<='z')j->p++;if(at==j->p)j->bad=1;
}
int dm_json(struct dm_look *d,const uint8_t *data,uint32_t n) {
  struct json j={data,data+n,0};struct dm_look candidate=*d;
  /* Require the supported base, and apply only its patches object. */
  int base=0,patches=0;if(!eat(&j,'{'))return -1;
  if(!eat(&j,'}')) {do {
    char key[80]={0};string(&j,key,79);if(!eat(&j,':'))return -1;
    if(same(key,"base")){char s[40]={0};string(&j,s,39);base=same(s,"xferplay-3.00");}
    else if(same(key,"patches")){value(&j,&candidate,"",0);patches=1;}
    else {struct dm_look ignored=candidate;value(&j,&ignored,"",0);}
  }while(!j.bad&&eat(&j,','));if(!eat(&j,'}'))return -1;}
  ws(&j);if(j.bad||j.p!=j.end||!base||!patches)return -1;*d=candidate;return 0;
}
static int u16(const uint8_t *p){return p[0]|p[1]<<8;}
static int exe(struct dm_look *d,const uint8_t *p,int32_t n) {
  /* Restrict offsets to the CD transfer build: MZ and every menu GWrite's
   * position push opcodes. Other player builds keep the stock look. */
  if(n<0x257a5||p[0]!='M'||p[1]!='Z')return 0;
  for(unsigned i=DM_PLAY_X;i<=DM_TITLEY;i++)
    if(p[numbers[i].off-1]!=(numbers[i].size==1?0x6a:0x68))return 0;
  for(unsigned i=0;i<9;i++){unsigned k=0;while(k<words[i].max&&p[words[i].off+k]>=32&&p[words[i].off+k]<=126)k++;if(p[words[i].off+k]!=0)return 0;}
  for(unsigned i=0;i<9;i++)str(d->words[i],(const char *)p+words[i].off,words[i].max);
  for(unsigned i=0;i<DM_NUMS;i++){int v=numbers[i].size==1?(int8_t)p[numbers[i].off]:u16(p+numbers[i].off);if(v>=numbers[i].min&&v<=numbers[i].max)d->num[i]=v;}
  for(unsigned i=0;i<7;i++)for(unsigned k=0;k<3;k++)if(p[rgb_off[i][k]]<=63)d->rgb[i][k]=p[rgb_off[i][k]];
  static const int target[]={0x26f,0x272,0x275,0x278,0x15fa,0x15fd,0x1600};
  for(int i=0;i<7;i++){if(u16(p+0xaf84)==target[i])d->color=i;if(u16(p+0xaf97)==target[i])d->message=i;}
  return 1;
}
void dm_load(struct dm_look *d,gp_read_fn read,void *user,const char *gam,uint8_t *scratch,uint32_t cap) {
  dm_defaults(d);char name[16];unsigned k=0;while(gam[k]&&gam[k]!='.'&&k<8){name[k]=gam[k];k++;}str(name+k,".EXE",4);
  const char *names[]={name,"XFERPLAY.EXE","PLAYGAME.EXE"};
  for(unsigned i=0;i<3;i++){int32_t n=read(user,names[i],scratch,cap);
    if(n<0){char lower[16];unsigned j=0;for(;names[i][j]&&j<15;j++){char c=names[i][j];lower[j]=c>='A'&&c<='Z'?c+32:c;}lower[j]=0;n=read(user,lower,scratch,cap);}
    if(exe(d,scratch,n))break;}
  int32_t n=read(user,"PLAYER.JSON",scratch,cap);if(n<0)n=read(user,"player.json",scratch,cap);if(n>0)dm_json(d,scratch,(uint32_t)n);
}
