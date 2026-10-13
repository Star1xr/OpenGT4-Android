/* Authored native driver asset metadata. Evidence: NTSC-U v2.00 functions
   003B5438, 003B0C80, 003B0D18, 00459178. Assets stay on the user's disc. */
#include "driver_model.h"
#include <math.h>
#include <string.h>
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned u32(const uint8_t *p){return u16(p)|u16(p+2)<<16;}
static float f32(const uint8_t *p){float f;memcpy(&f,p,4);return f;}
static int span(size_t n,size_t p,size_t len){return p<=n && len<=n-p;}
static int vector(const uint8_t *d,size_t p,float *out,int n)
{for(int i=0;i<n;i++){out[i]=f32(d+p+i*4);if(!isfinite(out[i]))return 0;}return 1;}

unsigned gt4_car_exhaust_count(const uint8_t *d,size_t n)
{
 /* 00390898 -> 00450FB0: CAR4 +10 selects CARINFO, +38 count.
    003B6B78 suppresses a pulse on a car without authored outlets. */
 if(!d || n<0x14 || memcmp(d,"CAR4",4))return 0;
 size_t info=u32(d+0x10);if(!info || !span(n,info,0x40))return 0;
 unsigned count=u32(d+info+0x38),points=u32(d+info+0x3c);
 return span(n,points,(size_t)count*24)?count:0;
}
unsigned gt4_car_exhaust_points(const uint8_t *d,size_t n,float out[][6],unsigned max)
{
 /* 00452AC8 reads 00450FB0's +3C: 0x18 bytes an outlet, the position and
    the direction the flame leaves along, in the model's frame */
 unsigned count=gt4_car_exhaust_count(d,n),k;
 if(!count)return 0;
 size_t points=u32(d+u32(d+0x10)+0x3c);
 for(k=0;k<count && k<max;k++)if(!vector(d,points+k*24,out[k],6))return 0;
 return k;
}
int gt4_driver_car_read(const uint8_t *d,size_t n,gt4_driver_car *out)
{
    if(!d || !out || n<0x40 || memcmp(d,"CAR4",4))return 0;
    size_t info=u32(d+16);if(!info || !span(n,info,0x54))return 0;
    gt4_driver_car c={0};c.flags=(uint16_t)u16(d+info);c.type=d[info+2];c.crew=d[info+3];
    c.builtin_offset=u32(d+52);c.geometry_offset=u32(d+info+0x50);
    if(c.builtin_offset) {
        size_t b=c.builtin_offset;
        if(!span(n,b,128) || memcmp(d+b,"MDLS",4) || u32(d+b+16)<128 || !span(n,b,u32(d+b+16)))return 0;
    }
    if(c.geometry_offset) {
        size_t g=c.geometry_offset;if(!span(n,g,0x7c))return 0;
        if(!vector(d,g,c.wheel,3) || !vector(d,g+12,c.wheel_normal,3) ||
           !vector(d,g+0x18,&c.wheel_radius,1) || !vector(d,g+0x1c,c.feet[0],3) ||
           !vector(d,g+0x28,c.feet[1],3) || !vector(d,g+0x34,c.seat,3) ||
           !vector(d,g+0x40,&c.chair_angle,1) || !vector(d,g+0x4c,c.shift,3) ||
           !vector(d,g+0x58,c.shift_normal,3) || !vector(d,g+0x64,&c.shift_length,1) ||
           !vector(d,g+0x6c,c.handbrake,3) || !vector(d,g+0x78,&c.handbrake_length,1))return 0;
        c.shift_type=(int16_t)u16(d+g+0x6a);
        /* 003CE288 +0x98..AC uses the normal at +B8 to choose the side;
           003CE4A0 +0x114/1F4 uses the position at +AC. +B0 is height. */
        if(span(n,g+0xac,24) && vector(d,g+0xac,c.fuel_port,3) &&
           vector(d,g+0xb8,c.fuel_normal,3))c.fuel_geometry=1;
    }
    *out=c;return 1;
}
gt4_driver_selection gt4_driver_select(const gt4_driver_car *c,unsigned entry,int variant)
{
    gt4_driver_selection s={0};if(!c)return s;
    s.crew_enabled=c->crew!=0 && entry<3;
    if(c->builtin_offset){s.source=GT4_DRIVER_BUILTIN;return s;}
    unsigned choice=variant>=0?(unsigned)variant%3:entry%3;
    static const char *normal[]={"/character/driver2.mdl","/character/driver.mdl","/character/driver3.mdl"};
    switch(c->type) {
    case 1:s.path=normal[choice];break;
    case 2:s.path="/character/driver4.mdl";break;
    case 5:case 6:s.path="/character/classic1.mdl";break;
    case 7:s.path="/character/classic2.mdl";break;
    case 10:s.path="/character/driver_nike.mdl";break;
    case 14:s.path="/character/driver.mdl";break;
    case 15:s.path=s.crew_enabled?"/character/driver_mtr.mdl":"/character/driver_nike.mdl";break;
    default:s.crew_enabled=0;return s;
    }
    s.source=GT4_DRIVER_SHARED;return s;
}
int gt4_driver_detail(unsigned type)
{return type==0?0:type==32 || type==33?1:type==16 || type==17?2:3;}

static const char *const names[GT4_DRIVER_BONES]={
    "PELVIS","LTHIGH_BONE","LSHANK_BONE","LFOOT_BONE","RTHIGH_BONE","RSHANK_BONE","RFOOT_BONE",
    "LOWER_SPINE","MIDDLE_SPINE","SCAPULA","LUPARM_BONE","LFOREARM_BONE","RUPARM_BONE","RFOREARM_BONE",
    "CRANIUM","LCLAVICLE","RCLAVICLE","LHAND_BONE","RHAND_BONE","NECK_BONE"};
static const char *name_at(const uint8_t *d,size_t n,size_t p)
{return p<n && memchr(d+p,0,n-p)?(const char *)d+p:NULL;}
static int symbols(const uint8_t *d,size_t n,const char *name,int *first,int *count)
{
    size_t p=u32(d+88);unsigned num=u16(d+42),models=u16(d+22);
    if(!span(n,p,(size_t)num*8))return 0;
    *first=-1;*count=0;
    for(unsigned i=0;i<num;i++) {
        const uint8_t *s=d+p+i*8;const char *label=name_at(d,n,u32(s));if(!label || u16(s+4)>=models)return 0;
        if(!strcmp(label,name)) {
            if(*first<0)*first=(int)u16(s+4);
            else if(u16(s+4)!=(unsigned)(*first+*count))return 0;
            (*count)++;
        }
    }
    return 1;
}
int gt4_driver_rig_read(const uint8_t *d,size_t n,gt4_driver_rig *out)
{
    if(!d || !out || n<128 || memcmp(d,"MDLS",4))return 0;
    size_t size=u32(d+16);if(size<128 || size>n)return 0;n=size;
    unsigned binds=u16(d+46);size_t p=u32(d+72);
    /* Reduced builtin pilots use rigid bone symbols and authored dimensions,
       without a skin palette (003B0D18 / 003B11A8). */
    if(binds>GT4_DRIVER_BONES || !span(n,p,(size_t)binds*80))return 0;
    gt4_driver_rig rig={0};unsigned slots=0;
    for(int b=0;b<GT4_DRIVER_BONES;b++) {
        gt4_driver_bone *bone=&rig.bone[b];bone->model_first=-1;
        if(!symbols(d,n,names[b],&bone->model_first,&bone->model_count))return 0;
        for(unsigned j=0;j<binds;j++) {
            size_t at=p+j*80;const char *label=name_at(d,n,u32(d+at+68));if(!label)return 0;
            if(strcmp(label,names[b]))continue;
            unsigned slot=u32(d+at+64);if(slot>=GT4_DRIVER_BONES || bone->present || (slots&(1u<<slot)))return 0;
            bone->present=1;bone->slot=(uint8_t)slot;slots|=1u<<slot;
            if(!vector(d,at,bone->inverse_bind,16))return 0;
        }
    }
    /* Every bind slot needs a known skeleton bone and dense shader indexing. */
    if(slots!=((1u<<binds)-1))return 0;rig.slots=binds;
    if(!symbols(d,n,"SKINNING",&rig.skin_first,&rig.skin_count) ||
       !symbols(d,n,"CRANIUM2",&rig.head2_first,&rig.head2_count))return 0;
    if(!binds && rig.skin_count)return 0;
    int rigid=0;for(int b=0;b<GT4_DRIVER_BONES;b++)rigid+=rig.bone[b].model_count;
    if(!binds && !rigid)return 0;
    int dimensions=0;p=u32(d+104);
    for(unsigned budget=0;budget<128;budget++) {
        if(!span(n,p,4))return 0;unsigned h=u32(d+p),id=h>>8,count=h&255;p+=4;
        if(!id){if(!dimensions)return 0;*out=rig;return 1;}
        if(!span(n,p,(size_t)count*4))return 0;
        if(id==0x800000) {if(count!=16 || dimensions || !vector(d,p,rig.dimensions,16))return 0;dimensions=1;}
        else if(id==0x800001) {if(count!=2 || !vector(d,p,rig.lod,2))return 0;}
        p+=(size_t)count*4;
    }
    return 0;
}

int gt4_driver_palette(const gt4_driver_rig *rig,const float pose[GT4_DRIVER_BONES][16],
                       float out[GT4_DRIVER_BONES][16])
{
    if(!rig || !pose || !out || !rig->slots || rig->slots>GT4_DRIVER_BONES)return 0;
    float palette[GT4_DRIVER_BONES][16];unsigned slots=0;
    for(int b=0;b<GT4_DRIVER_BONES;b++)for(int k=0;k<16;k++)palette[b][k]=k%5==0?1.f:0.f;
    for(int b=0;b<GT4_DRIVER_BONES;b++)if(rig->bone[b].present) {
        const gt4_driver_bone *bone=&rig->bone[b];unsigned slot=bone->slot;
        if(slot>=rig->slots || (slots&(1u<<slot)))return 0;slots|=1u<<slot;
        for(int k=0;k<16;k++)if(!isfinite(pose[b][k]) || !isfinite(bone->inverse_bind[k]))return 0;
        for(int c=0;c<4;c++)for(int row=0;row<4;row++) {
            float sum=0;
            for(int k=0;k<4;k++)sum+=pose[b][k*4+row]*bone->inverse_bind[c*4+k];
            if(!isfinite(sum))return 0;palette[slot][c*4+row]=sum;
        }
    }
    if(slots!=((1u<<rig->slots)-1))return 0;
    memcpy(out,palette,sizeof palette);return 1;
}
