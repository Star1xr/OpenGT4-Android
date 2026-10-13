#include "ranking_code.h"
#include <stdlib.h>
#include <string.h>
#include "miniz.h"
#include "inflate.h"
/* 0044B3A8: exponent/log tables of 45 in GF(257). */
static void tables(uint8_t exp[256],uint8_t log[256])
{unsigned x=1;for(unsigned i=0;i<256;i++){exp[i]=(uint8_t)x;log[(uint8_t)x]=(uint8_t)i;x=x*45%257;}}
static uint8_t rol(uint8_t x,int n){return (uint8_t)((x<<n)|(x>>(8-n)));}
/* 0044B680 +78..238: the two rotated key checksums and round biases. */
static void schedule(uint8_t out[217],const uint8_t left[8],const uint8_t right[8],unsigned rounds,const uint8_t exp[256])
{
 uint8_t a[9]={0},b[9]={0};out[0]=(uint8_t)rounds;
 for(int i=0;i<8;i++){a[i]=rol(left[i],5);b[i]=right[i];a[8]^=a[i];b[8]^=b[i];out[1+i]=b[i];}
 for(unsigned r=1;r<=rounds;r++){
  for(int i=0;i<9;i++){a[i]=rol(a[i],6);b[i]=rol(b[i],6);}
  for(unsigned i=0;i<8;i++){
   out[(r-1)*16+9+i]=(uint8_t)(a[(r*2-1+i)%9]+exp[exp[(uint8_t)(18*r+1+i)]]);
   out[(r-1)*16+17+i]=(uint8_t)(b[(r*2+i)%9]+exp[exp[(uint8_t)(18*r+10+i)]]);
  }
 }
}
/* 0044B900 +78..228: byte arithmetic and the authored permutation. */
static void block(uint8_t out[8],const uint8_t in[8],const uint8_t keys[217],const uint8_t exp[256],const uint8_t log[256])
{
 unsigned a=in[0],b=in[1],c=in[2],d=in[3],e=in[4],f=in[5],g=in[6],h=in[7];
 const uint8_t *k=keys+1;
 for(unsigned r=0;r<keys[0];r++,k+=16){
  a=(exp[a^k[0]]+k[8])&255;b=log[(b+k[1])&255]^k[9];c=log[(c+k[2])&255]^k[10];d=(exp[d^k[3]]+k[11])&255;
  e=(exp[e^k[4]]+k[12])&255;f=log[(f+k[5])&255]^k[13];g=log[(g+k[6])&255]^k[14];h=(exp[h^k[7]]+k[15])&255;
  b=(b+a)&255;a=(a+b)&255;d=(d+c)&255;c=(c+d)&255;f=(f+e)&255;e=(e+f)&255;h=(h+g)&255;g=(g+h)&255;
  c=(c+a)&255;a=(a+c)&255;g=(g+e)&255;e=(e+g)&255;d=(d+b)&255;b=(b+d)&255;h=(h+f)&255;f=(f+h)&255;
  e=(e+a)&255;a=(a+e)&255;f=(f+b)&255;b=(b+f)&255;g=(g+c)&255;c=(c+g)&255;h=(h+d)&255;d=(d+h)&255;
  unsigned bb=b,cc=c,dd=d,ee=e,ff=f,gg=g;b=ee;c=bb;d=ff;e=cc;f=gg;g=dd;
 }
 out[0]=(uint8_t)(a^k[0]);out[1]=(uint8_t)(b+k[1]);out[2]=(uint8_t)(c+k[2]);out[3]=(uint8_t)(d^k[3]);
 out[4]=(uint8_t)(e^k[4]);out[5]=(uint8_t)(f+k[5]);out[6]=(uint8_t)(g+k[6]);out[7]=(uint8_t)(h^k[7]);
}
/* 0044ACD8: two dependent 10-round blocks, both halves fed back. */
static void digest_block(uint8_t state[16],const uint8_t input[8],const uint8_t exp[256],const uint8_t log[256])
{
 uint8_t k1[217],k2[217],first[8],second[8];
 schedule(k1,state+8,input,10,exp);block(first,state,k1,exp,log);
 schedule(k2,input,state,10,exp);
 for(int i=0;i<8;i++){state[i]^=first[i];second[i]=(uint8_t)~state[8+i];}
 block(second,second,k2,exp,log);for(int i=0;i<8;i++)state[8+i]^=second[i];
}
static void bits(uint8_t *p,unsigned offset,uint32_t value,unsigned n)
{for(unsigned i=0;i<n;i++){unsigned bit=offset+i;p[bit/8]=(uint8_t)((p[bit/8]&~(1u<<(bit%8)))|(((value>>i)&1u)<<(bit%8)));}}
int gt4_ranking_code_key(gt4_vol *vol,uint8_t key[16])
{
 size_t n;uint8_t *raw=gt4_disc_read_root(gt4_vol_source(vol),"CORE.GT4",&n),*p=NULL;int ok=0;
 if(!raw || n<6 || raw[0]!=1 || raw[1]!=1)goto done;
 uint32_t size=raw[2]|raw[3]<<8|raw[4]<<16|(uint32_t)raw[5]<<24;
 if(size<0x12c || size>32u*1024*1024 || !(p=malloc(size)) || core_inflate_raw(raw+6,n-6,p,size)!=size)goto done;
 for(size_t at=0x12c;at+8<=size;){
  uint32_t base=p[at]|p[at+1]<<8|p[at+2]<<16|(uint32_t)p[at+3]<<24;
  uint32_t length=p[at+4]|p[at+5]<<8|p[at+6]<<16|(uint32_t)p[at+7]<<24;at+=8;if(length>size-at)goto done;
  if(base<=0x6a7c88 && (uint64_t)base+length>=0x6a7c98){memcpy(key,p+at+0x6a7c88-base,16);ok=1;break;}at+=length;
 }
done:free(raw);free(p);return ok;
}
void gt4_ranking_code(const uint32_t fields[10],const uint8_t key[16],uint32_t clock,char out[33])
{
 static const unsigned widths[10]={5,11,9,1,12,12,3,22,4,4};
 uint8_t packed[13]={0},exp[256],log[256],state[16]={0},message[16]={0},keys[217];tables(exp,log);
 unsigned offset=0;for(int i=0;i<10;i++){bits(packed,offset,fields[i],widths[i]);offset+=widths[i];}
 /* 0044AF40 +28..80: hash the low clock word + AEF0A4C0; its
    optional string argument is NULL here, so only four bytes and zero
    padding enter 0044AEF0. Console/host clock selection is P. */
 uint32_t stamp=clock+0xaef0a4c0u;for(int i=0;i<4;i++)message[i]=(uint8_t)(stamp>>(i*8));
 digest_block(state,message,exp,log);
 uint32_t sum=0;for(int i=0;i<4;i++)for(int j=0;j<4;j++)sum^=(uint32_t)state[j*4+i]<<(8*i);
 sum^=sum>>16;sum^=sum>>8;sum^=sum>>4;
 uint8_t encoded[13]={0};memcpy(encoded+2,packed,11);bits(encoded,0,sum,4);
 uint32_t crc=(uint32_t)mz_crc32(0,encoded,13);bits(encoded,4,crc^(crc>>12)^(crc>>24),12);
 /* 0044B060 +E0..114, 0044BED8: overlapping eight-byte blocks. */
 schedule(keys,key,key+8,13,exp);for(int i=0;i<=5;i++)block(encoded+i,encoded+i,keys,exp,log);
 /* 0044C918/0044C7F8: little-endian integer to fixed 32 decimal digits. */
 for(int i=31;i>=0;i--){unsigned rem=0;for(int j=12;j>=0;j--){unsigned v=(rem<<8)|encoded[j];encoded[j]=(uint8_t)(v/10);rem=v%10;}out[i]=(char)('0'+rem);}out[32]=0;
}
