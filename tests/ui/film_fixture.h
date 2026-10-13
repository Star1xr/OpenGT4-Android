/* Authored scene data, serialized according to 00334FC8/0033A2A0/
   0033ADE8/00459E30. Contains no extracted game bytes. */
#ifndef GT4_TEST_FILM_FIXTURE_H
#define GT4_TEST_FILM_FIXTURE_H
#include <stdint.h>
#include <string.h>
static void ff_word(uint8_t *p,unsigned v){for(int k=0;k<4;k++)p[k]=(uint8_t)(v>>(k*8));}
static void ff_float(uint8_t *p,float f){uint32_t v;memcpy(&v,&f,4);ff_word(p,v);}
static size_t ff_begin(uint8_t *p,size_t *at,const char *tag){size_t n=*at;memcpy(p+n,tag,4);*at+=8;return n;}
static void ff_end(uint8_t *p,size_t at,size_t start){ff_word(p+start+4,(unsigned)(at-start));}
static void ff_string(uint8_t *p,size_t *at,const char *s){size_t n=strlen(s)+1;memcpy(p+*at,s,n);*at+=n;}
static size_t film_fixture(uint8_t *p,size_t cap){
 if(cap<2048)return 0;memset(p,0,cap);size_t at=0,b=ff_begin(p,&at,"PMDI");at++;ff_end(p,at,b);
 b=ff_begin(p,&at,"FILM");p[at++]=2;
 p[at++]=128;p[at++]=2;p[at++]=224;p[at++]=1;p[at++]=2;p[at++]=2;
 ff_float(p+at,4.f/3);at+=4;p[at++]=0;p[at++]=32;p[at++]=0;
 ff_float(p+at,1);at+=4;at+=8;ff_float(p+at,1);at+=4;at+=4;
 ff_float(p+at,5500);at+=4;at+=4+32;ff_end(p,at,b);
 b=ff_begin(p,&at,"RHDR");p[at++]=0;ff_string(p,&at,"tsukuba");ff_string(p,&at,"");p[at++]=1;
 ff_string(p,&at,"ka_01");ff_string(p,&at,"ka_01");p[at++]=2;ff_word(p+at,2);at+=4;ff_end(p,at,b);
 for(int frame=0;frame<2;frame++){
  b=ff_begin(p,&at,"SFRM");size_t camera=ff_begin(p,&at,"PCAM");p[at++]=1;
  float values[17]={0,2,8,0,0,0,0,1,0,45,4.f/3,.2f,20000,8,1.f/60,.035f,0};
  for(int k=0;k<17;k++){ff_float(p+at,values[k]);at+=4;}ff_end(p,at,camera);
  size_t car=ff_begin(p,&at,"CARD"),params=ff_begin(p,&at,"CPRM");p[at++]=3;
  ff_float(p+at+12,(float)frame);for(int wheel=0;wheel<4;wheel++){ff_float(p+at+85+wheel*52+24,.175f);ff_float(p+at+85+wheel*52+28,.30f);ff_float(p+at+85+wheel*52+32,.17f);}at+=361;ff_end(p,at,params);ff_end(p,at,car);ff_end(p,at,b);
 }return at;
}
#endif
