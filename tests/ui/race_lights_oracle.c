#include "race_lights.h"
#include "ee.h"
#include "vol.h"
#include "disc_replay.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#define OBJ 0x1800000u
#define DATA 0x1840000u
#define MESH (DATA+0x4000)
#define ENV (DATA+0x10000)
#define COURSE (DATA+0x20000)
#define VT (DATA+0x30000)
#define STACK 0x1ef0000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"race lights %d: %s\n",__LINE__,#x);return 1;}}while(0)
extern ee_fn recomp_lookup(uint32_t);
static float pos[3],global[3],uploaded[4][4][3];
static int missing;
static void nop(ee_ctx *e){(void)e;}
static void runway(ee_ctx *e){e->r[2].uw[0]=MESH;}
static void position(ee_ctx *e){memcpy(e->ram+e->r[4].uw[0],pos,12);}
static void ground(ee_ctx *e){uint32_t id=0;float values[2]={.8f,.7f};unsigned at=e->r[4].uw[0];memcpy(e->ram+at,&id,4);memcpy(e->ram+at+4,values,8);}
static void power(ee_ctx *e){e->fpr[0].f=powf(e->fpr[12].f,e->fpr[13].f);}
static void cosine(ee_ctx *e){e->fpr[0].f=cosf(e->fpr[12].f*0.01745329238474369f);}
static void ambient(ee_ctx *e){memcpy(global,e->ram+e->r[5].uw[0],12);}
static void setter(ee_ctx *e){int index=(int)e->r[4].uw[0]-0x65,type=(int)e->r[5].uw[0];if(index<0 || index>=4)return;type=type==13?0:type==0?1:type==1?2:type==4?3:-1;if(type>=0)memcpy(uploaded[index][type],e->ram+e->r[6].uw[0],12);}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x396800:return runway;case 0x5ffff0:return position;case 0x5ffff8:return ground;
 case 0x57d6c0:return power;case 0x4a5ed8:return cosine;case 0x4a5cc0:return ambient;case 0x4a5bf8:return setter;
 case 0x397d48:case 0x397e38:case 0x4a1638:case 0x4ab040:return nop;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static void word(ee_ctx *e,unsigned at,unsigned v){memcpy(e->ram+at,&v,4);}
static void half(ee_ctx *e,unsigned at,unsigned short v){memcpy(e->ram+at,&v,2);}
static void putf(ee_ctx *e,unsigned at,float v){memcpy(e->ram+at,&v,4);}
static int call(ee_ctx *e,unsigned pc){jmp_buf j;e->exit_jmp=&j;e->r[29].uw[0]=STACK;if(setjmp(j)){fprintf(stderr,"original stopped %08x %s\n",e->end_pc,e->end_detail);return 0;}CHECK(recomp_lookup(pc));recomp_lookup(pc)(e);return 1;}
int main(int argc,char **argv){
 CHECK(argc>=2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0);e.lookup=lookup;e.hook=boundary;
 word(&e,COURSE+0x80,ENV);word(&e,DATA,VT);word(&e,VT+0x28+4,0x5ffff0);word(&e,VT+0x88+4,0x5ffff8);
 word(&e,MESH+0x68,MESH+0x100);word(&e,MESH+0x6c,MESH+0x300);word(&e,MESH+0x74,MESH+0x400);word(&e,MESH+0x78,MESH+0x500);
 half(&e,MESH+0x50,9);putf(&e,ENV+0x38,2);
 for(int i=0;i<9;i++){
  half(&e,MESH+0x300+i*2,(unsigned short)i);word(&e,MESH+0x100+i*32,(unsigned)i);
  putf(&e,MESH+0x100+i*32+16,(float)(i-4));putf(&e,MESH+0x100+i*32+20,4);putf(&e,MESH+0x100+i*32+24,3);
  unsigned s=ENV+i*128;putf(&e,s+0x124,1);putf(&e,s+0x128,.1f);putf(&e,s+0x12c,.02f);
  for(int t=0;t<3;t++)for(int c=0;c<3;c++)putf(&e,s+0xe0+t*16+c*4,(i+t+c)*.07f);
 }
 memcpy(e.ram+ENV,"Env0",4);word(&e,ENV+0x10,5);for(int c=0;c<3;c++)putf(&e,ENV+0xef0+c*4,.06f*(c+1));
 /* Identical immutable RNW4, converted offsets rather than relocated pointers. */
 unsigned char raw[0x20000]={0};gt4_course native={0};native.data=raw;native.size=sizeof raw;native.runway=0x4000;native.road_vertices=0x4400;native.road_tris=0x4500;native.road_tri_count=1;
 memcpy(raw+0x80,"\0\0\1\0",4);
 unsigned cases=0;
 for(int n=0;n<192;n++){
  int custom=n<128;missing=n>=112 && n<128;
  half(&e,MESH+0x400+12,missing?0xffff:0);
  half(&e,MESH+0x40,(unsigned short)custom);
  for(int i=0;i<9;i++){
   putf(&e,MESH+0x100+i*32+28,n&1?1:0);putf(&e,MESH+0x100+i*32+8,1);
   putf(&e,ENV+i*128+0x120,n%3==0?180:n%3==1?30:60);putf(&e,ENV+i*128+0x11c,2);
  }
  if(!custom){word(&e,ENV+0x10,n&1?5:0);}
  for(int j=0;j<3;j++)pos[j]=(n%8-4)*.2f+j*.3f;
  memset(e.ram+OBJ,0,0x250);e.r[4].uw[0]=OBJ;e.r[5].uw[0]=COURSE;e.r[6].uw[0]=DATA;e.fpr[12].f=1;CHECK(call(&e,0x38e7f8));
  memset(uploaded,0,sizeof uploaded);memset(global,0,sizeof global);e.r[4].uw[0]=OBJ;CHECK(call(&e,0x38eff8));
  memcpy(raw+0x4000,e.ram+MESH,0x600);memcpy(raw+0x10000,e.ram+ENV,0x1000);
  unsigned offsets[4]={0x100,0x300,0x400,0x500},slots[4]={0x68,0x6c,0x74,0x78};for(int j=0;j<4;j++)memcpy(raw+0x4000+slots[j],offsets+j,4);
  unsigned char before[sizeof raw];memcpy(before,raw,sizeof raw);
  r3d_lights out;CHECK(race_lights_sample(&native,0,pos,.7f,.8f,&out));CHECK(!memcmp(before,raw,sizeof raw));
  /* Photo film ENVP takes precedence without overwriting the borrowed course. */
  unsigned char snapshot[0x1000];memcpy(snapshot,raw+0x10000,sizeof snapshot);
  memset(raw+0x10000+0xef0,0,12);r3d_lights restored;
  CHECK(race_lights_sample_environment(&native,0,pos,.7f,.8f,snapshot,sizeof snapshot,&restored));
  CHECK(!memcmp(&out,&restored,sizeof out));memcpy(raw,before,sizeof raw);
  if(argc>2 && !strcmp(argv[2],"--negative"))out.ambient[0][0]+=.01f;
  for(int l=0;l<4;l++){
   float *actual[4]={out.direction[l],out.ambient[l],out.diffuse[l],out.specular[l]};
   for(int t=0;t<4;t++)for(int c=0;c<3;c++){
    int source=l;
    if(!custom || missing)source=(l==0?0:l==1?2:-1);
    float expected=source>=0?uploaded[source][t][c]:0;if(t)expected*=2;
    if(l==0 && t==1)expected+=global[c]*2;
    if(fabsf(actual[t][c]-expected)>.000002f){fprintf(stderr,"case%d lamp%d field%d channel%d native%g original%g\n",n,l,t,c,actual[t][c],expected);return 1;}
   }
  }cases++;
 }
 printf("PASS %u original car-light uploads including point/spot/directional/global ambient (2e-6).\n",cases);
 if(argc>3){
  gt4_vol *v=gt4_vol_open(argv[3],NULL);CHECK(v);gt4_disc_replay *dr=gt4_disc_replay_open(v);gt4_replay_file tape={0};char error[256];CHECK(gt4_disc_replay_read(dr,"/replay/Rdemo0/Rdemo0",&tape,error,sizeof error));printf("Rdemo0 course=%s\n",tape.course);
  const char *courses[]={tape.course,"ss_gion","ss_brooklyn","tsukuba","nurburgring"};
  for(unsigned i=0;i<5;i++){
   char path[128];snprintf(path,sizeof path,"/crs/%s",courses[i]);size_t size;unsigned char *bytes=gt4_vol_read(v,path,&size);CHECK(bytes);gt4_course c;CHECK(gt4_course_open(&c,bytes,size));
   unsigned custom=*(unsigned short*)(bytes+c.runway+0x40),samples=0;
   for(int t=0;t<c.road_tri_count;t+=c.road_tri_count/100+1){
    unsigned vertex=*(unsigned short*)(bytes+c.road_tris+t*12);float p[3];memcpy(p,bytes+c.road_vertices+vertex*16,12);r3d_lights out;CHECK(race_lights_sample(&c,t,p,1,1,&out));for(int l=0;l<out.count;l++)for(int k=0;k<3;k++)CHECK(isfinite(out.ambient[l][k]) && isfinite(out.diffuse[l][k]));samples++;
   }
   printf("%s: local-light table=%u, %u valid samples\n",courses[i],custom,samples);free(bytes);
  }
  gt4_replay_free(&tape);gt4_disc_replay_close(dr);gt4_vol_close(v);
 }
 ee_free(&e);return 0;
}
