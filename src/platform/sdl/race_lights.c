#include "race_lights.h"
#include <math.h>
#include <string.h>
static unsigned u32(const unsigned char*p){unsigned v;memcpy(&v,p,4);return v;}
static unsigned u16(const unsigned char*p){unsigned short v;memcpy(&v,p,2);return v;}
static float f32(const unsigned char*p){float v;memcpy(&v,p,4);return v;}
static int span(size_t n,size_t at,size_t len){return at<=n && len<=n-at;}
int race_lights_sample(const gt4_course *c,int triangle,const float pos[3],float sun,float level,r3d_lights *out)
{return race_lights_sample_environment(c,triangle,pos,sun,level,NULL,0,out);}
int race_lights_sample_environment(const gt4_course *c,int triangle,const float pos[3],float sun,float level,const uint8_t *replacement,size_t replacement_size,r3d_lights *out)
{
 memset(out,0,sizeof *out);if(!c || !c->data || !span(c->size,0x80,4))return 0;
 const unsigned char *d=c->data;size_t eo=u32(d+0x80);
 if(!eo || !span(c->size,eo,0xf00) || !span(c->size,c->runway,0xa0) || memcmp(d+eo,"Env0",4))return 0;
 const unsigned char *env=replacement?replacement:d+eo,*rw=d+c->runway;
 size_t env_size=replacement?replacement_size:c->size-eo;
 if(env_size<0xf00 || memcmp(env,"Env0",4))return 0;
 size_t list=0;int custom=u16(rw+0x40)!=0;
 if(custom && triangle>=0 && triangle<c->road_tri_count){
  const unsigned char *t=d+c->road_tris+(size_t)triangle*12;
  int index=(short)u16(d+c->road_vertices+u16(t)*16+12);
  if(index>=0)list=c->runway+u32(rw+0x6c)+(size_t)index*2;
 }
 if(list){
  /* 0038E960..0038EAF8: up to nine candidates. 0038EB10..0038EBF0
     stable descending attenuation sort, first four uploaded (not all lamps). */
  struct candidate{size_t record,source;float weight,dir[3];} a[9];
  unsigned count=u16(rw+0x50);if(count>9)count=9;
  if(!span(c->size,list,count*2))return 0;
  for(unsigned i=0;i<count;i++){
   int index=(short)u16(d+list+i*2);if(index<0)return 0;
   struct candidate *v=a+i;v->record=c->runway+u32(rw+0x68)+(size_t)index*32;
   if(!span(c->size,v->record,32))return 0;
   const unsigned char *r=d+v->record;v->source=(size_t)u32(r)*128;
   if(!span(env_size,v->source,0x140))return 0;
   const unsigned char *s=env+v->source;memcpy(v->dir,r+16,12);v->weight=1;
   if(f32(r+28)!=0){
    for(int j=0;j<3;j++)v->dir[j]-=pos[j];
    float q=(v->dir[0]*v->dir[0]+v->dir[1]*v->dir[1])+v->dir[2]*v->dir[2];
    float inverse=1.f/sqrtf(q),distance=q*inverse;
    for(int j=0;j<3;j++)v->dir[j]*=inverse;
    v->weight=1.f/((f32(s+0x124)+f32(s+0x128)*distance)+f32(s+0x12c)*q);
    if(f32(s+0x120)!=180){
     float dot=(v->dir[0]*f32(r+4)+v->dir[1]*f32(r+8))+v->dir[2]*f32(r+12);dot=fmaxf(dot,0);
     /* 004A5ED8 cosine / 0057D6C0 pow are explicit host-libm boundaries. */
     float threshold=cosf(f32(s+0x120)*0.01745329238474369f);
     v->weight=threshold<=dot?v->weight*powf(dot,f32(s+0x11c)):0;
    }
   }
  }
  for(unsigned n=count;n>1;n--)for(unsigned i=0;i<n-1;i++)if(a[i].weight<a[i+1].weight){struct candidate v=a[i];a[i]=a[i+1];a[i+1]=v;}
  out->count=count>4?4:(int)count;
  for(int i=0;i<out->count;i++){
   memcpy(out->direction[i],a[i].dir,12);const unsigned char *s=env+a[i].source;
   float k=f32(d+a[i].record+28)!=0?a[i].weight:1;
   for(int j=0;j<3;j++){out->ambient[i][j]=f32(s+0xe0+j*4)*k;out->diffuse[i][j]=f32(s+0xf0+j*4)*k;out->specular[i][j]=f32(s+0x100+j*4)*k;}
  }
 }else{
  /* 0038EFB0 leaves the initialized ground values zero when custom
     lights lack a surface/list; otherwise 00398210 uses supplied sun/level. */
  if(custom)sun=level=0;
  const unsigned fade[3][2]={{0xac8,0xacc},{0xab4,0xabc},{0xac0,0xac4}};
  for(unsigned i=0;i<4;i++)if(u32(env+0x10)&(1u<<i)){
   int l=out->count++;const unsigned char *s=env+0xd0+i*128;
   memcpy(out->direction[l],s,12);float *colors[3]={out->ambient[l],out->diffuse[l],out->specular[l]};
   for(int t=0;t<3;t++){
    float a=1-f32(env+fade[t][0]),b=1-f32(env+fade[t][1]);
    float k=(1-(1-sun)*a)*b*level+(1-b);
    for(int j=0;j<3;j++)colors[t][j]=f32(s+16+t*16+j*4)*k;
   }
  }
 }
 /* 0038F024..0048 uploads EF0 global ambient independently of lamp
    selection/ground fades. Fold into the first ambient for the GPU sum. */
 if(!out->count)out->count=1;
 for(int j=0;j<3;j++)out->ambient[0][j]+=f32(env+0xef0+j*4);
 out->vertex_limit=f32(env+0x38)*255.f/128;
 return 1;
}
