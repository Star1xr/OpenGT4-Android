#include "showroom_math.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void)
{
    /* ABE=0 paint writes RGB even when its alpha carries low reflectivity. */
    CHECK(showroom_blend_channel(200,20,.25f,1,0x44,0,0)==200);
    CHECK(showroom_blend_channel(200,20,.25f,1,0x44,0,1)==65);
    /* Reflection adds Cs*Ad to the paint, without interpreting Ad as opacity. */
    CHECK(showroom_blend_channel(80,100,1,.25f,0x58,0,1)==120);
    CHECK(showroom_blend_channel(240,100,1,1,0x58,0,1)==255);
    CHECK(showroom_alpha_byte(1.f-1e-7f)==128);
    CHECK(showroom_alpha_byte(1.f+1e-7f)==128);
    CHECK(showroom_alpha_byte(127.f/128)==127);
    CHECK(showroom_alpha_byte(1.5f)==192);
    CHECK(showroom_alpha_byte(10)==255);
    /* Lamp UVs can extend beyond an edge whose last texel must be held.
       Wrapping there repeats the red lens onto the surrounding bodywork. */
    CHECK(showroom_texel(-1,64,0,0,63)==63);
    CHECK(showroom_texel(64,64,0,0,63)==0);
    CHECK(showroom_texel(-1,64,1,0,0)==0);
    CHECK(showroom_texel(64,64,1,0,0)==63);
    CHECK(showroom_texel(-8,64,2,4,59)==4);
    CHECK(showroom_texel(64,64,2,4,59)==59);
    CHECK(showroom_texel(20,64,2,4,59)==20);
    CHECK(showroom_texel(63,64,2,0,63)==63 && showroom_texel(64,64,2,0,63)==63);
    CHECK(showroom_texel(18,64,3,15,32)==34);
    CHECK(showroom_texel(-1,64,3,15,32)==47);
    /* Factory Focus ride height lifts the body, not the wheel/ground plane. */
    float body[3]={.7f,0,1.3f};showroom_body_pose(body,0,1,.155f);
    CHECK(fabsf(body[1]-.155f)<1e-6f && body[2]==1.3f);
    float sn=.02f,cs=sqrtf(1-sn*sn),front[3]={0,0,-1},rear[3]={0,0,1};
    showroom_body_pose(front,sn,cs,.12f);showroom_body_pose(rear,sn,cs,.12f);
    CHECK(fabsf(front[1]-.1f)<1e-6f && fabsf(rear[1]-.14f)<1e-6f);
    /* Focus brake face: opposite native sides must sit inside the wheel
       barrel, not behind the body hull (a reversed offset put x at -.1815).
       Representative coordinates within fdeu0007 bounds; width from its
       factory TIRESIZE row. */
    for(unsigned axle=0;axle<2;axle++) {
        float left[3]={.074f,.1f,-.12f},right[3]={-.074f,.1f,-.12f};
        showroom_wheel_attachment(left,axle*2,.215f);
        showroom_wheel_attachment(right,axle*2+1,.215f);
        CHECK(left[0]>-.1075f && left[0]<0 && right[0]>0 && right[0]<.1075f);
        CHECK(fabsf(left[0]+right[0])<1e-6f && fabsf(right[0]-.0335f)<1e-6f);
        CHECK(left[1]==.1f && left[2]==-.12f && right[1]==.1f && right[2]==-.12f);
    }
    /* The native diameter encoding has three cases, not a universal /10.
       Vintage 26/41-inch wheels must remain full-size, including thin tires. */
    float width,rim,radius;uint8_t tire[4]={16,0,26,10};
    showroom_tire_size(tire,&width,&rim,&radius);
    CHECK(fabsf(width-.05f)<1e-6f && fabsf(rim-.3302f)<1e-6f && fabsf(radius-.3702f)<1e-6f);
    tire[2]=41;showroom_tire_size(tire,&width,&rim,&radius);
    CHECK(fabsf(rim-.5207f)<1e-6f && fabsf(radius-.5607f)<1e-6f);
    tire[0]=9;tire[2]=170;tire[3]=41;showroom_tire_size(tire,&width,&rim,&radius);
    CHECK(fabsf(rim-.2159f)<1e-6f && fabsf(radius-.30815f)<1e-6f);
    tire[2]=175;showroom_tire_size(tire,&width,&rim,&radius);CHECK(fabsf(rim-.22225f)<1e-6f);
    tire[0]=0;tire[2]=60;tire[3]=0;showroom_tire_size(tire,&width,&rim,&radius);
    CHECK(fabsf(rim-.762f)<1e-6f && fabsf(width-.01f)<1e-6f && radius==rim);
    tire[2]=61;showroom_tire_size(tire,&width,&rim,&radius);CHECK(fabsf(rim-.07747f)<1e-6f);
    puts("showroom: materials, wheel attachment and native tire sizing passed");return 0;
}
