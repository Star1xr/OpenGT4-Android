/* /pit/license_001 Mot1 banks. Evidence: 003E7360, 00429870, 00429BC0,
   0042A290/0042A3D8/0042C348 and 0042B440. No guest execution in the app. */
#include "license_motion.h"
#include <math.h>
#include <fenv.h>
#include <stdlib.h>
#include <string.h>
typedef struct {const uint8_t *d;size_t size;} bank;
typedef struct {int selected;unsigned group;int atomic,sequence;float time;} track;
struct gt4_license_motion {uint8_t *data;size_t size;bank actors,camera;track slot[7];unsigned camera_slot;int borrowed;};
static unsigned h(const uint8_t *d){return d[0]|(unsigned)d[1]<<8;}
static uint32_t w(const uint8_t *d){return h(d)|h(d+2)<<16;}
static float f(const uint8_t *d){float x;memcpy(&x,d,4);return x;}
static int span(size_t n,size_t p,size_t len){return p<=n && len<=n-p;}
static const uint8_t *at(bank b,size_t p,size_t len){return span(b.size,p,len)?b.d+p:NULL;}
static const uint8_t *group(bank b,unsigned i){const uint8_t *t=at(b,w(b.d+16),16*h(b.d+20));return t && i<h(b.d+20)?t+i*16:NULL;}
static const uint8_t *record(bank b,unsigned g,int index,int sequence){
 const uint8_t *t=group(b,g);if(!t || index<0 || (unsigned)index>=h(t+(sequence?2:0)))return NULL;
 return at(b,w(t+(sequence?8:4))+(size_t)index*32,32);
}
static const uint8_t *entry(bank b,const uint8_t *r,float time){
 unsigned count=h(r);const uint8_t *list=at(b,w(r+4),(size_t)count*32);if(!count || !list)return NULL;
 /* 0042A560 / 0042A510: choose the last start <= time, clamped at ends. */
 unsigned i=0;while(i+1<count && time>=f(list+(i+1)*32+4))i++;return list+i*32;
}
static float loop_entry(const uint8_t *e,float time){
 /* 0042A290: flag +14 clamps; otherwise repeat the entry's own period. */
 float begin=f(e+4),period=f(e+12);
 if(e[20])return fminf(time,begin+period);
 float t=time-begin;if(period)t-=period*floorf(t/period);return t+begin;
}
static int sequence_step(bank b,const uint8_t *r,float *time,float dt,int control){
 /* 00429960 +0x40..0x148: advance through policy/control gates,
    then apply the current entry's loop/clamp. Course camera entries
    use policy 2 and controls 1..5; license actors use control zero. */
 const uint8_t *old=entry(b,r,*time);if(!old)return 0;
 float next=*time+dt;
 if((int32_t)w(old+28)<control){
  const uint8_t *now=entry(b,r,next);if(!now)return 0;
  const uint8_t *last=at(b,w(r+4)+(size_t)(h(r)-1)*32,32);
  if(now[19]>=2 && (int32_t)w(now+28)<control){
   if(now==last){*time=f(r+8);return 1;}now+=32;next=f(now+4);
  }
  if(now!=old){
   while(now[19] && (int32_t)w(now+28)<control){
    if(now==last){*time=f(r+8);return 1;}now+=32;next=f(now+4);
   }
   *time=next;return 1;
  }
  if((int32_t)w(now+28)<control){*time=next;return 1;}
  old=now;
 }
 next=loop_entry(old,next);
 if(next>=f(old+4)+f(old+12))next-=dt*.001f;
 *time=next;return 1;
}
gt4_license_motion *gt4_license_motion_open(const uint8_t *d,size_t size){
 if(!d || size<20)return NULL;unsigned offsets[4];for(unsigned i=0;i<4;i++)offsets[i]=w(d+4+i*4);
 for(unsigned i=0;i<4;i++)if(!span(size,offsets[i],128) || (i && offsets[i]<=offsets[i-1]))return NULL;
 if(memcmp(d+offsets[0],"Mot1",4) || memcmp(d+offsets[1],"Mot1",4) || memcmp(d+offsets[2],"MDLS",4) || memcmp(d+offsets[3],"MDLS",4))return NULL;
 gt4_license_motion *m=calloc(1,sizeof *m);if(!m)return NULL;m->data=malloc(size);if(!m->data){free(m);return NULL;}
 memcpy(m->data,d,size);m->size=size;m->camera_slot=3;m->actors=(bank){m->data+offsets[0],offsets[1]-offsets[0]};m->camera=(bank){m->data+offsets[1],offsets[2]-offsets[1]};
 bank banks[2]={m->actors,m->camera};
 for(unsigned i=0;i<2;i++){bank b=banks[i];if(!at(b,w(b.d+16),16*h(b.d+20)) || !at(b,w(b.d+28),16*h(b.d+22))){gt4_license_motion_close(m);return NULL;}}
 return m;
}
gt4_license_motion *gt4_license_motion_open_pit(const uint8_t *d,size_t size){
 gt4_license_motion *m=gt4_license_motion_open(d,size);if(m)m->camera_slot=6;return m;
}
gt4_license_motion *gt4_license_motion_clone(const gt4_license_motion *owner){
 if(!owner)return NULL;gt4_license_motion*m=malloc(sizeof *m);if(m){*m=*owner;m->borrowed=1;memset(m->slot,0,sizeof m->slot);}return m;
}
gt4_license_motion *gt4_license_motion_open_camera(const uint8_t *d,size_t size){
 if(!d || size<32 || memcmp(d,"Mot1",4))return NULL;
 bank b={d,size};if(!at(b,w(d+16),16*h(d+20)) || !at(b,w(d+28),16*h(d+22)))return NULL;
 gt4_license_motion *m=calloc(1,sizeof *m);if(!m)return NULL;
 /* 0037D734 reads the resident course bank. Borrow it too: no copy
    of the course tail, and no allocation/decoding during animation. */
 m->camera=b;m->camera_slot=3;return m;
}
void gt4_license_motion_close(gt4_license_motion *m){if(m){if(!m->borrowed)free(m->data);free(m);}}
const uint8_t *gt4_license_motion_model(const gt4_license_motion *m,unsigned part,size_t *size){
 if(!m || !m->data || part>1 || !size)return NULL;size_t p=w(m->data+12+part*4),n=w(m->data+p+16);
 if(!span(m->size,p,n))return NULL;*size=n;return m->data+p;
}
int gt4_license_motion_select(gt4_license_motion *m,unsigned slot,const char *name){
 if(!m || slot>m->camera_slot || !name)return 0;bank b=slot==m->camera_slot?m->camera:m->actors;
 if(!b.d)return 0;
 const uint8_t *table=at(b,w(b.d+28),16*h(b.d+22));if(!table)return 0;
 for(unsigned i=0;i<h(b.d+22);i++){
  const uint8_t *r=table+i*16;size_t p=w(r);const uint8_t *label=at(b,p,1);
  if(!label || !memchr(label,0,b.size-p))return 0;
  if(strcmp((const char *)label,name))continue;
  track t={1,h(r+4),(int16_t)h(r+6),(int16_t)h(r+8),0};
  if(!record(b,t.group,t.atomic>=0?t.atomic:t.sequence,t.atomic<0))return 0;
  m->slot[slot]=t;return 1;
 }return 0;
}
int gt4_license_motion_step_control(gt4_license_motion *m,unsigned slot,float dt,int control){
 if(!m || slot>m->camera_slot || !m->slot[slot].selected || !isfinite(dt) || dt<0)return 0;
 bank b=slot==m->camera_slot?m->camera:m->actors;track *t=&m->slot[slot];const uint8_t *r=record(b,t->group,t->atomic>=0?t->atomic:t->sequence,t->atomic<0);if(!r)return 0;
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);float time=t->time;
 if(t->atomic>=0){
  /* 00429BC0 +0x58 / 0042D430, looping packed motion. */
  time+=dt;float end=f(r),begin=f(r+4);if(time>=end){float period=end-begin;time-=begin;if(period)time-=period*floorf(time/period);time+=begin;}
 }else{
  if(!sequence_step(b,r,&time,dt,control)){fesetenv(&env);return 0;}
  float end=f(r+8);
  if(end>0 && time>=end){
   do{time-=end;}while(time>=end);
   if(!sequence_step(b,r,&time,0,control)){fesetenv(&env);return 0;}
  }
 }
 t->time=time;fesetenv(&env);return 1;
}
int gt4_license_motion_step(gt4_license_motion *m,unsigned slot,float dt){return gt4_license_motion_step_control(m,slot,dt,0);}
int gt4_license_motion_set_time(gt4_license_motion *m,unsigned slot,float time){
 if(!m || slot>m->camera_slot || !m->slot[slot].selected || !isfinite(time))return 0;
 m->slot[slot].time=time;return 1;
}
int gt4_license_motion_seek_control(gt4_license_motion *m,unsigned slot,int control){
 if(!m || slot>m->camera_slot || !m->slot[slot].selected)return 0;
 track*t=m->slot+slot;if(t->atomic>=0){t->time=0;return 1;}
 bank b=slot==m->camera_slot?m->camera:m->actors;const uint8_t*r=record(b,t->group,t->sequence,1);
 const uint8_t*list=r?at(b,w(r+4),h(r)*32):NULL;if(!list)return 0;
 for(unsigned i=0;i<h(r);i++)if((int32_t)w(list+i*32+28)>=control){t->time=f(list+i*32+4);return 1;}return 0;
}
int gt4_license_motion_interval(const gt4_license_motion *m,unsigned slot,const char **label,float *begin,float *end){
 if(!m || slot>m->camera_slot || !m->slot[slot].selected || !label || !begin || !end)return 0;
 const track*t=m->slot+slot;bank b=slot==m->camera_slot?m->camera:m->actors;
 const uint8_t*r=record(b,t->group,t->sequence,1),*e=t->atomic<0 && r?entry(b,r,t->time):NULL;
 if(!e)return 0;size_t p=w(e+24);*label=p && at(b,p,1) && memchr(b.d+p,0,b.size-p)?(const char*)b.d+p:NULL;
 *begin=f(e+4);*end=*begin+f(e+12);return 1;
}
int gt4_license_motion_passed(const gt4_license_motion *m,unsigned slot,const char *label){
 if(!m || slot>m->camera_slot || !m->slot[slot].selected || !label)return 0;
 const track*t=m->slot+slot;if(t->atomic>=0)return 0;
 bank b=slot==m->camera_slot?m->camera:m->actors;const uint8_t*r=record(b,t->group,t->sequence,1),*e=r?entry(b,r,t->time):NULL;
 const uint8_t*first=r?at(b,w(r+4),h(r)*32):NULL;if(!e || !first)return 0;
 while(e>first){e-=32;size_t p=w(e+24);if(p && at(b,p,1) && memchr(b.d+p,0,b.size-p) && !strcmp((const char*)b.d+p,label))return 1;}return 0;
}
unsigned gt4_license_motion_targets(const gt4_license_motion*m,unsigned slot){
 if(!m || slot>m->camera_slot || !m->slot[slot].selected)return 0;const track*t=m->slot+slot;if(t->atomic>=0)return 1;
 bank b=slot==m->camera_slot?m->camera:m->actors;const uint8_t*r=record(b,t->group,t->sequence,1),*e=r?entry(b,r,t->time):NULL;return e?h(e+16):0;
}
int gt4_license_motion_label_interval(const gt4_license_motion*m,unsigned slot,const char*label,float*begin,float*end){
 if(!m || slot>m->camera_slot || !m->slot[slot].selected || !label || !begin || !end)return 0;
 const track*t=m->slot+slot;if(t->atomic>=0)return 0;bank b=slot==m->camera_slot?m->camera:m->actors;
 const uint8_t*r=record(b,t->group,t->sequence,1),*list=r?at(b,w(r+4),h(r)*32):NULL;if(!list)return 0;
 for(unsigned i=0;i<h(r);i++){const uint8_t*e=list+i*32;size_t p=w(e+24);if(p && at(b,p,1) && memchr(b.d+p,0,b.size-p) && !strcmp((const char*)b.d+p,label)){*begin=f(e+4);*end=*begin+f(e+12);return 1;}}return 0;
}
float gt4_license_motion_time(const gt4_license_motion *m,unsigned slot){return m && slot<=m->camera_slot?m->slot[slot].time:0;}
int gt4_license_motion_sample_blend(const gt4_license_motion *m,unsigned slot,float blend,unsigned *g,unsigned atoms[2],float *time,float *factor){
 if(!m || slot>m->camera_slot || !g || !atoms || !time || !factor || !isfinite(blend) || blend<=-2147483648.f || blend>=2147483648.f || !m->slot[slot].selected)return 0;
 bank b=slot==m->camera_slot?m->camera:m->actors;const track *t=&m->slot[slot];*g=t->group;*time=t->time;*factor=0;
 if(t->atomic>=0){atoms[0]=atoms[1]=(unsigned)t->atomic;return 1;}
 const uint8_t *r=record(b,t->group,t->sequence,1),*e=r?entry(b,r,t->time):NULL;if(!e || !h(e+16))return 0;
 const uint8_t *indices=at(b,w(e),h(e+16)*2);if(!indices)return 0;
 /* 0042C348 +68..b8: cvt.w.s truncates, then clamps each adjacent
    target independently. The fractional part belongs to the unclamped i. */
 int index=(int)blend,first=index,second=index+1,last=(int)h(e+16)-1;
 if(first<0)first=0;if(first>last)first=last;if(second<0)second=0;if(second>last)second=last;
 atoms[0]=h(indices+first*2);atoms[1]=h(indices+second*2);
 if(first!=second)*factor=blend-(float)index;
 float local=t->time;if(e[18])local=f(e+4)+(f(e+4)+f(e+12))-local;
 *time=(local-f(e+4))+f(e+8);return record(b,*g,(int)atoms[0],0) && record(b,*g,(int)atoms[1],0);
}
int gt4_license_motion_sample(const gt4_license_motion *m,unsigned slot,unsigned *g,unsigned *a,float *time){
 unsigned atoms[2];float factor;if(!a || !gt4_license_motion_sample_blend(m,slot,0,g,atoms,time,&factor))return 0;*a=atoms[0];return 1;
}
/* 0042D5D0 / 0042D8C0: the first inclusive event, not a count of keys. */
static float atomic_mark(bank b,const uint8_t *r,const char *name,float lo,float hi){
 const uint8_t *list=at(b,w(r+28),h(r+22)*16);if(!list)return 0;
 for(unsigned i=0;i<h(r+22);i++){
  const uint8_t *mark=list+i*16;size_t p=w(mark);const uint8_t *label=at(b,p,1);
  if(!label || !memchr(label,0,b.size-p))return 0;if(strcmp((const char *)label,name))continue;
  const uint8_t *events=at(b,w(mark+8),h(mark+4)*8);if(!events)return 0;
  for(unsigned k=0;k<h(mark+4);k++)if(lo<=f(events+k*8) && f(events+k*8)<=hi)return f(events+k*8+4);
  return 0;
 }return 0;
}
static float entry_marks(bank b,unsigned g,const uint8_t *e,const char *name,float lo,float hi){
 const uint8_t *indices=at(b,w(e),h(e+16)*2);if(!indices)return 0;
 lo=(lo-f(e+4))+f(e+8);hi=(hi-f(e+4))+f(e+8);float result=0;
 for(unsigned i=0;i<h(e+16);i++){const uint8_t *r=record(b,g,(int16_t)h(indices+i*2),0);if(r)result+=atomic_mark(b,r,name,lo,hi);}return result;
}
static float sequence_marks(bank b,const track *t,const char *name,float old,float now,float dt){
 const uint8_t *r=record(b,t->group,t->sequence,1),*e=r?entry(b,r,old):NULL,*n=r?entry(b,r,now):NULL;if(!e || !n)return 0;
 float lo,hi,lo2=0,hi2=0;const uint8_t *second=NULL;
 /* 0042C4B8 +80..174: a wrap visits the head and tail, while a
    crossed entry visits only the old and new entries within dt. */
 if(e==n){
  int wrapped=now<old;lo=wrapped?now:old;hi=wrapped?old:now;
  if((wrapped && !e[18]) || (!wrapped && e[18])){second=e;lo2=hi;hi2=f(e+4)+f(e+12);hi=lo;lo=f(e+4);}
 }else{
  if(!e[18]){lo=old;hi=fminf(old+dt,f(e+4)+f(e+12));}else{lo=fmaxf(old-dt,f(e+4));hi=old;}
  second=n;if(!n[18]){lo2=fmaxf(now-dt,f(n+4));hi2=now;}else{lo2=now;hi2=fminf(now+dt,f(n+4)+f(n+12));}
 }
 float result=entry_marks(b,t->group,e,name,lo,hi);if(second)result+=entry_marks(b,t->group,second,name,lo2,hi2);return result;
}
float gt4_license_motion_mark(const gt4_license_motion *m,unsigned slot,const char *name,float old,float now,float dt){
 if(!m || slot>m->camera_slot || !name || !m->slot[slot].selected || !isfinite(old) || !isfinite(now) || !isfinite(dt))return 0;
 bank b=slot==m->camera_slot?m->camera:m->actors;const track *t=m->slot+slot;
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 float result=t->atomic>=0?atomic_mark(b,record(b,t->group,t->atomic,0),name,old,now):sequence_marks(b,t,name,old,now,dt);
 fesetenv(&env);return result;
}
static int curve(bank b,const uint8_t *r,unsigned index,float time,float *value){
 /* 0042D688: +14 is a 16-bit channel count; +16 is the independent
    event count. Pit motions carry both; treating them as one word rejects
    valid channels as soon as an animation contains a sound mark. */
 if(index>=h(r+20))return 0;const uint8_t *list=at(b,w(r+24),h(r+20)*4);if(!list)return 0;
 const uint8_t *header=at(b,w(list+index*4),16);if(!header)return 0;
 unsigned segments=w(header);if(!segments || segments>4096)return 0;
 /* 0042D9F0/0042D998: a channel may contain several key blocks,
    selected by their first time; +4 is the EE's mutable search hint. */
 const uint8_t *blocks=at(b,w(list+index*4)+12,segments*4);if(!blocks)return 0;
 unsigned selected=0;const uint8_t *first_block=at(b,w(blocks),12);if(!first_block)return 0;
 if(time>f(first_block+8))for(unsigned i=1;i<segments;i++){
  const uint8_t *q=at(b,w(blocks+i*4),12);if(!q)return 0;if(time<f(q+8))break;selected=i;
 }
 unsigned data_offset=w(blocks+selected*4);const uint8_t *data=at(b,data_offset,4);if(!data)return 0;unsigned n=w(data);if(n<2 || n>4096)return 0;
 data=at(b,data_offset+4,(size_t)(n+1)*4);if(!data)return 0;
 /* 0042DAA0 -> 0042DC00 -> 004861A8: cubic keys, evaluated in the
    original Horner order (not a rearranged Bernstein polynomial). */
 unsigned count=(n-2)/4+1;const uint8_t *first=data+4;
 if(count==1){*value=f(first+4);return 1;}
 for(unsigned k=0;k+1<count;k++){
  const uint8_t *x=first+k*16,*y=x+16;if(k+2<count && time>f(y))continue;
  float s=(time-f(x))/(f(y)-f(x));
  float v=f(x+4),c1=(f(x+8)-f(x+12))*3.f,c2=(v-f(x+8))*3.f;
  *value=(((((f(y+4)-v)+c1)*s+(c2-c1))*s-c2)*s)+v;return 1;
 }*value=f(first+(count-1)*16+4);return 1;
}
int gt4_license_motion_curve(const gt4_license_motion *m,unsigned slot,unsigned index,float *out){
 unsigned g,a;float time;if(!out || !gt4_license_motion_sample(m,slot,&g,&a,&time))return 0;
 bank b=slot==m->camera_slot?m->camera:m->actors;const uint8_t*r=record(b,g,(int)a,0);
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);int ok=curve(b,r,index,time,out);fesetenv(&env);return ok;
}
static int components(bank b,const uint8_t *r,size_t *p,unsigned mask,unsigned count,float time,float *v){
 for(unsigned i=0;i<count;i++){
  const uint8_t *q=at(b,*p,(mask&(1u<<i))?2:4);if(!q)return 0;
  if(mask&(1u<<i)){if(!curve(b,r,h(q),time,v+i))return 0;*p+=2;}else{v[i]=f(q);*p+=4;}
 }return 1;
}
static void identity(float m[16]){for(unsigned i=0;i<16;i++)m[i]=i%5==0?1.f:0;}
static void multiply(float m[16],const float n[16]){float result[16];for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++){float s=0;for(unsigned k=0;k<4;k++)s+=m[k*4+r]*n[c*4+k];result[c*4+r]=s;}memcpy(m,result,sizeof result);}
static void euler_quaternion(const float e[3],float q[4]){
 /* 0048B888 Euler->quaternion then 004875B0, post-multiplied. */
 float sx=sinf(e[0]*.5f),cx=cosf(e[0]*.5f),sy=sinf(e[1]*.5f),cy=cosf(e[1]*.5f),sz=sinf(e[2]*.5f),cz=cosf(e[2]*.5f);
 q[0]=(sx*cy)*cz-(cx*sy)*sz;q[1]=(cx*sy)*cz+(sx*cy)*sz;q[2]=(cx*cy)*sz-(sx*sy)*cz;q[3]=(cx*cy)*cz+(sx*sy)*sz;
}
static void quaternion_rotate(float m[16],const float q[4]){
 float x=q[0],y=q[1],z=q[2],wq=q[3];
 float xx=2*x*x,yy=2*y*y,zz=2*z*z,xy=2*x*y,xz=2*x*z,yz=2*y*z,xw=2*x*wq,yw=2*y*wq,zw=2*z*wq,r[16];identity(r);
 r[0]=(1-yy)-zz;r[1]=xy+zw;r[2]=xz-yw;r[4]=xy-zw;r[5]=(1-zz)-xx;r[6]=yz+xw;r[8]=xz+yw;r[9]=yz-xw;r[10]=(1-xx)-yy;multiply(m,r);
}
static void rotation_blend(float m[16],const float a[3],const float b[3],float t){
 float from[4],to[4],q[4];euler_quaternion(a,from);euler_quaternion(b,to);
 /* 0048CA50 +80..354: shortest quaternion arc. libm sin/asin are
    explicit comparison boundaries, like the existing Euler sin/cos. */
 float dot=((from[0]*to[0]+from[1]*to[1])+from[2]*to[2])+from[3]*to[3];
 if(dot<0){for(unsigned i=0;i<4;i++)to[i]=-to[i];dot=-dot;}
 float half_pi;uint32_t bits=0x3fc90fda;memcpy(&half_pi,&bits,4);
 float angle=half_pi-asinf(fmaxf(-1,fminf(dot,1))),s=sinf(angle);
 if(!s){quaternion_rotate(m,from);return;}float inv=1.f/s,u=sinf(angle*(1-t))*inv,v=sinf(angle*t)*inv;
 for(unsigned i=0;i<4;i++)q[i]=from[i]*u+to[i]*v;quaternion_rotate(m,q);
}
static int blend_components(bank b,const uint8_t *r,const uint8_t *r2,size_t *p,unsigned mask,unsigned count,float time,float t,float *v,float *second){
 size_t start=*p;if(!components(b,r,p,mask,count,time,v))return 0;
 if(t>0 && t<1){if(!components(b,r2,&start,mask,count,time,second))return 0;}
 else memcpy(second,v,count*4);return 1;
}
static int evaluate(const gt4_license_motion *m,unsigned slot,float blend,const char *const *names,unsigned count,float (*frames)[16],float *look,float *fov,float *coord,const char *scalar,float *value){
 unsigned g,atoms[2];float time,factor;if(!gt4_license_motion_sample_blend(m,slot,blend,&g,atoms,&time,&factor))return -1;
 if(factor>=1){atoms[0]=atoms[1];factor=0;}if(factor<=0)factor=0;
 bank b=slot==m->camera_slot?m->camera:m->actors;const uint8_t *r=record(b,g,(int)atoms[0],0),*r2=record(b,g,(int)atoms[1],0),*group_data=group(b,g);
 size_t p=w(group_data+12);float frame[16],stack[32][16];unsigned depth=0,found=0;identity(frame);
 for(unsigned budget=0;budget<512;budget++){
  const uint8_t *op=at(b,p,1);if(!op)return -1;unsigned code=*op;p++;
  if(code==1)return depth?-1:(int)found;
  if(code==0)continue;
  if(code==2){if(depth==32)return -1;memcpy(stack[depth++],frame,64);continue;}
  if(code==3){if(!depth)return -1;memcpy(frame,stack[--depth],64);continue;}
  if(code==4 || code==16){
   unsigned mask=0;if(code==16){op=at(b,p++,1);if(!op)return -1;mask=*op;}
   const uint8_t *label=at(b,p,1),*end=label?memchr(label,0,b.size-p):NULL;if(!end)return -1;p=(size_t)(end-b.d)+1;
   if(code==4){for(unsigned i=0;i<count;i++)if(!strcmp((const char *)label,names[i])){memcpy(frames[i],frame,64);found++;}}
   else{float v,v2;if(!blend_components(b,r,r2,&p,mask,1,time,factor,&v,&v2))return -1;if(factor)v=v*(1-factor)+v2*factor;if(coord){if(!strcmp((const char *)label,"CameraCoord"))coord[0]=v;else if(!strcmp((const char *)label,"TargetCoord"))coord[1]=v;}if(scalar && !strcmp((const char *)label,scalar)){*value=v;found++;}}
   continue;
  }
  unsigned n=code==5 || code==19?3:code==11?9:code==10?1:0;if(!n)return -1;
  op=at(b,p,code==11?2:1);if(!op)return -1;unsigned mask=code==11?h(op):*op;p+=code==11?2:1;
  float v[9],v2[9];if(!blend_components(b,r,r2,&p,mask,n,time,factor,v,v2))return -1;
  /* 0042B288 preserves the original two products and sum. */
  if(code!=19 && factor)for(unsigned i=0;i<n;i++)v[i]=v[i]*(1-factor)+v2[i]*factor;
  if(code==5){float t[16];identity(t);memcpy(t+12,v,12);multiply(frame,t);}
  else if(code==19){if(factor)rotation_blend(frame,v,v2,factor);else{float q[4];euler_quaternion(v,q);quaternion_rotate(frame,q);}}
  else if(code==11 && look){memcpy(look,v,36);found++;}else if(code==10 && fov){*fov=v[0];found++;}
 }return -1;
}
int gt4_license_motion_frames(const gt4_license_motion *m,unsigned slot,const char *const *names,unsigned count,float (*frames)[16]){
 return gt4_license_motion_frames_blend(m,slot,0,names,count,frames);
}
int gt4_license_motion_frames_blend(const gt4_license_motion *m,unsigned slot,float blend,const char *const *names,unsigned count,float (*frames)[16]){
 if(!m || slot>=m->camera_slot || !names || !frames)return -1;
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);int found=evaluate(m,slot,blend,names,count,frames,NULL,NULL,NULL,NULL,NULL);fesetenv(&env);return found;
}
int gt4_license_motion_scalar(const gt4_license_motion *m,unsigned slot,float blend,const char *label,float *value){
 if(!m || slot>m->camera_slot || !label || !value)return 0;
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);int found=evaluate(m,slot,blend,NULL,0,NULL,NULL,NULL,NULL,label,value);fesetenv(&env);return found==1;
}
int gt4_license_motion_camera(const gt4_license_motion *m,float look[9],float *fov,float *camera_coord,float *target_coord){
 return gt4_license_motion_camera_blend(m,0,look,fov,camera_coord,target_coord);
}
int gt4_license_motion_camera_blend(const gt4_license_motion *m,float blend,float look[9],float *fov,float *camera_coord,float *target_coord){
 if(!m || !look || !fov || !camera_coord || !target_coord)return 0;float coord[2]={0};
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);int found=evaluate(m,m->camera_slot,blend,NULL,0,NULL,look,fov,coord,NULL,NULL);fesetenv(&env);
 if(found!=2)return 0;*camera_coord=coord[0];*target_coord=coord[1];return 1;
}
