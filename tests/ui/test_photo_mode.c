/* CRPM values compared independently to the user's original destination
   bytes. Does not certify camera matrices or the final rendered image. */
#include "photo_mode.h"
#include "vol.h"
#include "sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"photo data line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t word(const uint8_t *p){return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;}
static float number(const uint8_t *p){uint32_t b=word(p);float n;memcpy(&n,&b,4);return n;}
int main(int argc,char **argv)
{
    CHECK(argc==2 || argc==3);int negative=argc==3;const char *err;
    gt4_vol *vol=gt4_vol_open(argv[1],&err);CHECK(vol);
    const char *names[]={"brooklyn","forum","fremont","gion","grandcanyon","louisburg","nanzenji","rialt","sagano","sanmarco","shibuya","snow","times","tsukiji","tsumago"};
    for(int i=0;i<15;i++) {
        char path[96];snprintf(path,sizeof path,"/crs/ss_%s",names[i]);size_t n;uint8_t *b=gt4_vol_read(vol,path,&n);CHECK(b);
        gt4_photo_location p;CHECK(gt4_photo_location_read(b,n,&p));
        const uint8_t *d=b+word(b+0xcc);CHECK(p.data==d);
        CHECK(p.regions==word(d+0x18) && p.region_table==word(d+0x1c) && p.cameras==word(d+0x20) && p.camera_table==word(d+0x24));
        if(negative && !i)p.car[0]+=1;
        CHECK(p.camera[0]==number(d+0x38) && p.camera[2]==number(d+0x3c) && p.car[0]==number(d+0x40) && p.car[2]==number(d+0x44) && p.car_angle==number(d+0x48));
        for(int k=0;k<4;k++)CHECK(p.bounds[k]==number(d+0x28+k*4));
        float width=number(d+0x54)-number(d+0x4c),height=number(d+0x58)-number(d+0x50);
        if(width<=1)width=500;if(height<=1)height=500;
        CHECK(p.map_width==width && p.map_height==height && p.map_length==(width<height?width:height));
        gt4_course course;gt4_photo_mode state;CHECK(gt4_course_open(&course,b,n) && gt4_photo_mode_init(&state,&course));
        CHECK(state.ready && isfinite(state.eye[1]) && isfinite(state.yaw) && state.view_angle==70);
        uint8_t *mutable=(uint8_t *)d;uint8_t old[4];memcpy(old,mutable+0x1c,4);memset(mutable+0x1c,0xff,4);
        gt4_photo_location unchanged=p;CHECK(!gt4_photo_location_read(b,n,&unchanged) && !memcmp(&p,&unchanged,sizeof p));memcpy(mutable+0x1c,old,4);
        uint32_t offset=word(b+0xcc);for(size_t k=0;k<offset+0xc0;k+=17)CHECK(!gt4_photo_location_read(b,k,&unchanged));
        printf("%s: regions=%u cameras=%u original poses/bounds/map dimensions exact\n",path,p.regions,p.cameras);free(b);
    }
    float apertures[17];CHECK(gt4_photo_apertures_load(argv[1],apertures));
    for(int i=1;i<=16;i++)CHECK(isfinite(apertures[i]) && apertures[i]>0);
    /* Each selected shutter bank must be playable by the existing decoder. */
    for(char c='a';c<='s';c++) {
        char path[80];snprintf(path,sizeof path,"/sound/gt4shuttertype%c.ins",c);size_t n;uint8_t *b=gt4_vol_read(vol,path,&n);CHECK(b);
        int16_t *pcm;size_t frames;CHECK(gt4_sound_render(b,n,0,&pcm,&frames) && frames>0);free(pcm);free(b);
    }
    gt4_vol_close(vol);puts("PASS 15 original CRPM headers and corruption/truncation rejection, 19 shutter banks; negative moves one original car pose.");return 0;
}
