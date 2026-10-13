#include "career_setting.h"
#include <string.h>

typedef struct {uint16_t offset;uint8_t width,paired;} field;
/* Offsets are live-car offsets; serialized bodies omit the class header.
   0043EC30/58 and 0043E3C0/E8: brake balance at +137/+138.
   0043EDC0..0043F068 / 0043E598..0043E918: suspension selector order.
   0043F090..158 / 0043E940..EA08: LSD front triple, then rear triple.
   0043EB60..EBE0 / 0043E2F0..E370: eight ratios, final, auto, saved final.
   0043F248/E B38, EC08/E398, F1F8/F220/EAE8/EB10: NOS, VCD, ballast.
   0043F1A8/F1D0/EA58/EAA0: ASM writes both members of each pair.
   0043F180/EA30 and EC80/ECA8/E410/E438: TCS and downforce. */
static const field fields[GT4_SETTING_GROUPS][18]={
 {{0x137,1},{0x138,1}},
 {{0x14e,1},{0x14f,1},{0x150,1},{0x151,1},{0x152,1},{0x153,1},
  {0x154,1},{0x155,1},{0x141,1},{0x142,1},{0x148,1},{0x149,1},
  {0x144,2},{0x146,2},{0x14a,1},{0x14b,1},{0x156,1},{0x157,1}},
 {{0x158,1},{0x15a,1},{0x15c,1},{0x159,1},{0x15b,1},{0x15d,1}},
 {{0x118,2},{0x11a,2},{0x11c,2},{0x11e,2},{0x120,2},{0x122,2},
  {0x124,2},{0x126,2},{0x130,2},{0x132,1},{0x134,2}},
 {{0x16f,1}},{{0x136,1}},{{0x163,1},{0x164,3}},
 {{0x15f,1,1},{0x161,1,1}},{{0x15e,1}},{{0x139,1},{0x13a,1}}
};
static const uint8_t counts[]={2,18,6,11,1,1,2,2,1,2};
static int address(const uint8_t *body,int group,int index,const field **f){
 uint32_t slot;memcpy(&slot,body+0x488,4);
 if(slot>=3 || group<0 || group>=GT4_SETTING_GROUPS || index<0 || index>=counts[group])return -1;
 *f=&fields[group][index];return (int)(slot*0x178+(*f)->offset-8);
}
int gt4_setting_get(const uint8_t body[0x4b8],int group,int index,int32_t *out){
 const field *f;int at=address(body,group,index,&f);if(at<0)return 0;
 if(f->width==2){uint16_t v;memcpy(&v,body+at,2);*out=v;}
 else if(f->width==3){int8_t v;memcpy(&v,body+at,1);*out=v;}
 else *out=body[at];return 1;
}
int gt4_setting_set(uint8_t body[0x4b8],int group,int index,int32_t value){
 const field *f;int at=address(body,group,index,&f);if(at<0)return 0;
 /* Original setters truncate; authored SliderBar handles the range. */
 if(f->width==2){uint16_t v=(uint16_t)value;memcpy(body+at,&v,2);}
 else {body[at]=(uint8_t)value;if(f->paired)body[at+1]=(uint8_t)value;}return 1;
}
/* VM range constructors: 001490B0 (6), 0014B478 (42), 0014AAD8 (18),
   0014D638 (3), 0014A3D0 (2), 0014D278 (four immediate constants),
   0014CBC8 (12), 0014C898 (3), 0014A6A0 (4).
   The first four suspension values are lhu; all other row loads are lbu. */
static const uint16_t offsets[GT4_SETTING_GROUPS][42]={
 {3,4,5,7,8,9},
 {0x84,0x86,0x8a,0x8c,0x93,0x94,0x96,0x97,0xab,0xac,0xae,0xaf,
  0xb8,0xb9,0xbb,0xbc,0xcc,0xcd,0xce,0xd0,0xd1,0xd3,0xd4,0xd5,
  0xd7,0xd8,0xda,0xdb,0xdc,0xde,0xdf,0xe1,0xe2,0xe3,0xe5,0xe6,
  0xea,0xeb,0xec,0xee,0xef,0xf0},
 {0xf8,0xf9,0xfc,0xfd,0x100,0x101,0x104,0x105,0x108,0x109,0x10c,0x10d,
  0xfa,0xfe,0x102,0x106,0x10a,0x10e},
 {0}, {0x13f,0x140,0x141}, {0x4c,0x4d}, {0},
 {0x11e,0x11f,0x120,0x124,0x125,0x126,0x129,0x12a,0x12b,0x12f,0x130,0x131},
 {0x116,0x117,0x118}, {0x5d,0x5e,0x60,0x61}
};
static const uint8_t range_counts[]={6,42,18,0,3,2,4,12,3,4};
int gt4_setting_range(const uint8_t body[0x4b8],const uint8_t rows[0x142],
 const uint8_t valid[0x142],int group,int32_t out[42]){
 if(group<0 || group>=GT4_SETTING_GROUPS)return -1;
 if(group==GT4_SETTING_BALLAST){out[0]=0;out[1]=200;out[2]=-50;out[3]=50;return 4;}
 unsigned n=range_counts[group];if(!n)return -1;
 for(unsigned i=0;i<n;i++){
  unsigned at=offsets[group][i],width=group==GT4_SETTING_SUSPENSION && i<4?2:1;
  if(!valid[at] || (width==2 && !valid[at+1]))return -1;
  if(width==2){uint16_t v;memcpy(&v,rows+at,2);out[i]=v;}else out[i]=rows[at];
 }
 if(group==GT4_SETTING_DOWNFORCE){
  uint32_t slot;memcpy(&slot,body+0x488,4);if(slot>=3)return -1;
  uint64_t wing;memcpy(&wing,body+slot*0x178+0x100,8);
  /* 0014A738..878: wing contributions are added to BOTH endpoints. */
  if(wing!=UINT64_MAX){if(!valid[0x137] || !valid[0x138])return -1;
   for(unsigned i=0;i<4;i++)out[i]+=rows[0x137+(i&1)];}
 }return (int)n;
}

/* 00149538 +0x228..0x298 -> 003553A0 -> 00355278. Integer bounds
   interpolate adjacent ratios with signed division toward zero. */
void gt4_setting_ratio_bounds(int n,const int16_t *ratios,int16_t *lo,int16_t *hi){
 if(n<1 || n>9)return;
 hi[1]=(int16_t)(ratios[1]*9/8);
 for(int i=1;i<n;i++){lo[i]=(int16_t)((ratios[i]*2+ratios[i+1])/3);hi[i+1]=(int16_t)((ratios[i]+ratios[i+1]*2)/3);}
 lo[n]=(int16_t)(ratios[n]*7/8);lo[0]=(int16_t)(ratios[0]*7/8);hi[0]=(int16_t)(ratios[0]*9/8);
}
