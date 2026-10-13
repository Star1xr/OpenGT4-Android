#include "ps2_media.h"
#include "save_ps2.h"
#include "disc_replay.h"
#include "platform/platform.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
typedef struct item {struct item *next;uint8_t *data;size_t size;int kind;char name[96];} item;
typedef struct {gt4_disc_replay *replay;gt4_media_import *report;item *first,*last;} importer;
static uint32_t word(const uint8_t *p){return p[0]|p[1]<<8|p[2]<<16|(uint32_t)p[3]<<24;}
static void put(uint8_t *p,uint32_t n){for(int k=0;k<4;k++)p[k]=(uint8_t)(n>>(k*8));}
typedef struct {const uint8_t *p;size_t n,at;} film_reader;
static int film_bytes(film_reader *r,void *out,size_t n){if(n>r->n-r->at)return 0;if(out)memcpy(out,r->p+r->at,n);r->at+=n;return 1;}
static int film_string(film_reader *r,char *out,size_t cap){
 const uint8_t *end=memchr(r->p+r->at,0,r->n-r->at);if(!end)return 0;
 size_t n=end-(r->p+r->at);if(n>=cap)return 0;
 for(size_t k=0;k<n;k++)if(r->p[r->at+k]<32 || r->p[r->at+k]>126)return 0;
 memcpy(out,r->p+r->at,n);out[n]=0;r->at+=n+1;return 1;
}
static int film_section(film_reader *r,char tag[5],film_reader *body){
 if(r->n-r->at<8)return 0;const uint8_t *p=r->p+r->at;uint32_t n=word(p+4);
 if(n<8 || n>r->n-r->at)return 0;memcpy(tag,p,4);tag[4]=0;
 *body=(film_reader){p+8,n-8,0};r->at+=n;return 1;
}
static int film_float(film_reader *r,float *out){uint32_t n;if(!film_bytes(r,&n,4))return 0;memcpy(out,&n,4);return isfinite(*out);}
static int film_header(film_reader *r,gt4_film *f,int *frames){
 /* 0033AA58 reads RHDR: version, labels, car count and per-car paint.
    0033A1B0 serializes CDTL's four fitted model/part labels and byte. */
 uint8_t version,count;uint32_t flag;if(!film_bytes(r,&version,1) || version>2 || !film_string(r,f->course,sizeof f->course) || !film_string(r,f->race,sizeof f->race) || !film_bytes(r,&count,1) || !count || count>8)return 0;
 f->cars=count;for(int k=0;k<count;k++){uint8_t paint;
  if(!film_string(r,f->car[k].code,sizeof f->car[k].code) || !film_string(r,f->car[k].model,sizeof f->car[k].model) || !film_bytes(r,&paint,1))return 0;f->car[k].paint=paint;
 }/* 0033ABF4 -> 0045B1B8: frame count is a 32-bit integer. */
 if(!film_bytes(r,&flag,4) || flag<1 || flag>2)return 0;
 *frames=(int)flag;
 int detail=0;while(r->at<r->n){char tag[5];film_reader b;if(!film_section(r,tag,&b))return 0;
  if(!strcmp(tag,"ENVP")){if(b.n!=0x17f4 || f->has_environment)return 0;memcpy(f->environment,b.p,b.n);f->has_environment=1;}
  else if(!strcmp(tag,"CDTL")){uint8_t v;if(detail>=count || !film_bytes(&b,&v,1) || v>1)return 0;
   for(int k=0;k<4;k++)if(!film_string(&b,f->car[detail].parts[k],sizeof f->car[detail].parts[k]))return 0;
   /* 0033A998 -> 0045B118: signed byte, not a short. */
   if(v && !film_bytes(&b,NULL,1))return 0;
   if(b.at!=b.n)return 0;detail++;
  }else return 0;
 }return version<2 || detail==count;
}
static int film_parameters(film_reader *r,float out[91]){
 /* 0045A4A0 +48..238: serialized order differs from the native 0x16C
    parameter bank. First vector is angular pose, second is position;
    0045A778 / 0045A718 interpolate them differently. DRVR is the human
    skeleton, not the car transform. */
 uint8_t version;if(!film_bytes(r,&version,1) || version>3)return 0;
 uint8_t boolean;
 if(!film_bytes(r,out,60) || !film_bytes(r,(uint8_t *)out+0x10c,8) || !film_bytes(r,&boolean,1))return 0;
 /* 0045A558 -> 0045B1E0: a byte on disk, integer in the parameter bank. */
 uint32_t flag=boolean!=0;memcpy((uint8_t *)out+0x114,&flag,4);
 if(!film_bytes(r,(uint8_t *)out+0x118,16) || !film_bytes(r,(uint8_t *)out+0x3c,208) || !film_bytes(r,(uint8_t *)out+0x12c,16))return 0;
 if(version && (!film_bytes(r,(uint8_t *)out+0x128,4) || !film_bytes(r,(uint8_t *)out+0x13c,48)))return 0;
 for(int k=0;k<91;k++)if(k!=0x114/4 && !isfinite(out[k]))return 0;
 return r->at==r->n;
}
static int film_driver(film_reader *r,gt4_driver_skeleton *s,float grasp[2],uint8_t *has_grasp){
 /* 00339A78/0041B2E0: twenty serialized nodes; 0041BD08 reads
    position/quaternion/end, 0041C120 adds two branch origins to pelvis
    and scapula. The following 16 bytes and animation state are not IK. */
 uint8_t version;if(!film_bytes(r,&version,1) || version>1)return 0;
 for(int k=0;k<20;k++){
  gt4_driver_segment *b=&s->bone[k];
  for(int j=0;j<3;j++)if(!film_float(r,b->position+j))return 0;
  for(int j=0;j<4;j++)if(!film_float(r,b->rotation+j))return 0;
  for(int j=0;j<3;j++)if(!film_float(r,b->end+j))return 0;
  if(k==GT4_PELVIS || k==GT4_SCAPULA)for(int j=0;j<6;j++)if(!film_float(r,&b->branch[0][0]+j))return 0;
  b->length=sqrtf(b->end[0]*b->end[0]+b->end[1]*b->end[1]+b->end[2]*b->end[2]);
  b->inverse_length=b->length?1/b->length:0;
 }
 if(!film_bytes(r,NULL,16))return 0;
 /* 00339CF8: v0 ends here; v1 adds three byte booleans and 21 floats. */
 if(version){
  /* 00339CF8 +A4..114: two mode flags, channels, third flag, then
     recorded finger closure at DrivingPosture +58/+5C. */
  uint8_t flag;float ignored;
  if(!film_bytes(r,&flag,1) || !film_float(r,&ignored) || !film_bytes(r,&flag,1))return 0;
  for(int j=0;j<18;j++)if(!film_float(r,&ignored))return 0;
  if(!film_bytes(r,&flag,1) || !film_float(r,grasp) || !film_float(r,grasp+1))return 0;
  *has_grasp=1;
 }return r->at==r->n;
}
static int film_frame(film_reader *r,gt4_film_frame *f,int cars){
 int camera=0;while(r->at<r->n){char tag[5];film_reader b;if(!film_section(r,tag,&b))return 0;
  if(!strcmp(tag,"PCAM")){
   /* 0033ADE8: byte version then 17 floats. 0033AEA0 constructs eye,
      target and up vectors, FOV/aspect/clip planes and four lens values. */
   uint8_t v;if(camera || !film_bytes(&b,&v,1) || v!=1 || b.n!=69)return 0;
   for(int k=0;k<17;k++)if(!film_float(&b,f->camera+k))return 0;camera=1;
  }else if(!strcmp(tag,"CARD")){
   if(f->cars>=cars)return 0;int parameters=0;
   while(b.at<b.n){film_reader c;char sub[5];if(!film_section(&b,sub,&c))return 0;
    if(!strcmp(sub,"CPRM")){if(parameters || !film_parameters(&c,f->parameters[f->cars]))return 0;parameters=1;}
    else if(!strcmp(sub,"DRVR")){if(f->has_driver[f->cars] || !film_driver(&c,&f->driver[f->cars],f->driver_grasp[f->cars],&f->has_driver_grasp[f->cars]))return 0;f->has_driver[f->cars]=1;}
    else if(!strcmp(sub,"SMOK")){
     /* 003E91E0/003E93D0: version byte, u16 count, 33-byte records. */
     uint8_t version;uint16_t count;if(!film_bytes(&c,&version,1) || version || !film_bytes(&c,&count,2) || count>256-f->smoke_count || c.n-c.at!=(size_t)count*33)return 0;
     for(int k=0;k<count;k++){const uint8_t *a=c.p+c.at;if(a[1]>=6)return 0;for(int j=0;j<4;j++){float x;memcpy(&x,a+14+j*4,4);if(!isfinite(x))return 0;}memcpy(f->smoke[f->smoke_count++],a,33);c.at+=33;}
    }
    /* Other CARD sections are original driver/particle snapshots. Kept
       in the lossless source; renderer adapters are separately audited. */
   }if(!parameters)return 0;f->cars++;
  }else if(!strcmp(tag,"PTCL")){
   /* 003C2990: version byte, particle type+1, records, zero terminator.
      LeafParticle virtual writer 0040B690: kind, 3 angles, XYZ, shade.
      SparkParticle is disabled by 003C2B20 (D_00621B50 +A8). */
   uint8_t version,type;if(!film_bytes(&b,&version,1) || version)return 0;
   for(;;){if(!film_bytes(&b,&type,1))return 0;if(!type)break;
    if(type!=2 || f->leaf_count>=128 || b.n-b.at<17)return 0;
    const uint8_t *a=b.p+b.at;if(a[0]>=8)return 0;
    for(int j=0;j<3;j++){float x;memcpy(&x,a+4+j*4,4);if(!isfinite(x))return 0;}
    memcpy(f->leaves[f->leaf_count++],a,17);b.at+=17;
   }if(b.at!=b.n)return 0;
  }
 }return camera && f->cars==cars;
}
int gt4_media_film_decode(const uint8_t *p,size_t n,gt4_film *out){
 if(!out || !gt4_media_film_valid(p,n))return 0;gt4_film f={0};film_reader r={p,n,9};char tag[5];film_reader b;int frames;
 if(!film_section(&r,tag,&b) || strcmp(tag,"FILM"))return 0;
 /* 00335248 +E8..214: version, dimensions, tiles, aspect, portrait,
    samples, scene flag, exposure (v2), six lens and eight develop words. */
 uint8_t version,tiles[2],portrait,samples,scene;int16_t wh[2];
 /* 0045B168 signed short width/height; 0045B118 signed byte tile counts
    and samples; 0045B1E0 byte booleans. 00335048..003350A4 writes them. */
 if(!film_bytes(&b,&version,1) || !film_bytes(&b,wh,4) || !film_bytes(&b,tiles,2) || !film_float(&b,&f.aspect) || !film_bytes(&b,&portrait,1) || !film_bytes(&b,&samples,1) || !film_bytes(&b,&scene,1))return 0;
 f.exposure=1;if(version>=2 && !film_float(&b,&f.exposure))return 0;
 if(!film_bytes(&b,f.lens,24) || !film_bytes(&b,f.develop,32) || b.at!=b.n)return 0;
 for(int k=2;k<6;k++)if(!isfinite(f.lens[k]))return 0;
 if(!tiles[0] || tiles[0]>127 || !tiles[1] || tiles[1]>127 || wh[0]<=0 || wh[1]<=0 || wh[0]>4096 || wh[1]>4096 || f.aspect<=0 || portrait>1 || !samples || samples>127 || f.exposure<0)return 0;
 if(scene>1)return 0;
 f.width=wh[0];f.height=wh[1];f.portrait=portrait;f.samples=samples;f.scene=scene;
 if(!film_section(&r,tag,&b) || strcmp(tag,"RHDR") || !film_header(&b,&f,&frames))return 0;
 while(r.at<r.n && r.n-r.at>=8 && word(r.p+r.at)){
  if(!film_section(&r,tag,&b) || strcmp(tag,"SFRM") || f.frames>=2 || !film_frame(&b,&f.frame[f.frames],f.cars))return 0;f.frames++;
 }if(f.frames!=frames)return 0;*out=f;return 1;
}
int gt4_media_film_valid(const uint8_t *p,size_t n){
 /* 0045B5C8 +1C..3C: four-byte tag + little-endian section length,
    including header. 00334FC8 emits PMDI, FILM, RHDR and SFRM sections.
    FILM's version is one byte (0045B0F8), not a host integer. */
 /* 00335010..24 -> 0045AF58 writes one boolean byte. Sections are
    packed without padding (0045B548 +28..64); PMDI is nine bytes. */
 if(!p || n<9 || n>0x14000 || memcmp(p,"PMDI",4) || word(p+4)!=9 || p[8]>1)return 0;
 size_t at=9;int film=0,header=0,frames=0;
 while(at<n){
  if(n-at<8){for(;at<n;at++)if(p[at])return 0;break;}
  if(!word(p+at)){for(;at<n;at++)if(p[at])return 0;break;}
  uint32_t length=word(p+at+4);if(length<8 || length>n-at)return 0;
  /* 00334FC8: v1 body 70 bytes, v2 adds four-byte exposure. */
  if(!memcmp(p+at,"FILM",4)){if(film || length<9 || p[at+8]<1 || p[at+8]>2 || length<(p[at+8]==1?78u:82u))return 0;film=1;}
  else if(!memcmp(p+at,"RHDR",4)){if(!film || header || length<12)return 0;header=1;}
  else if(!memcmp(p+at,"SFRM",4)){if(!header || length<8 || ++frames>2)return 0;}
  else return 0;
  at+=length;
 }return film && header && frames;
}
void gt4_media_film_color(const gt4_film *f,float out[6]){
 /* 00334B28 +24..34 -> 003EFC10: EV, contrast, saturation, Kelvin.
    +38..14C: filter 1 scales saturation by 0.6, 2 is monochrome,
    4 composes the mature coefficients using 003EFD80. */
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 float gain=f->lens[3]!=0?powf(2,f->lens[3]):1;
 float delta=((f->lens[4]-5500)*0.19999998807907104f)/2000;
 out[0]=gain*(1-delta);out[1]=gain;out[2]=gain*(1+delta);out[3]=0;
 out[4]=1-f->lens[5]*0.09999999403953552f;out[5]=f->lens[2];
 uint32_t filter;memcpy(&filter,f->lens,4);
 if(filter==1)out[5]*=.59999996423721313f;
 else if(filter==2)out[5]=0;
 else if(filter==4){out[0]*=1.5f;out[1]*=.94999998807907104f;out[2]*=.59999996423721313f;out[5]*=.29999998211860657f;}
 fesetenv(&env);
}
static int accept(void *user,const char *name,const uint8_t *raw,size_t n){
 importer *i=user;uint8_t *data=NULL;size_t size=0;int kind=0;
 int picture=strlen(name)==5 && name[0]=='P' && strspn(name+1,"0123456789")==4;
 int film=strlen(name)==5 && name[0]=='F' && strspn(name+1,"0123456789")==4;
 if(picture || film){
  /* 001D0CC0 -> 001D4E60, PhotoData keys; 001D0828 uses FilmData keys.
     Rounded swap region 64, original update variant 1, CRC mandatory. */
  uint8_t *plain=NULL;size_t length=0;
  if(!gt4_ps2_unwrap(raw,n,film?0x3f278194:0x3f2e4f76,film?0x3eb0fcb4:0x3eced916,64,1,&plain,&length))goto invalid;
  if(film){gt4_film scene;if(!gt4_media_film_decode(plain,length,&scene)){free(plain);goto invalid;}data=plain;size=length;kind=3;}
  else {int ok=length>=4 && plain[0]==255 && plain[1]==216 && plat_jpeg_png(plain,length,&data,&size);free(plain);if(!ok)goto invalid;kind=2;}
 }else {
  gt4_replay_file r;char error[128];
  if(!gt4_disc_replay_decode(i->replay,name,raw,n,&r,error,sizeof error)){
   /* Unsupported replay classes remain skipped, not a playable success.
      The native world adapter must be implemented before accepting them. */
   i->report->skipped++;return 0;
  }
  gt4_replay_free(&r);if(n>GT4_SAVE_LIMIT-8)goto invalid;
  size=n+8;data=malloc(size);if(!data)goto invalid;memcpy(data,"PS2R",4);put(data+4,(uint32_t)n);memcpy(data+8,raw,n);kind=1;
 }
 item *entry=calloc(1,sizeof *entry);if(!entry){free(data);goto invalid;}
 entry->data=data;entry->size=size;entry->kind=kind;
 unsigned hash=(unsigned)mz_crc32(0,raw,n);
 snprintf(entry->name,sizeof entry->name,"%s_ps2_%08x.%s",kind==1?"replay":kind==2?"photo":"film",hash,kind==1?"ogt4":kind==2?"png":"ogt4film");
 if(i->last)i->last->next=entry;else i->first=entry;i->last=entry;return 1;
invalid:snprintf(i->report->error,sizeof i->report->error,"Cannot decode GT4 media file %s.",name);return -1;
}
int gt4_media_import_ps2(gt4_vol *vol,const char *path,gt4_media_import *report){
 if(!report)return 0;memset(report,0,sizeof *report);importer i={0};i.report=report;i.replay=gt4_disc_replay_open(vol);
 if(!i.replay){snprintf(report->error,sizeof report->error,"Cannot open the game's media decoder.");return 0;}
 char error[256];int found=gt4_ps2_visit_media(path,accept,&i,error,sizeof error),ok=found>0;
 if(!ok && !report->error[0])snprintf(report->error,sizeof report->error,"%s",error);
 /* Decode all inputs before writing: malformed input cannot publish a
    partial import. A subsequent I/O failure may leave earlier files;
    report counters identify them. Source bytes are never changed. */
 for(item *p=i.first;ok && p;p=p->next){
  char folder[1200],dest[1300];snprintf(folder,sizeof folder,"%s/%s",gt4_save_directory(),p->kind==1?"replays":p->kind==2?"photos":"films");
  snprintf(dest,sizeof dest,"%s/%s",folder,p->name);
  ok=gt4_save_directory()[0] && gt4_save_mkdir(folder) && (p->kind==2?plat_file_write(dest,p->data,p->size):gt4_save_write(dest,p->data,p->size));
  if(!ok)snprintf(report->error,sizeof report->error,"Could not store imported media. Check the save folder.");
  else if(p->kind==1)report->replays++;else if(p->kind==2)report->photos++;else report->films++;
 }
 while(i.first){item *p=i.first;i.first=p->next;free(p->data);free(p);}gt4_disc_replay_close(i.replay);return ok;
}
