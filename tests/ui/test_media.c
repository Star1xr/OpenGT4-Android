/* Synthetic parser/clock regressions; optional local files for disc validation. */
#include "strobe.h"
#include "vol.h"
#include "pss_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void p16(uint8_t *b,unsigned v){b[0]=(uint8_t)v;b[1]=(uint8_t)(v>>8);}
static void p32(uint8_t *b,unsigned v){p16(b,v);p16(b+2,v>>16);}
static void pf(uint8_t *b,float v){memcpy(b,&v,4);}
static size_t pes(uint8_t *b,unsigned track,const uint8_t *data,size_t n)
{
    memset(b,0,n+13);b[2]=1;b[3]=0xbd;b[4]=(uint8_t)((n+7)>>8);b[5]=(uint8_t)(n+7);
    b[6]=0x80;b[9]=0xff;b[10]=0xa1;b[12]=(uint8_t)track;memcpy(b+13,data,n);return n+13;
}
static void audio_test(void)
{
    uint8_t ss[104]={0},b[256]={0};memcpy(ss,"SShd",4);p32(ss+4,24);p32(ss+8,16);
    p32(ss+12,48000);p32(ss+16,2);p32(ss+20,16);memcpy(ss+32,"SSbd",4);p32(ss+36,64);
    /* Two channel groups: positive left, negative right, then predictor tails. */
    ss[40]=ss[56]=12;ss[72]=ss[88]=0x1c;
    memset(ss+42,0x77,14);memset(ss+58,0x99,14);
    size_t n=pes(b,1,ss,30);n+=pes(b+n,2,ss,4);n+=pes(b+n,1,ss+30,74);
    int16_t *pcm;size_t frames;
    CHECK(gt4_pss_audio(b,n,1,&pcm,&frames) && frames==56);
    CHECK(pcm[0]==7 && pcm[1]==-7 && pcm[56]>0 && pcm[57]<0);free(pcm);
    CHECK(!gt4_pss_audio(b,n,3,&pcm,&frames) && !pcm && !frames);
    CHECK(!gt4_pss_audio(b,n-1,1,&pcm,&frames));
    ss[40]=0xfc;size_t bad=pes(b,1,ss,sizeof ss);CHECK(!gt4_pss_audio(b,bad,1,&pcm,&frames));
    ss[40]=12;p32(ss+36,1000000);bad=pes(b,1,ss,sizeof ss);CHECK(!gt4_pss_audio(b,bad,1,&pcm,&frames));
}
static void strobe_test(void)
{
    uint8_t b[812]={0};memcpy(b,"STRB",4);p32(b+8,sizeof b);pf(b+16,4);pf(b+20,4);pf(b+28,30);
    p32(b+0x30,208);p32(b+0x34,1);p32(b+0x38,416);p32(b+0x3c,1);
    p32(b+0x68,560);p32(b+0x6c,1);p32(b+0x98,568);p32(b+0x9c,1);p32(b+0xb0,632);p32(b+0xb4,1);
    p32(b+208,244);p32(b+212,2);p32(b+232,396);p32(b+236,2);p32(b+240,2);
    for(unsigned i=0;i<2;i++) {
        size_t p=244+i*40,a=324+i*36;
        p16(b+p+2,i);p16(b+p+4,i+1);p16(b+p+8,1);p32(b+p+24,(unsigned)a);p32(b+p+28,1);
        pf(b+a+8,1);pf(b+a+12,1);memset(b+a+24,128,4);b[a+27]=(uint8_t)(128-i*64);
        p32(b+396+i*8,412+i*2);p32(b+400+i*8,1);p16(b+412+i*2,i);
    }
    pf(b+424,4);pf(b+428,4);p32(b+432,440);p32(b+436,1);
    p32(b+440,448);p32(b+444,1);p32(b+452,460);p32(b+456,1);
    p32(b+472,480);p32(b+476,4);
    float xy[4][2]={{0,4},{0,0},{4,0},{4,4}};
    for(int i=0;i<4;i++) {
        uint8_t *e=b+480+i*20;
        pf(e+4,(xy[i][0]+xy[(i+3)%4][0])*.5f);pf(e+8,(xy[i][1]+xy[(i+3)%4][1])*.5f);
        pf(e+12,xy[i][0]);pf(e+16,xy[i][1]);
    }
    p16(b+560,4);pf(b+568,.25f);pf(b+588,.25f);p32(b+632,648);p32(b+636,164);
    uint8_t *t=b+648;memcpy(t,"Tex1",4);p16(t+20,1);p16(t+22,1);p32(t+24,48);p32(t+28,88);
    unsigned tex0=(1u<<14)|(2u<<26)|(2u<<30);p32(t+48,tex0);
    p32(t+88,100);t[94]=1;p16(t+96,4);p16(t+98,4);
    for(int i=0;i<16;i++){t[100+i*4]=255;t[103+i*4]=128;}
    gt4_strobe s;CHECK(gt4_strobe_decode(b,sizeof b,&s));
    CHECK(s.nframes==2 && s.nquads==2 && s.nbitmaps==1);
    CHECK(s.quads[0].color[3]==255 && s.quads[1].color[3]==128);
    CHECK(s.quads[0].u1==1 && s.quads[0].v1==1 && s.quads[0].w==4);
    CHECK(gt4_strobe_frame_at(&s,0)==0 && gt4_strobe_frame_at(&s,.04)==1 && gt4_strobe_frame_at(&s,.07)==0);
    CHECK(gt4_strobe_frame_at(&s,NAN)==0);gt4_strobe_free(&s);
    CHECK(!gt4_strobe_decode(b,sizeof b-1,&s));
    p32(b+232,0xffffff00);CHECK(!gt4_strobe_decode(b,sizeof b,&s));p32(b+232,396);
    p16(b+244+10,1);CHECK(!gt4_strobe_decode(b,sizeof b,&s));p16(b+244+10,0);
    pf(b+484,9);CHECK(!gt4_strobe_decode(b,sizeof b,&s));pf(b+484,2);
    uint8_t looped[832];memcpy(looped,b,sizeof b);memset(looped+sizeof b,0,sizeof looped-sizeof b);p32(looped+8,sizeof looped);
    p32(looped+0xa8,812);p32(looped+0xac,1);p32(looped+812,820);p32(looped+816,7);
    const uint8_t commands[]={0x81,2,0,0,0,6,0};memcpy(looped+820,commands,7);
    p32(looped+216,828);p32(looped+220,1);p16(looped+828,1);
    CHECK(gt4_strobe_decode(looped,sizeof looped,&s));CHECK(s.loop_frames==1);
    CHECK(gt4_strobe_frame_at(&s,.04)==0);gt4_strobe_free(&s);
    looped[825]=7;CHECK(!gt4_strobe_decode(looped,sizeof looped,&s));looped[825]=6;
    p16(looped+828,2);CHECK(!gt4_strobe_decode(looped,sizeof looped,&s));p16(looped+828,1);
    p32(looped+812,831);CHECK(!gt4_strobe_decode(looped,sizeof looped,&s));
}
typedef struct {gt4_vol *volume;unsigned count,failed;} dealer_probe;
static void dealer_asset(void *user,const char *name,int directory,uint32_t length)
{
    (void)length;dealer_probe *probe=user;if(directory || !strstr(name,".strb"))return;
    char path[256];snprintf(path,sizeof path,"/piece/tuner_screen/%s",name);size_t size=0;
    uint8_t *bytes=gt4_vol_read(probe->volume,path,&size);gt4_strobe clip;
    if(!gt4_strobe_decode(bytes,size,&clip)){fprintf(stderr,"unsupported dealer: %s\n",name);probe->failed++;}
    else {CHECK(clip.nbitmaps>0 && clip.nframes>1 && clip.loop_frames>0);CHECK(gt4_strobe_frame_at(&clip,(clip.loop_frames+.5)/clip.rate)==0);gt4_strobe_free(&clip);}free(bytes);probe->count++;
}
int main(int argc,char **argv)
{
    audio_test();strobe_test();puts("native media: passed");
    if(argc>2 && !strcmp(argv[1],"dealers")){
        const char *error=NULL;dealer_probe probe={0};probe.volume=gt4_vol_open(argv[2],&error);CHECK(probe.volume);
        CHECK(gt4_vol_list(probe.volume,"/piece/tuner_screen",dealer_asset,&probe));gt4_vol_close(probe.volume);
        printf("Manufacturer slides: %u decoded, %u unsupported\n",probe.count-probe.failed,probe.failed);CHECK(probe.count && !probe.failed);return 0;
    }
    if(argc>2) {
        FILE *f=fopen(argv[2],"rb");CHECK(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
        CHECK(n>0 && n<512l*1024*1024);uint8_t *b=malloc(n);CHECK(b && fread(b,1,n,f)==(size_t)n);fclose(f);
        if(!strcmp(argv[1],"pss")) {
            int16_t *pcm;size_t frames;CHECK(gt4_pss_audio(b,n,1,&pcm,&frames));printf("PCM frames=%zu\n",frames);
            if(argc>3){f=fopen(argv[3],"wb");CHECK(f && fwrite(pcm,4,frames,f)==frames);fclose(f);}free(pcm);
        } else {
            gt4_strobe s;CHECK(gt4_strobe_decode(b,n,&s));printf("STRB frames=%u images=%u quads=%u\n",s.nframes,s.nbitmaps,s.nquads);gt4_strobe_free(&s);
        }
        free(b);
    }
    return 0;
}
