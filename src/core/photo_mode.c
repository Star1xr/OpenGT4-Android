#include "photo_mode.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <fenv.h>
#include "vol.h"
#include "inflate.h"

static uint32_t u32(const uint8_t *p){uint32_t n;memcpy(&n,p,4);return n;}
static float f32(const uint8_t *p){float n;memcpy(&n,p,4);return n;}
static int table(size_t n,uint32_t at,uint32_t count,size_t stride)
{return at<=n && count<=(n-at)/stride;}

int gt4_photo_location_read(const uint8_t *course,size_t size,gt4_photo_location *out)
{
    if(!course || !out || size<0xd0)return 0;
    /* 0039674C..00396764: course +CC is DPhotoMode. 003C65E8 +50..D4:
       +18/+1C are count/offset of 64-byte regions; +20/+24 the 16-byte
       camera records. 0036FAE8 +190..248 reads the initial poses/bounds. */
    uint32_t at=u32(course+0xcc);
    if(!at || at>size || size-at<0xc0 || memcmp(course+at,"CRPM",4))return 0;
    gt4_photo_location p={0};p.data=course+at;p.size=size-at;
    p.regions=u32(p.data+0x18);p.region_table=u32(p.data+0x1c);
    p.cameras=u32(p.data+0x20);p.camera_table=u32(p.data+0x24);
    if(!p.regions || !table(p.size,p.region_table,p.regions,64) || !table(p.size,p.camera_table,p.cameras,16))return 0;
    for(int i=0;i<4;i++){p.bounds[i]=f32(p.data+0x28+4*i);if(!isfinite(p.bounds[i]))return 0;}
    p.camera[0]=f32(p.data+0x38);p.camera[2]=f32(p.data+0x3c);
    p.car[0]=f32(p.data+0x40);p.car[2]=f32(p.data+0x44);p.car_angle=f32(p.data+0x48);
    /* 0036FAE8 +1D4..218: map dimensions fall back to 500 only when
       <=1; initial base length is the smaller dimension. */
    p.map_width=f32(p.data+0x54)-f32(p.data+0x4c);
    p.map_height=f32(p.data+0x58)-f32(p.data+0x50);
    if(p.map_width<=1)p.map_width=500;
    if(p.map_height<=1)p.map_height=500;
    p.map_length=fminf(p.map_width,p.map_height);
    for(int i=0;i<3;i++)if(!isfinite(p.camera[i]) || !isfinite(p.car[i]))return 0;
    if(!isfinite(p.car_angle) || !isfinite(p.map_width) || !isfinite(p.map_height) || !isfinite(p.map_length))return 0;
    *out=p;return 1;
}

int gt4_photo_mode_init(gt4_photo_mode *p,const gt4_course *course)
{
    if(!p || !course)return 0;
    gt4_photo_mode next={0};
    if(!gt4_photo_location_read(course->data,course->size,&next.location))return 0;
    memcpy(next.eye,next.location.camera,sizeof next.eye);
    memcpy(next.car,next.location.car,sizeof next.car);next.car_angle=next.location.car_angle;
    /* 0036FAE8 +190 / 00371220 +7C..8C: trace down from y=100.
       Ground adapter uses the same installed RNW4, without a vehicle solver.
       A: boundary polygons/obstruction tests of 003FECB0 are not ported yet. */
    /* 003FE358 +2C/+E8..12C: ground query returns zero when no
       surface is hit (e.g. Grand Canyon camera outside RNW4). */
    float ground=0;
    gt4_course_height(course,next.eye[0],next.eye[2],100,&ground);
    gt4_course_height(course,next.car[0],next.car[2],100,&next.car[1]);
    const uint8_t *r=next.location.data+next.location.region_table;
    /* 00370DD0 +68..C4: standing height is clamped to region +24/+28. */
    float low=fmaxf(.1f,f32(r+0x24)),high=f32(r+0x28);
    /* 0036FAE8 +100..128: camera height above the surface starts 1.3. */
    /* 00370DD0 +90..C4: high is absolute, low is above ground. */
    next.eye[1]=fmaxf(ground+low,fminf(high,ground+1.3f));
    /* 00371220 +D0..FC: first region's +18/+1C is the camera's aim in
       the map plane, normalized angle = 90 + atan2(dz,dx) in degrees.
       Native float trig is a platform adapter, pending EE comparison (A). */
    next.yaw=90+atan2f(f32(r+0x1c)-next.eye[2],f32(r+0x18)-next.eye[0])*57.29577951308232f;
    /* MPhotoMapWindow ctor 00192BE0 +94/+B4/+BC: 70, 7, 10. */
    next.view_angle=70;next.aperture=7;next.lightness=10;
    next.image_scale=1;
    strcpy(next.operate_mode,"none");strcpy(next.color_filter,"none");
    next.ready=1;*p=next;return 1;
}

float gt4_photo_aperture(const uint8_t *core,size_t size,int index)
{
    /* 0036F5C0 +0..3C: clamp 1..16, table 00620E6C, truncate f*100.
       00370D70 divides the resulting integer by 100. CORE begins 100000. */
    size_t at=0x520e6c;if(!core || size<at+17*4)return 0;
    if(index<1)index=1;if(index>16)index=16;
    float value=f32(core+at+index*4);
    return (float)(int)(value*100)/100;
}

int gt4_photo_tables_load(const char *disc,float out[17],int32_t shutters[19])
{
    size_t size=0;uint8_t *raw=gt4_disc_read_root(disc,"CORE.GT4",&size),*decoded=NULL;int ok=0;
    if(!raw || size<6 || raw[0]!=1 || raw[1]!=1)goto done;
    uint32_t n=u32(raw+2);if(n<0x12c || n>32u*1024*1024)goto done;
    decoded=malloc(n);
    if(!decoded || core_inflate_raw(raw+6,size-6,decoded,n)!=n || u32(decoded+0x100)!=0x176265b3)goto done;
    for(size_t at=0x12c;at<n;) {
        if(n-at<8)goto done;
        uint32_t base=u32(decoded+at),length=u32(decoded+at+4);at+=8;
        if(length>n-at)goto done;
        /* 0036F5C0: original aperture table 00620E6C, user's disc. */
        if(base<=0x620e6c && (uint64_t)base+length>=0x620ef0+19*4) {
            const uint8_t *p=decoded+at+0x620e6c-base;
            for(int i=0;i<17;i++)out[i]=(float)(int)(f32(p+i*4)*100)/100;
            if(shutters)for(int i=0;i<19;i++)shutters[i]=(int32_t)u32(decoded+at+0x620ef0-base+i*4);
            ok=1;
        }
        at+=length;
    }
done:free(raw);free(decoded);return ok;
}

int gt4_photo_drive_tables_load(const char *disc,float ev[21],float kelvin[11]){
 size_t size=0;uint8_t *raw=gt4_disc_read_root(disc,"CORE.GT4",&size),*decoded=NULL;int ok=0;
 if(!raw || size<6 || raw[0]!=1 || raw[1]!=1)goto done;uint32_t n=u32(raw+2);if(n<0x12c || n>32u*1024*1024)goto done;
 decoded=malloc(n);if(!decoded || core_inflate_raw(raw+6,size-6,decoded,n)!=n || u32(decoded+0x100)!=0x176265b3)goto done;
 for(size_t at=0x12c;at<n;){if(n-at<8)goto done;uint32_t base=u32(decoded+at),length=u32(decoded+at+4);at+=8;if(length>n-at)goto done;
  /* 0036F650/0036F678: original PhotoCamera EV and Kelvin tables. */
  if(base<=0x620f40 && (uint64_t)base+length>=0x620f98+44){memcpy(ev,decoded+at+0x620f40-base,84);memcpy(kelvin,decoded+at+0x620f98-base,44);ok=1;}
  at+=length;
 }done:free(raw);free(decoded);return ok;
}

int gt4_photo_apertures_load(const char *disc,float out[17])
{return gt4_photo_tables_load(disc,out,NULL);}

float gt4_photo_shutter_seconds(int32_t value)
{
    /* 0036FA20 +0..64: negative reciprocal, zero one second, positive /100. */
    return value<0?1.f/(float)-value:value?(float)value/100:1;
}
float gt4_photo_exposure(float seconds)
{
    /* 00333680 +0..34: literal 3C888888, upper clamp only. */
    float extent=seconds/0.01666666567325592041f;
    return extent>1?1:extent;
}
float gt4_photo_time(float exposure,int sample,int count)
{
    /* 00334730 +50..70, frozen world (+124): sample/count * exposure. */
    return ((float)sample/(float)count)*exposure;
}
/* 00459338/004593D0/004594D0: degree normalization, shortest path,
   and the original weighted sum. Values outside +/-7200 become zero. */
static float photo_degree(float a){if(a < -7200 || a>7200)return 0;while(a<0)a+=360;while(a>=360)a-=360;return a;}
/* 004514A0 +40..60: upper hub limit minus the recorded tire radius.
   Scoped toward-zero, same as the original wheel position setter. */
float gt4_photo_wheel_height(float height,float limit,float radius)
{
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    if(limit!=0){float top=limit-radius;if(top<height)height=top;}
    fesetenv(&env);return height;
}
float gt4_photo_angle(float current,float previous,float time)
{
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    float p=photo_degree(previous),c=photo_degree(current),d=c-p;
    if(fabsf(d)>180){if(d>0)p+=360;else c+=360;}
    float result=p*time+c*(1-time);fesetenv(&env);return result;
}
void gt4_photo_lerp(float *out,const float *current,const float *previous,int count,float time)
{
    /* 0033B358 +0..48: time=0 current, time=1 previous; no normalization. */
    float remaining=1-time;
    for(int i=0;i<count;i++){float a=previous[i]*time,b=current[i]*remaining;out[i]=a+b;}
}

float gt4_photo_fov(float mm)
{
    /* 00370D08 +0..2C: trunc(mm*10); 0036F4E0 +0..54: sensor height
       24 / (2*focal length), atan * 114.59155. A: host atan replaces
       the EE polynomial 0057D008 pending an original oracle. */
    if(!isfinite(mm) || mm<.1f || mm>10000)return 0;
    return atanf(24/((float)(int)(mm*10)*2/10))*114.591552734375f;
}

void gt4_photo_step(gt4_photo_mode *p,const gt4_course *course,float x,float z,float turn)
{
    if(!p || !p->ready || !course || !isfinite(x) || !isfinite(z) || !isfinite(turn))return;
    if(!x && !z && !turn){p->turn_frames=0;return;}
    float d=3.141592502593994f/180,c=cosf(p->yaw*d),s=sinf(p->yaw*d);
    if(!strcmp(p->operate_mode,"move")) {
        /* 00371800 +44..68, +E4..11C: 2*FOV/45, 22-frame ramp;
           +128..16C: pitch -30..45. Region angular constraints remain A. */
        p->turn_frames=x || z?(p->turn_frames<22?p->turn_frames+1:22):0;
        float a=-p->roll*d,k=2*gt4_photo_fov(p->view_angle)/45*p->turn_frames/22;
        p->yaw+=(x*cosf(a)+z*sinf(a))*k;
        p->pitch=fminf(45,fmaxf(-30,p->pitch+(-x*sinf(a)+z*cosf(a))*k));
    } else if(!strcmp(p->operate_mode,"roll")) {
        /* 00371A58 +0..28: roll += input.x * 2. */
        p->roll+=x*2;
    } else if(!strcmp(p->operate_mode,"climb")) {
        /* 003719D0 +30..40: height -= input.z * 0x3D888888. */
        p->eye[1]-=z*0.06666666269302368f;
    } else if(!strcmp(p->operate_mode,"pan")) {
        /* 003716A8 +4C..BC: two world-space vectors, 0x3E088888.
           A: region clipping of 00371530 is still pending. */
        p->eye[0]+=(c*x-s*z)*0.13333332538604736f;
        p->eye[2]+=(s*x+c*z)*0.13333332538604736f;
    } else if(!strcmp(p->operate_mode,"car") || !strcmp(p->operate_mode,"camera")) {
        /* A: map-space movement adapter; original 00370640/00370960's
           ramp and polygon restrictions still need an EE comparison. */
        float *pos=!strcmp(p->operate_mode,"car")?p->car:p->eye;
        pos[0]+=x*p->location.map_length/254;
        pos[2]+=z*p->location.map_length/382;
        if(pos==p->car)p->car_angle+=turn*2;
        float ground;
        if(gt4_course_height(course,pos[0],pos[2],100,&ground))pos[1]=ground+(pos==p->eye?1.3f:0);
    }
    float floor=0;
    gt4_course_height(course,p->eye[0],p->eye[2],500,&floor);
    if(p->drive){p->yaw=fmodf(p->yaw+360,360);return;}
    const uint8_t *region=p->location.data+p->location.region_table;
    /* 00370DD0 +90..C4: ceiling is absolute; minimum is above ground.
       A: region 0 until the polygon selector has been ported. */
    p->eye[1]=fmaxf(floor+fmaxf(.1f,f32(region+0x24)),fminf(f32(region+0x28),p->eye[1]));
    p->yaw=fmodf(p->yaw+360,360);
    /* 0036FAE8 +190..1CC: bounds; A: internal polygons pending. */
    p->car[0]=fminf(p->location.bounds[2],fmaxf(p->location.bounds[0],p->car[0]));
    p->car[2]=fminf(p->location.bounds[3],fmaxf(p->location.bounds[1],p->car[2]));
    p->eye[0]=fminf(p->location.bounds[2],fmaxf(p->location.bounds[0],p->eye[0]));
    p->eye[2]=fminf(p->location.bounds[3],fmaxf(p->location.bounds[1],p->eye[2]));
}

void gt4_photo_color(const gt4_photo_mode *p,float o[6])
{
    /* 00194470 +0x55c..61c: EV = brightness/8, Kelvin =
       whiteBalance*2000/5+5500; 003EFC10 +2c..c8, 003C7DA8. */
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    float gain=powf(2,p->brightness*.125f);
    float kelvin=p->drive?p->drive_kelvin:(float)p->white_balance*2000/5+5500;
    float delta=(kelvin-5500)*0.19999998807907104f/2000;
    o[0]=gain*(1-delta);o[1]=gain;o[2]=gain*(1+delta);o[3]=0;
    o[4]=1-p->contrast*0.09999999403953552f;o[5]=1;
    if(!strcmp(p->color_filter,"monochrome"))o[5]=0;
    else if(!strcmp(p->color_filter,"mature")){
        /* 003C7DA8 +130..17c -> 003EFD80 component composition. */
        o[0]*=1.5f;o[1]*=0.94999998807907104f;o[2]*=0.59999996423721313f;
        o[5]*=0.29999998211860657f;
    }
    fesetenv(&env);
}
/* 00579438/005794C0/005795E0, private photo RNG. It never touches race RNG. */
typedef struct {uint32_t v[624];int at;} photo_rng;
static uint32_t photo_random(photo_rng *r)
{
    if(r->at==624){for(int i=0;i<624;i++){
        uint32_t y=(r->v[i]&0x80000000u)|(r->v[(i+1)%624]&0x7fffffffu);
        r->v[i]=r->v[(i+397)%624]^(y>>1)^((0u-(y&1))&0x9908b0dfu);
    }r->at=0;}
    uint32_t y=r->v[r->at++];y^=y>>11;y^=(y<<7)&0x9d2c5680u;y^=(y<<15)&0xefc60000u;return y^(y>>18);
}
static float photo_uniform(photo_rng *r)
{
    /* 00579548: unsigned conversion through signed halve/restore,
       then multiply by 2^-32. */
    uint32_t u=photo_random(r);float f=u&0x80000000u?(float)((u>>1)|(u&1))*2:(float)(int32_t)u;
    return f*0x1p-32f;
}
void gt4_photo_samples(float (*o)[4],int n)
{
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    photo_rng r;r.v[0]=0x12345678u;
    for(unsigned i=1;i<624;i++)r.v[i]=1812433253u*(r.v[i-1]^(r.v[i-1]>>30))+i;
    r.at=624;
    for(int i=0;i<n;i++){
        float x,y;do{x=photo_uniform(&r)*2-1;y=photo_uniform(&r)*2-1;}while(x*x+y*y>1);
        o[i][0]=x;o[i][1]=y;o[i][2]=photo_uniform(&r);o[i][3]=photo_uniform(&r);
    }
    fesetenv(&env);
}
void gt4_photo_lens(float focal,float aperture,float focus,float aspect,float sy,
                    const float sample[4],float shift[2],float jitter[2])
{
    /* 00332550 +a0..124: sensor diagonal in mm, square-root aspect
       conversion, projection Y and twice the original f-number.
       +124..154: aspect correction; +154..160: focus below .1 -> 10000. */
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    float scale=(focal*0.0010000000474974513f)/sqrtf(aspect*aspect+1)*.5f*sy/(aperture*2);
    shift[0]=sample[0]*scale;shift[1]=sample[1]*scale;
    /* The native viewport and sensor use the same aspect: f16/f15=1.
       PhotoPause's original sensor aspect at 003C4D74 is 4/3. */
    if(focus<.1f)focus=10000;
    jitter[0]=shift[0]/focus*(sy/aspect);jitter[1]=shift[1]/focus*sy;
    fesetenv(&env);
}
