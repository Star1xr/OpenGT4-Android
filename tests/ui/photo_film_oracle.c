/* Original FILM/PCAM writers and CPRM reader are independent oracles.
   Scene records are authored, never captured game bytes. */
#include "ee.h"
#include "ps2_media.h"
#include "photo_mode.h"
#include "film_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"film oracle %d: %s\n",__LINE__,#x);return 1;}}while(0)
ee_fn recomp_lookup(uint32_t);
static uint8_t fixture[2048];
static size_t header,frame[2],end;
static unsigned rd(const uint8_t *p){return p[0]|p[1]<<8|p[2]<<16|(unsigned)p[3]<<24;}
static void append(ee_ctx *e,size_t at,size_t n){
 unsigned stream=e->r[4].uw[0],cursor=rd(e->ram+stream+8);
 memcpy(e->ram+cursor,fixture+at,n);ff_word(e->ram+stream+8,cursor+(unsigned)n);
}
static void header_hook(ee_ctx *e){
 /* Hook only world traversal; serialization metadata remains original. */
 unsigned count=e->r[6].uw[0];size_t h=header+8;
 h++;while(fixture[h++]);while(fixture[h++]);h++;while(fixture[h++]);while(fixture[h++]);h++;
 ff_word(fixture+h,count);append(e,header,frame[0]-header);
}
static void frame_hook(ee_ctx *e){int k=e->r[6].uw[0]?1:0;append(e,frame[k],(k?end:frame[1])-frame[k]);}
static float original_color[6];
static void color_hook(ee_ctx *e){memcpy(original_color,e->ram+e->r[5].uw[0],24);}
static void power_hook(ee_ctx *e){e->fpr[0].f=powf(e->fpr[12].f,e->fpr[13].f);}
/* 0048D3E0 is only VU0 vadd.xyzw. The diagnostic has no VU registers;
   substitute that decoded hardware operation, not skeleton interpolation. */
static void vu_add(ee_ctx *e){
 float a[4],b[4],out[4];memcpy(a,e->ram+e->r[5].uw[0],16);memcpy(b,e->ram+e->r[6].uw[0],16);
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);for(int k=0;k<4;k++)out[k]=a[k]+b[k];fesetenv(&env);memcpy(e->ram+e->r[4].uw[0],out,16);
}
static ee_fn hook(uint32_t pc){return pc==0x48d3e0?vu_add:pc==0x33a2a0?header_hook:pc==0x33a018?frame_hook:pc==0x33de30?color_hook:pc==0x57d6c0?power_hook:NULL;}
static ee_fn lookup(uint32_t pc){ee_fn f=hook(pc);return f?f:recomp_lookup(pc);}
static int call(ee_ctx *e,unsigned pc,unsigned a,unsigned b,unsigned c){
 memset(e->r,0,sizeof e->r);e->r[4].uw[0]=a;e->r[5].uw[0]=b;e->r[6].uw[0]=c;e->r[29].uw[0]=0x1eff000;
 jmp_buf j;e->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"original stopped %08x: %s\n",e->end_pc,e->end_detail);return 0;}recomp_lookup(pc)(e);return 1;
}
int main(int argc,char **argv){
 CHECK(argc>=2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0);e.lookup=lookup;e.hook=hook;
 end=film_fixture(fixture,sizeof fixture);CHECK(end);header=9+rd(fixture+13);frame[0]=header+rd(fixture+header+4);frame[1]=frame[0]+rd(fixture+frame[0]+4);
 const unsigned obj=0x1800000,data=0x1810000,stream=0x1820000,bank=0x1830000;
 for(int t=0;t<256;t++){
  memset(e.ram+obj,0,0x200);int width=320+t*11,height=240+t*7,portrait=t&1,samples=t%127+1;
  ff_word(e.ram+obj+4,width);ff_word(e.ram+obj+8,height);ff_word(e.ram+obj+12,t%8+1);ff_word(e.ram+obj+16,t%5+1);
  ff_float(e.ram+obj+20,(float)width/height);ff_word(e.ram+obj+24,portrait);ff_word(e.ram+obj+0x118,samples);
  ff_word(e.ram+obj+0x124,t&1);ff_float(e.ram+obj+0x120,t&1?.75f:0);
  for(int k=0;k<14;k++)ff_float(e.ram+obj+0x128+k*4,(t+k)*.125f);
  CHECK(call(&e,0x334fc8,obj,data,0x14000));unsigned n=e.r[2].uw[0];gt4_film f;
  CHECK(rd(e.ram+data+4)==9 && rd(e.ram+data+13)==82);
  CHECK(gt4_media_film_decode(e.ram+data,n,&f));
  if(argc>2 && !strcmp(argv[2],"--negative") && !t)f.width++;
  CHECK(f.width==width && f.height==height && f.portrait==portrait && f.samples==samples && f.frames==(t&1?2:1));
  CHECK(f.aspect==(float)width/height && f.exposure==(t&1?.75f:0));
  CHECK(!memcmp(f.lens,e.ram+obj+0x128,24) && !memcmp(f.develop,e.ram+obj+0x144,32));
  /* PCAM stores seventeen floats verbatim, including the four lens fields. */
  memcpy(e.ram+obj,fixture+frame[1]+17,68);ff_float(e.ram+obj,(float)t);
  ff_word(e.ram+stream,data);ff_word(e.ram+stream+4,data+1024);ff_word(e.ram+stream+8,data);
  CHECK(call(&e,0x33ade8,obj,stream,0));CHECK(rd(e.ram+data+4)==77 && e.ram[data+8]==1 && !memcmp(e.ram+data+9,e.ram+obj,68));
  /* Exercise all CPRM versions with a byte boolean and shuffled banks. */
  unsigned version=t%4;size_t at=0,b=ff_begin(e.ram+data,&at,"CPRM");e.ram[data+at++]=(uint8_t)version;
  size_t lengths[]={60,8,1,16,208,16,4,48};
  for(int block=0;block<(version?8:6);block++){
   if(block==2){e.ram[data+at++]=(uint8_t)(t&1);continue;}
   for(size_t k=0;k<lengths[block];k+=4){ff_float(e.ram+data+at,(t+at)*.0625f);at+=4;}
  }ff_end(e.ram+data,at,b);
  ff_word(e.ram+stream,data);ff_word(e.ram+stream+4,data+(unsigned)at);ff_word(e.ram+stream+8,data);memset(e.ram+bank,0,364);
  CHECK(call(&e,0x45a4a0,bank,stream,0));
  /* Install the exact original CPRM in an otherwise authored film. */
  uint8_t test[4096];size_t total=film_fixture(test,sizeof test),params=frame[1]+77+8+8;
  size_t old=rd(test+params+4),tail=total-params-old;memmove(test+params+at,test+params+old,tail);memcpy(test+params,e.ram+data,at);
  ff_word(test+params-4,(unsigned)(8+at));ff_word(test+frame[1]+4,(unsigned)(8+77+8+at));total=total-old+at;
  CHECK(gt4_media_film_decode(test,total,&f));CHECK(!memcmp(f.frame[1].parameters[0],e.ram+bank,364));
  /* Human node bytes come from the original virtual writers, including
     the extra origins of pelvis and scapula; no synthesized IK channels. */
  size_t driver=total,body=ff_begin(test,&total,"DRVR");test[total++]=1;
  gt4_driver_skeleton expected={0};
  for(int k=0;k<20;k++){
   memset(e.ram+obj,0,0x80);gt4_driver_segment *bone=&expected.bone[k];
   for(int j=0;j<3;j++){bone->position[j]=(t+k+j)*.0625f;ff_float(e.ram+obj+j*4,bone->position[j]);bone->end[j]=(k+j+1)*.015625f;ff_float(e.ram+obj+0x30+j*4,bone->end[j]);}
   for(int j=0;j<4;j++){bone->rotation[j]=j==3?1:0;ff_float(e.ram+obj+0x10+j*4,bone->rotation[j]);}
   int branch=k==GT4_PELVIS || k==GT4_SCAPULA;
   for(int j=0;j<6;j++){bone->branch[j/3][j%3]=(t+j)*.03125f;ff_float(e.ram+obj+0x50+(j/3)*16+(j%3)*4,bone->branch[j/3][j%3]);}
   CHECK(call(&e,branch?0x41c0b8:0x41bcc0,obj,data,0));unsigned bytes=e.r[2].uw[0]-data;CHECK(bytes==(branch?64:40));
   memcpy(test+total,e.ram+data,bytes);total+=bytes;
  }
  memset(test+total,0,103);ff_float(test+total+95,t*.00390625f);ff_float(test+total+99,(255-t)*.00390625f);total+=103;ff_end(test,total,body);
  ff_word(test+params-4,rd(test+params-4)+(unsigned)(total-driver));ff_word(test+frame[1]+4,rd(test+frame[1]+4)+(unsigned)(total-driver));
  CHECK(gt4_media_film_decode(test,total,&f) && f.frame[1].has_driver[0]);CHECK(f.frame[1].has_driver_grasp[0] && f.frame[1].driver_grasp[0][0]==t*.00390625f && f.frame[1].driver_grasp[0][1]==(255-t)*.00390625f);
  if(argc>2 && !strcmp(argv[2],"--driver-negative") && !t)f.frame[1].driver[0].bone[0].position[0]+=1;
  for(int k=0;k<20;k++){const gt4_driver_segment *a=&expected.bone[k],*b=&f.frame[1].driver[0].bone[k];CHECK(!memcmp(a->position,b->position,12) && !memcmp(a->rotation,b->rotation,16) && !memcmp(a->end,b->end,12));if(k==GT4_PELVIS || k==GT4_SCAPULA)CHECK(!memcmp(a->branch,b->branch,24));}
  /* Original particle virtual writers produce the bytes under test. */
  size_t particles=total,section=ff_begin(test,&total,"SMOK");test[total++]=0;test[total++]=1;test[total++]=0;
  memset(e.ram+obj,0,0x80);e.ram[obj+2]=(uint8_t)t;e.ram[obj+3]=(uint8_t)(t%6);ff_word(e.ram+obj+4,0x80402010);
  for(int j=0;j<10;j++)ff_float(e.ram+obj+8+j*4,(t+j)*.03125f);
  e.ram[obj+0x1c]=64;e.ram[obj+0x1d]=1;e.ram[obj+0x1f]=(uint8_t)t;ff_float(e.ram+obj+0x34,1);
  ff_word(e.ram+stream,data);ff_word(e.ram+stream+4,data+1024);ff_word(e.ram+stream+8,data);
  CHECK(call(&e,0x3e93d0,obj,stream,0));CHECK(rd(e.ram+stream+8)==data+33);
  uint8_t smoke[33];memcpy(smoke,e.ram+data,33);memcpy(test+total,smoke,33);total+=33;ff_end(test,total,section);
  ff_word(test+params-4,rd(test+params-4)+(unsigned)(total-particles));
  ff_word(test+frame[1]+4,rd(test+frame[1]+4)+(unsigned)(total-particles));
  particles=total;section=ff_begin(test,&total,"PTCL");test[total++]=0;test[total++]=2;
  memset(e.ram+obj,0,0x80);ff_word(e.ram+obj+0x4c,bank+12+(t%8)*0x74);
  for(int j=0;j<6;j++)ff_float(e.ram+obj+8+j*4,(t+j)*.015625f);ff_float(e.ram+obj+0x48,(t%256)/255.f);
  ff_word(e.ram+stream,data);ff_word(e.ram+stream+4,data+1024);ff_word(e.ram+stream+8,data);
  CHECK(call(&e,0x40b690,obj,stream,bank));CHECK(rd(e.ram+stream+8)==data+17);
  uint8_t leaf[17];memcpy(leaf,e.ram+data,17);memcpy(test+total,leaf,17);total+=17;test[total++]=0;ff_end(test,total,section);
  ff_word(test+frame[1]+4,rd(test+frame[1]+4)+(unsigned)(total-particles));
  CHECK(gt4_media_film_decode(test,total,&f));
  if(argc>2 && !strcmp(argv[2],"--particle-negative") && !t)f.frame[1].leaves[0][4]^=1;
  CHECK(f.frame[1].smoke_count==1 && !memcmp(f.frame[1].smoke[0],smoke,33));
  CHECK(f.frame[1].leaf_count==1 && !memcmp(f.frame[1].leaves[0],leaf,17));
  /* An invalid type/finite position fails atomically, not halfway into a scene. */
  test[section+9]=255;gt4_film unchanged=f;CHECK(!gt4_media_film_decode(test,total,&f) && !memcmp(&f,&unchanged,sizeof f));test[section+9]=2;
  float previous=(t-128)*57.125f,current=(127-t)*43.25f,time=(t%17)*.0625f;
  e.fpr[12].f=previous;e.fpr[13].f=current;e.fpr[14].f=time;CHECK(call(&e,0x4594d0,0,0,0));
  float angle=gt4_photo_angle(current,previous,time);if(argc>2 && !strcmp(argv[2],"--wheel-negative") && !t)angle+=1;CHECK(!memcmp(&angle,&e.fpr[0].f,4));
  float wheel_height=(t-128)*.015625f,limit=t%3?(t-96)*.0078125f:0,radius=(t%31)*.00390625f;
  memset(e.ram+obj,0,0x200);ff_word(e.ram+obj,bank);ff_word(e.ram+bank+0x10,stream);
  ff_float(e.ram+stream+0x58+(t%4>=2?4:0),limit);ff_float(e.ram+obj+0x94+(t%4)*0x38+0x20,radius);
  e.fpr[12].f=1;e.fpr[13].f=wheel_height;e.fpr[14].f=3;CHECK(call(&e,0x4514a0,obj,t%4,0));
  float hub_y=gt4_photo_wheel_height(wheel_height,limit,radius);if(argc>2 && !strcmp(argv[2],"--suspension-negative") && !t)hub_y+=1;
  CHECK(!memcmp(&hub_y,e.ram+obj+0x98+(t%4)*0x38,4));
  /* Exercise original complete skeleton interpolation, including both
     branching nodes and antipodal quaternions, not just frozen DRVR bytes. */
  unsigned offsets[20]={0,0x70,0x110,0x1b0,0xc0,0x160,0x200,0x250,0x2a0,0x2f0,0x360,0x400,0x3b0,0x450,0x4a0,0x4f0,0x540,0x590,0x5e0,0x630};
  gt4_driver_skeleton first={0},second={0},mixed;
  memset(e.ram+obj,0,0x700);memset(e.ram+bank,0,0x700);memset(e.ram+data,0,0x700);
  for(int k=0;k<20;k++)for(int side=0;side<2;side++){
   gt4_driver_segment *bone=side?&second.bone[k]:&first.bone[k];uint8_t *original=e.ram+(side?bank:obj)+offsets[k];
   for(int j=0;j<3;j++){bone->position[j]=(t-128+k+j)*(side?.03125f:.0625f);bone->end[j]=(k+j+side+1)*.015625f;ff_float(original+j*4,bone->position[j]);ff_float(original+0x30+j*4,bone->end[j]);}
   bone->rotation[side?(k%3):3]=side?.6f:1; if(side)bone->rotation[3]=.8f;
   if(side && (t&1))for(int j=0;j<4;j++)bone->rotation[j]=-bone->rotation[j];memcpy(original+0x10,bone->rotation,16);
   if(k==GT4_PELVIS || k==GT4_SCAPULA)for(int j=0;j<6;j++){bone->branch[j/3][j%3]=(t+j-side)*.03125f;ff_float(original+0x50+(j/3)*16+(j%3)*4,bone->branch[j/3][j%3]);}
  }
  e.fpr[12].f=time;CHECK(call(&e,0x41b048,data,obj,bank));CHECK(gt4_driver_skeleton_blend(&mixed,&first,&second,time));
  if(argc>2 && !strcmp(argv[2],"--driver-blend-negative") && !t)mixed.bone[0].position[0]+=1;
  for(int k=0;k<20;k++){const gt4_driver_segment *bone=&mixed.bone[k];const uint8_t *original=e.ram+data+offsets[k];
   CHECK(!memcmp(bone->position,original,12) && !memcmp(bone->rotation,original+0x10,16) && !memcmp(bone->end,original+0x30,12));
   CHECK(!memcmp(&bone->length,original+0x40,4) && !memcmp(&bone->inverse_length,original+0x44,4));
   if(k==GT4_PELVIS || k==GT4_SCAPULA)CHECK(!memcmp(bone->branch[0],original+0x50,12) && !memcmp(bone->branch[1],original+0x60,12));
  }
  uint32_t filters[]={0,1,2,4};ff_word((uint8_t *)f.lens,filters[t%4]);
  f.lens[2]=(t%9)*.125f;f.lens[3]=(t%17-8)*.125f;f.lens[4]=3500+t*13.f;f.lens[5]=t%6;
  memcpy(e.ram+obj+0x128,f.lens,24);float color[6];gt4_media_film_color(&f,color);
  CHECK(call(&e,0x334b28,obj,0,0));CHECK(!memcmp(color,original_color,24));
 }
 gt4_film sentinel;memset(&sentinel,0xa5,sizeof sentinel);gt4_film unchanged=sentinel;
 for(size_t n=0;n<end;n++){CHECK(!gt4_media_film_decode(fixture,n,&sentinel));CHECK(!memcmp(&sentinel,&unchanged,sizeof sentinel));}
 fixture[8]=255;CHECK(!gt4_media_film_decode(fixture,end,&sentinel));
 puts("PASS 256 original FILM headers, 256 PCAM writers, 256 CPRM banks/all versions, 256 DRVR skeletons/interpolations, 256 SMOK and PTCL records, 256 shortest wheel angles/suspension limits, 256 color/filter blocks; truncation preserves output, negative changes width.");ee_free(&e);return 0;
}
