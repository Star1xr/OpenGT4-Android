#include "career_home.h"
#include "vol.h"
#include "inflate.h"
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned cap(unsigned x,unsigned max){return x<max?x:max;}
static int32_t signed32(uint32_t bits){int32_t value;memcpy(&value,&bits,4);return value;}
static const uint8_t *mapped(const uint8_t *data,size_t n,uint32_t address,size_t size){
 for(size_t at=0x12c;at<n;){if(n-at<8)return NULL;uint32_t base=u32(data+at),length=u32(data+at+4);at+=8;if(length>n-at)return NULL;
  if(base<=address && (uint64_t)address+size<=(uint64_t)base+length)return data+at+address-base;at+=length;}return NULL;
}
int gt4_career_home_data_load(const char *iso,gt4_career_home_data *out){
 size_t size;uint8_t *raw=gt4_disc_read_root(iso,"CORE.GT4",&size),*decoded=NULL;int ok=0;
 if(!raw || size<6 || raw[0]!=1 || raw[1]!=1)goto done;
 uint32_t n=u32(raw+2);if(n<0x12c || n>32u*1024*1024)goto done;
 decoded=malloc(n);if(!decoded || core_inflate_raw(raw+6,size-6,decoded,n)!=n || u32(decoded+0x100)!=0x176265b3)goto done;
 for(size_t at=0x12c;at<n;){if(n-at<8)goto done;uint32_t base=u32(decoded+at),length=u32(decoded+at+4);at+=8;if(length>n-at)goto done;
  if(base<=0x623e40 && (uint64_t)base+length>=0x623e48){memcpy(out->weights,decoded+at+0x623e40-base,8);ok=1;}at+=length;
 }
 if(ok){
  /* 0042EE70 event names; 0042F790/+0x40 initialize the two enums. */
  const uint32_t bases[]={0x622d58,0x6a5138,0x6a51f8};const unsigned counts[]={9,5,8};
  char (*names[3])[24]={out->event_names,out->car_event_names,out->course_event_names};
  for(unsigned group=0;group<3;group++)for(unsigned i=0;i<counts[group];i++){
   const uint8_t *entry=mapped(decoded,n,bases[group]+i*(group?8:4),group?8:4);
   if(!entry || (group && u32(entry)!=i)){ok=0;goto done;}
   const uint8_t *name=mapped(decoded,n,u32(entry+(group?4:0)),24);
   if(!name || !memchr(name,0,24)){ok=0;goto done;}strcpy(names[group][i],(const char *)name);
  }
 }
done:free(decoded);free(raw);return ok;
}
/* 00467BE8/00467C20: triangular domains of the eight classes. */
static void range(unsigned i,unsigned *first,unsigned *last){*first=i<5?0:i-4;*last=i<3?i+5:8;}
/* 00467C58, 00467D90, 00467F60, 004680D8. Read-only Home
   statistics; none of this changes the driver's behaviour or race dynamics. */
void gt4_career_bspec_status(const gt4_career_home_data *d,const uint8_t e[0x180],int32_t out[4]){
 unsigned machine=0,course=0,battle_sum=0,battle_den=0,total_sum=0,total_den=0;
 for(unsigned i=0;i<8;i++){
  unsigned first,last;range(i,&first,&last);unsigned mn=0,md=0,cn=0,cd=0,w=d->weights[i];
  for(unsigned j=first;j<last;j++){
   unsigned at=2*(j*8+i),mw=d->weights[j];mn+=mw*(cap(u16(e+at),1024)+cap(u16(e+at+0x80),1024));md+=mw*2048;
   at=2*(i*8+j);unsigned a=cap(u16(e+at),1024),b=cap(u16(e+at+0x80),1024),c=cap(u16(e+at+0x100),256);
   cn+=w*(a+b);cd+=w*2048;battle_sum+=w*c;battle_den+=w*256;
   total_sum+=w*(a+b+cap((c*4)&65535,1024));total_den+=w*3072;
  }
  machine+=125*mn/md;course+=125*cn/cd;
 }
 unsigned straight=1000*u16(e+0xf0)/59160,traffic=1000*u16(e+0x170)/59160;
 out[1]=(int32_t)(cap(machine,1000)/10);
 out[2]=(int32_t)(cap((70*course+20*straight+10*traffic)/100,1000)/10);
 out[3]=(int32_t)(cap((70*(1000*battle_sum/battle_den)+30*traffic)/100,1000)/10);
 out[0]=(int32_t)cap((70*(unsigned)((uint64_t)10000*total_sum/total_den)+20*(10000*u16(e+0xf0)/59160)+10*(10000*u16(e+0x170)/59160))/100,10000);
}
/* 0043B018 +0x10..0x2c, seven counters with 32-bit wrap. */
int32_t gt4_career_home_race_count(const uint32_t p[7]){uint32_t sum=0;for(unsigned i=0;i<7;i++)sum+=p[i];return signed32(sum);}
/* 0042F060 +0x30..0x70: today's/future events are not history. */
int32_t gt4_career_history_index(int32_t today,int32_t requested,unsigned count){int32_t age=signed32((uint32_t)today-(uint32_t)requested);return age>0 && age<=(int32_t)count?(int32_t)count-age:-1;}
/* 0042EA80/0042EA58: sorted 64-bit SpecDB keys; +0x2408 bypass. */
int gt4_career_course_available(const uint64_t courses[128],uint64_t key,int all){if(all)return 1;unsigned lo=0,hi=128;while(lo<hi){unsigned mid=(lo+hi)/2;if(courses[mid]<key)lo=mid+1;else hi=mid;}return lo<128 && courses[lo]==key;}
/* 0042E968/0042EA38: insertion retains the first 128 sorted keys. */
void gt4_career_course_unlock(uint64_t courses[128],uint64_t key){unsigned i=0;while(i<128 && courses[i]<key)i++;if(i==128 || courses[i]==key)return;memmove(courses+i+1,courses+i,(127-i)*8);courses[i]=key;}
/* 001640D0 +0x60..0x78, 00164268 +0x60..0x78 -> 0057F480:
   signed u64 bits -> single float -> 00472C28 unit -> 0057F360 truncation.
   The large-integer helper truncates its mantissa instead of host rounding. */
int64_t gt4_career_home_distance(uint64_t metres,int miles){
 uint32_t sign=(uint32_t)(metres>>63)<<31;uint64_t magnitude=sign?0-metres:metres;float input;
 if(magnitude<UINT64_C(0x80000000)){int32_t small=(int32_t)magnitude;input=sign?-(float)small:(float)small;}
 else {unsigned exponent=0;for(uint64_t v=magnitude;v>>1;v>>=1)exponent++;uint32_t bits=sign|((exponent+127)<<23)|((uint32_t)(magnitude>>(exponent-23))&0x7fffff);memcpy(&input,&bits,4);}
 uint32_t bits=miles?0x3a22e4c6:0x3a83126e;float factor;memcpy(&factor,&bits,4);float converted=input*factor;return (int64_t)converted;
}
