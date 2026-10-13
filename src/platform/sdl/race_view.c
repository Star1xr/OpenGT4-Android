/*
 * Race view: the course's world model and a car on it, drawn by the GPU
 * renderer (render3d.c) with a chase camera. The car stands on grid
 * position 0 until a pose is given (the physics, tests/physics/drive.c);
 * with a pose, its wheels are drawn apart and turned by their steer,
 * rotation and suspension travel.
 */
#include "race_view.h"
#include "race_lights.h"
#include "car_model.h"
#include "driver_pose.h"
#include "course.h"
#include "gpb.h"
#include "render3d.h"
#include "race_smoke.h"
#include "race_leaves.h"
#include "wheel_motion.h"
#include "race_pit_crew.h"
#include "physics/car.h"
#include "vol.h"
#include "platform/platform.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 640
#define H 448

#define OPPONENTS 7
typedef struct {
    char key[128];gt4_driver_rig rig;gt4_model *skin,*part[GT4_DRIVER_BONES];
    r3d_mesh *skin_mesh,*part_mesh[GT4_DRIVER_BONES];
} driver_asset;
typedef struct {
    driver_asset *asset;gt4_driver_skeleton skeleton;
    float pose[GT4_DRIVER_BONES][16],palette[GT4_DRIVER_BONES][16];int ready,dirty;
    gt4_driver_animation animation;float steering,grasp[2];
    int brake;   /* 003446B8: the brake pedal (+0x590) above 0, for the lamps */
} driver_actor;

/* The course, the car, the other cars, and the reflection image the
   showroom's cars use */
typedef struct {uint8_t *data;gt4_course course;gt4_course_vision vision[3];gt4_course_surface *surface;gt4_model *world,*sky,*scenery,*far;car_model *car,*other[OPPONENTS];tex1_image *env;int env_count;r3d_lights lights;float light_fade[3][2];gt4_model *env_sky;float env_turn[2],env_color[2],env_plain[2];int fog;float fog_distance,fog_max,fog_rgb[3];
    /* the road's sheen (func_003411D0): mask +0x34, reflection +0x24, its sky +0xC8 */
    gt4_model *sheen_mask,*sheen_set,*sheen_sky;int sheen_size,sheen_blur,sheen_fog;float sheen_rgb[3],sheen_fog_distance,sheen_fog_max,sheen_fog_rgb[3];
    /* the cars' shadows (func_0038F240, func_00398758): colour, alpha, the sun's way times its length, the alpha's factor in the sun and out of it */
    int shadow;float shadow_rgb[3],shadow_alpha,shadow_way[3],shadow_sun,shadow_shade;
    driver_asset driver[OPPONENTS+1];int drivers;driver_actor pilot[OPPONENTS+1];float wheel_exposure;
    gt4_course_gadgets *gadgets;
    gt4_model *photo_map;
    uint8_t photo_environment[0x17f4];int photo_environment_on;
    tex1_image *smoke;int smoke_count;   /* the tyres' smoke texture (course header +0x68) */
    tex1_image *bits;int bit_count;      /* the spark (0) and the leaves (1..8): course header +0x9C */} race_asset;

/* A car's meshes: whole, or body (with its dark hull) and wheels apart */
typedef struct {r3d_mesh *car,*shadow,*cast,*hull,*body,*wheel[4],*brake[4],*steering[GT4_MODEL_STEERING_MAX];float lo[3],hi[3];} car_meshes;
typedef struct {int on,wheels_on,snapshot_on;float snapshot[4][8],interval[4],pos[3],rot[9],wheel_pose[4][3],ghost;} car_pose;
typedef struct {int on;float pos[3],rot[9],sun,level;} car_shadow;

/* Renderer double buffers, 0033B358/0033B3B0. Poses are published once per
   solver tick. A: native pose/driver adapter, not the complete EE world cache. */
typedef struct {
    car_pose car[OPPONENTS+1];car_shadow shadow[OPPONENTS+1];
    driver_actor pilot[OPPONENTS+1];
    float eye[3],rows[9],projection[3];int camera,hide;
} photo_frame;

struct race_view {
    char *disc;
    SDL_Thread *worker;SDL_atomic_t done;race_asset *pending;
    char course_request[96],car_request[96];int color_request;
    char wheel_request[32],wing_request[32];   /* the player's fitted wheel and wing (race_view_set_parts) */
    int photo_request;r3d_mesh *photo_map;
    float film_tires[OPPONENTS+1][4][3];uint8_t film_tires_on[OPPONENTS+1];
    char other_request[OPPONENTS][32],other_wheel[OPPONENTS][32],other_wing[OPPONENTS][32];int other_color[OPPONENTS],other_count;
    race_asset *now;
    float focal,orbit,distance,ground;
    int pose_on;float pose_pos[3],pose_rot[9];
    int wheels_on,wheel_snapshot_on;float wheel_snapshot[4][8],wheel_interval[4],wheel_pose[4][3];
    car_shadow shadow[OPPONENTS+1];
    const gt4_pit_crew *pit_crew[OPPONENTS+1];
    r3d_mesh *world,*sky_mesh,*scenery,*far,*sheen_mask,*sheen_set,*sheen_sky;car_meshes mine,other[OPPONENTS];car_pose other_pose[OPPONENTS];plat_texture *sky;
    r3d_mesh *env_sky;uint8_t *env_faces;tex1_image env_map;int env_ok;   /* the cars' reflection of the course */
    int warned;
    int hide_car,gadget_section;
    r3d_mesh **gadgets;unsigned gadget_types;
    int grade_on;float grade[6];   /* race_view_set_grade */
    float photo_jitter[2];int screen_reflect;
    photo_frame photo_current,photo_previous,photo_saved;int photo_history,photo_sampling;
    int cam_on;float cam_eye[3],cam_rows[9],cam_sx,cam_sy,cam_near;   /* race_view_set_camera */
    r3d_sprite_texture *smoke_tex;const race_smoke *smoke;   /* race_view_set_smoke */
    r3d_sprite_texture *leaf_tex[8];const race_leaves *leaves;   /* race_view_set_leaves */
    r3d_mesh *afterfire[6];int afterfire_tried;   /* race/after_fire.mdl's six frames */
    float flame[OPPONENTS+1][4][3];int flame_on[OPPONENTS+1];   /* race_view_set_afterfire */
};

static void free_asset(race_asset *a)
{if(a){gt4_course_gadgets_close(a->gadgets);gt4_course_surface_free(a->surface);gt4_model_free(a->photo_map);gt4_model_free(a->world);gt4_model_free(a->sky);gt4_model_free(a->scenery);gt4_model_free(a->far);gt4_model_free(a->env_sky);gt4_model_free(a->sheen_mask);gt4_model_free(a->sheen_set);gt4_model_free(a->sheen_sky);free(a->data);car_model_free(a->car);for(int i=0;i<OPPONENTS;i++)car_model_free(a->other[i]);
    for(int i=0;i<a->drivers;i++){gt4_model_free(a->driver[i].skin);for(int b=0;b<GT4_DRIVER_BONES;b++)gt4_model_free(a->driver[i].part[b]);}
    tex1_free(a->env,a->env_count);tex1_free(a->smoke,a->smoke_count);tex1_free(a->bits,a->bit_count);free(a);}}

/* Worker only: one decoded shared pilot per file, independent skeleton state
   per vehicle. The GL thread uploads and releases these CPU geometry copies. */
static void load_driver(gt4_vol *v,race_asset *a,car_model *car,unsigned entry)
{
    if(!car || (!car->driver_path && !car->driver_data) || entry>OPPONENTS)return;
    char key[128];snprintf(key,sizeof key,"%s",car->driver_path?car->driver_path:car->model);
    driver_asset *asset=NULL;
    for(int i=0;i<a->drivers;i++)if(!strcmp(a->driver[i].key,key)){asset=&a->driver[i];break;}
    if(!asset) {
        if(a->drivers>=OPPONENTS+1)return;
        size_t n=car->driver_size;uint8_t *shared=car->driver_path?gt4_vol_read(v,car->driver_path,&n):NULL;
        const uint8_t *d=shared?shared:car->driver_data;
        gt4_driver_rig rig;
        if(!d || !gt4_driver_rig_read(d,n,&rig) || rig.skin_count>1){free(shared);return;}
        asset=&a->driver[a->drivers++];snprintf(asset->key,sizeof asset->key,"%s",key);asset->rig=rig;
        if(rig.skin_count)asset->skin=gt4_model_decode_skin_part(d,n,rig.skin_first,NULL);
        for(int b=0;b<GT4_DRIVER_BONES;b++)if(rig.bone[b].model_count==1) {
            if(b==GT4_LHAND || b==GT4_RHAND)
                asset->part[b]=gt4_model_decode_hand_part(d,n,rig.bone[b].model_first,NULL);
            if(!asset->part[b])asset->part[b]=gt4_model_decode_skin_part(d,n,rig.bone[b].model_first,NULL);
        }
        free(shared);
    }
    driver_actor *pilot=&a->pilot[entry];pilot->asset=asset;float channels[64];
    int geometry=asset->skin!=NULL;
    for(int b=0;b<GT4_DRIVER_BONES;b++)geometry|=asset->part[b]!=NULL;
    pilot->ready=geometry && gt4_driver_skeleton_init(asset->rig.dimensions,&pilot->skeleton) &&
        gt4_driver_record_init(&car->driver,channels) && gt4_driver_race_pose(&car->driver,channels,&pilot->skeleton,pilot->pose) &&
        (!asset->rig.slots || gt4_driver_palette(&asset->rig,pilot->pose,pilot->palette));
    if(pilot->ready){pilot->grasp[0]=channels[0x80/4];pilot->grasp[1]=channels[0x84/4];}
    fprintf(stderr,"race driver: %s source=%s seated=%d slots=%u\n",car->code,key,pilot->ready,asset->rig.slots);
}

/* Original course header (field names as in Nenkai's PDTools
   CourseDataFileGT4): world +04, sky +54, far +58 make the view. The
   environment set +14 (with its sky +5C) is what the cars reflect, not
   part of the view: drawn over the road it left the dark shapes the
   PCSX2 captures do not have (Laguna Seca, s03). Reflection +24 and
   its mask +34 are the road's reflections, +60 the mirror's sky.
   Optional sets must have a valid bounded MDLS; no substituted assets. */
static gt4_model *course_set(const uint8_t *data,size_t size,int field)
{
    size_t at=(size_t)data[field]|(size_t)data[field+1]<<8|(size_t)data[field+2]<<16|(size_t)data[field+3]<<24;
    if(!at || at>size || size-at<128 || memcmp(data+at,"MDLS",4))return NULL;
    return gt4_model_decode_world(data+at,size-at,NULL);
}

/* The course's lights for the cars. The scene's car pass (func_00340950
   -> func_0038EFF8) sets them through func_00398210: the environment block
   ("Env0", header +0x80) switches light i on with bit i of +0x10; each
   light is 0x80 bytes from +0xD0, passed to the light setter
   (func_004A5BF8) as parameters 0xD (+0, the way to the light), 0 (+0x10,
   ambient), 1 (+0x20, diffuse) and 4 (+0x30, specular). The colours go
   through func_00397FF8 with the ground's two values under the car:
   ((1 - (1 - sun) * a) * b * level + 1 - b) / Env0 +0x38, where a and b
   are one less +0xAC8 and +0xACC for the ambient, +0xAB4 and +0xABC for
   the diffuse, +0xAC0 and +0xAC4 for the specular (car_light below; the
   division is undone by the frame's doubling). The lights' block the PS2
   sent to VU1 in the captures s03 and s06 holds these numbers (Laguna
   Seca: ambient 0.30 0.30 0.32, diffuse 1.12 1.12 1.11, specular 1 1
   0.99, each over 2 and times 128). The other set, +0xAD0 with bit 11 + i
   (func_00398058), is not the cars'. */
static void course_lights(const uint8_t *data,size_t size,r3d_lights *out,float fade[3][2])
{
    uint32_t env,flags;float k[8],scale;
    memset(out,0,sizeof *out);memset(fade,0,6*sizeof(float));
    if(size<0x84)return;
    memcpy(&env,data+0x80,4);
    if(!env || env>size || size-env<0xAD0 || memcmp(data+env,"Env0",4))return;
    memcpy(&flags,data+env+0x10,4);memcpy(k,data+env+0xAB0,sizeof k);memcpy(&scale,data+env+0x38,4);
    for(int i=0;i<4;i++) {
        const uint8_t *b=data+env+0xD0+i*0x80;
        if(!(flags>>i&1))continue;
        memcpy(out->direction[out->count],b,12);memcpy(out->ambient[out->count],b+0x10,12);
        memcpy(out->diffuse[out->count],b+0x20,12);memcpy(out->specular[out->count],b+0x30,12);
        out->count++;
    }
    fade[0][0]=1-k[6];fade[0][1]=1-k[7];   /* ambient: +0xAC8, +0xACC */
    fade[1][0]=1-k[1];fade[1][1]=1-k[3];   /* diffuse: +0xAB4, +0xABC */
    fade[2][0]=1-k[4];fade[2][1]=1-k[5];   /* specular: +0xAC0, +0xAC4 */
    /* the vertex colour stops at 255, where 128 is one over +0x38 of these units */
    out->vertex_limit=(scale>0?scale:1)*255/128;
}

/* func_00397FF8: a light's colours for a car over ground of this sun and level */
static void car_light(const r3d_lights *in,const float fade[3][2],float sun,float level,r3d_lights *out)
{
    float f[3];
    *out=*in;
    for(int i=0;i<3;i++)f[i]=(1-(1-sun)*fade[i][0])*fade[i][1]*level+(1-fade[i][1]);
    for(int l=0;l<out->count;l++)for(int c=0;c<3;c++){out->ambient[l][c]*=f[0];out->diffuse[l][c]*=f[1];out->specular[l][c]*=f[2];}
}

/* The course's fog (func_00397CE0 -> func_00397DA0, Env0 +0x60): the
   colour +0x60, the distance +0x70 and the floor of the GS's F +0x74
   (func_004A40B0). While the scene is drawn the colour is divided by
   +0x38 * +0x3C (func_00397DF8, 2 on every course seen: the frame comes
   out doubled) and kept as a GS byte each (func_004A3F98); here it is
   put back in the doubled picture's units. */
static void course_fog(const uint8_t *data,size_t size,race_asset *a)
{
    uint32_t env;float c[3],scale[2],k;
    if(size<0x84)return;
    memcpy(&env,data+0x80,4);
    if(!env || env>size || size-env<0x78 || memcmp(data+env,"Env0",4))return;
    memcpy(c,data+env+0x60,12);memcpy(&a->fog_distance,data+env+0x70,4);memcpy(&a->fog_max,data+env+0x74,4);
    memcpy(scale,data+env+0x38,8);
    k=scale[0]*scale[1];if(!(k>0))k=1;
    for(int i=0;i<3;i++){float v=c[i]/k;v=v<0?0:v>1?1:v;a->fog_rgb[i]=(float)(int)(v*255)/255*2;}
    a->fog=1;
}

/* The road's sheen (func_003411D0, the scene's slot +0x378 after the
   world): Env0 +0xC10 the colour and +0xC1C its strength, +0xC20 the GS
   ALPHA of the sum (0x58: Cs * Ad + Cd), +0xC24/+0xC28 the reflection
   picture's size, +0xC2C how often it is blurred, +0xC40 its own fog
   (colour, +0xC50 distance, +0xC54 floor). The sum is made in the frame
   before it is doubled (func_0033E828: the frame times +0x38 through a
   FIX of 255, 255/128), so the colour carries that factor here. */
static void course_sheen(const uint8_t *data,size_t size,race_asset *a)
{
    uint32_t env,w,h,blur,alpha;float c[4],f[6],k,scale[2];
    if(size<0x84)return;
    memcpy(&env,data+0x80,4);
    if(!env || env>size || size-env<0xC58 || memcmp(data+env,"Env0",4))return;
    memcpy(c,data+env+0xC10,16);memcpy(&alpha,data+env+0xC20,4);
    memcpy(&w,data+env+0xC24,4);memcpy(&h,data+env+0xC28,4);memcpy(&blur,data+env+0xC2C,4);
    memcpy(f,data+env+0xC40,12);memcpy(f+4,data+env+0xC50,8);memcpy(scale,data+env+0x38,8);
    /* only the sum the courses use, on a square picture (all seen are 64 or 128) */
    if((alpha&0xff)!=0x58 || !w || w!=h || w>1024 || blur>8)return;
    a->sheen_mask=course_set(data,size,0x34);
    if(!a->sheen_mask)return;
    a->sheen_set=course_set(data,size,0x24);a->sheen_sky=course_set(data,size,0xC8);
    if(!a->sheen_set && !a->sheen_sky){gt4_model_free(a->sheen_mask);a->sheen_mask=NULL;return;}
    a->sheen_size=(int)w;a->sheen_blur=(int)blur;
    {float fix=scale[0]*128;fix=fix<0?0:fix>255?255:(float)(int)fix;
     for(int i=0;i<3;i++)a->sheen_rgb[i]=c[i]*c[3]*fix/128;}
    k=scale[0]*scale[1];if(!(k>0))k=1;
    for(int i=0;i<3;i++){float v=f[i]/k;v=v<0?0:v>1?1:v;a->sheen_fog_rgb[i]=(float)(int)(v*255)/255;}
    a->sheen_fog_distance=f[4];a->sheen_fog_max=f[5];a->sheen_fog=1;
}

/* The cars' reflection colour (func_00397CE0 -> func_004568B0/A0, given
   to pgl at each model, 00455464): VU1 0x215 multiplies the reflection
   layer's colour by +0x34 + +0x34 * +0x30 * min(I.N, 0) / |I|, a Fresnel
   rise toward the outline. The sum goes into the frame before it is
   doubled, so the frame's factor (as in course_sheen) is carried here. */
static void course_env_colour(const uint8_t *data,size_t size,race_asset *a)
{
    uint32_t env;float k[3];
    if(size<0x84)return;
    memcpy(&env,data+0x80,4);
    if(!env || env>size || size-env<0x3C || memcmp(data+env,"Env0",4))return;
    memcpy(k,data+env+0x30,12);
    if(!isfinite(k[0]) || !isfinite(k[1]) || !isfinite(k[2]))return;
    float fix=k[2]*128;fix=fix<0?0:fix>255?255:(float)(int)fix;
    a->env_color[0]=k[1]*fix/128;a->env_color[1]=k[1]*k[0]*fix/128;
    a->env_plain[0]=k[1];a->env_plain[1]=k[1]*k[0];   /* outside the frame (the results' car icons) */
}

/* The cars' shadows on this course. func_0038F240 (the scene, before each
   car's shadow): Env0 +0xA60 the colour, divided by +0x38 as the frame is
   doubled afterwards, +0xA6C the alpha, and the alpha's factor
   (func_003983E0) (sun * (+0xAB0 - +0xAB8) + +0xAB8) * level, from the
   ground under the car. func_003986E8 -> func_00398758 (with the
   environment): the sun's way from the angles +0xA70 and +0xA74, degrees
   (func_00398438: (sin B cos A, -sin A, cos B cos A) with A, B the angles
   negated), times +0xA78. */
static void course_shadow(const uint8_t *data,size_t size,race_asset *a)
{
    uint32_t env;float c[4],t[3],f[3],scale,fix;
    const float d=3.14159265f/180;
    if(size<0x84)return;
    memcpy(&env,data+0x80,4);
    if(!env || env>size || size-env<0xABC || memcmp(data+env,"Env0",4))return;
    memcpy(c,data+env+0xA60,16);memcpy(t,data+env+0xA70,12);memcpy(f,data+env+0xAB0,12);memcpy(&scale,data+env+0x38,4);
    if(!(scale>0))return;
    /* func_004A6100 keeps the colour in the GS's 255; the frame's doubling is a FIX of 255 at most (func_0033E828) */
    fix=scale*128;fix=fix>255?255:(float)(int)fix;
    for(int i=0;i<3;i++)a->shadow_rgb[i]=c[i]/scale*fix/128;
    a->shadow_alpha=c[3];a->shadow_sun=f[0];a->shadow_shade=f[2];
    {float A=-t[0]*d,B=-t[1]*d;
     a->shadow_way[0]=sinf(B)*cosf(A)*t[2];a->shadow_way[1]=-sinf(A)*t[2];a->shadow_way[2]=cosf(B)*cosf(A)*t[2];}
    a->shadow=1;
}

static race_asset *load(const char *disc,const char *course,const char *car,int color,const char *wheel,const char *wing,char others[][32],char other_wheels[][32],char other_wings[][32],const int *other_colors,int nothers,int photo,const float tires[][4][3],const uint8_t *tires_on)
{
    const char *err=NULL;gt4_vol *v=gt4_vol_open(disc,&err);if(!v)return NULL;
    char path[96];size_t size=0;snprintf(path,sizeof path,"/crs/%s",course);
    race_asset *a=calloc(1,sizeof *a);
    if(a){a->data=gt4_vol_read(v,path,&size);}
    gt4_photo_location location;
    if(!a || !a->data || !gt4_course_open(&a->course,a->data,size) ||
       (photo==1?!gt4_photo_location_read(a->data,size,&location):photo==0?!a->course.grid_count:0)){gt4_vol_close(v);free_asset(a);return NULL;}
    if(photo==1) {
        /* MPhotoMapWindow 00194DB0 +90..124: CRPM +14 is the map MDLS. */
        uint32_t at;memcpy(&at,location.data+0x14,4);
        if(at && at<location.size)a->photo_map=gt4_model_decode_world(location.data+at,location.size-at,NULL);
        if(a->photo_map)for(int i=0;i<a->photo_map->count;i++)for(int k=0;k<3;k++) {
            gt4_vertex *v=&a->photo_map->triangles[i].v[k];
            v->p[1]=-v->p[2];v->p[2]=0;
        }
    }
    a->world=gt4_model_decode_world(a->data+a->course.world,size-a->course.world,NULL);
    if(!a->world || !a->world->count){gt4_vol_close(v);free_asset(a);return NULL;}
    /* 00396E10 ->004580F0 ->00396F00: the camera's nearest authored
       vision node selects world models; never draw all LODs together. */
    for(unsigned pass=0;pass<3;pass++) {
        uint32_t at;memcpy(&at,a->data+8+pass*4,4);
        if(at && (at>=size || !gt4_course_vision_open(a->vision+pass,a->data+at,size-at,a->world->source_models))) {
            fprintf(stderr,"invalid course vision list %u\n",pass);gt4_vol_close(v);free_asset(a);return NULL;
        }
    }
    if(photo!=1)a->gadgets=gt4_course_gadgets_open(&a->course);
    a->sky=course_set(a->data,size,0x54);
    a->far=course_set(a->data,size,0x58);
    a->surface=gt4_course_surface_create(&a->course);
    course_lights(a->data,size,&a->lights,a->light_fade);
    course_fog(a->data,size,a);
    course_sheen(a->data,size,a);
    course_env_colour(a->data,size,a);
    course_shadow(a->data,size,a);
    /* 003D73F8: course header +80 points at Env0; +EA0 is wheel
       exposure. Missing source data disables blur instead of inventing it. */
    {uint32_t env;memcpy(&env,a->data+0x80,4);
     if(env && env<size && size-env>=0xEA4 && !memcmp(a->data+env,"Env0",4))memcpy(&a->wheel_exposure,a->data+env+0xEA0,4);}
    {   /* the smoke's Tex1 (course header +0x68, the world's +0x31D0 +0x34): four frames side by side */
        uint32_t t;memcpy(&t,a->data+0x68,4);
        if(t && t<size && size-t>=16 && !memcmp(a->data+t,"Tex1",4)){int n=tex1_decode(a->data+t,size-t,&a->smoke);a->smoke_count=n>0?n:0;if(n<=0)a->smoke=NULL;}
        /* the particles' Tex1 (header +0x9C, the world's D_00621B44) */
        memcpy(&t,a->data+0x9c,4);
        if(t && t<size && size-t>=16 && !memcmp(a->data+t,"Tex1",4)){int n=tex1_decode(a->data+t,size-t,&a->bits);a->bit_count=n>0?n:0;if(n<=0)a->bits=NULL;}
    }
    a->scenery=course_set(a->data,size,0x14);   /* the environment, for the reflections only */
    a->env_sky=course_set(a->data,size,0x5C);
    {   /* func_0044E528: the environment's sky turned by Env0 +0x48 about x, +0x28 about y (degrees) */
        uint32_t env;memcpy(&env,a->data+0x80,4);
        if(env && env<size && size-env>=0x4C && !memcmp(a->data+env,"Env0",4)){memcpy(&a->env_turn[0],a->data+env+0x48,4);memcpy(&a->env_turn[1],a->data+env+0x28,4);}
    }
    if((car && *car) || nothers) {
        car_model_tables *t=car_model_tables_open(v);
        /* Photo Travel has a static car without RaceCar's driver/LOD path.
           A: high menu geometry adapter until PhotoCar's draw selection is tested. */
        if(car && *car)a->car=photo==2 && tires_on[0]?car_model_load_film_parts(v,t,car,color,nothers+1,0,wheel,wing,tires[0]):photo==1?car_model_load_parts(v,t,car,color,wheel,wing):car_model_load_race_parts(v,t,car,color,nothers+1,0,wheel,wing);
        for(int i=0;i<nothers && i<OPPONENTS;i++)if(others[i][0])a->other[i]=photo==2 && tires_on[i+1]?car_model_load_film_parts(v,t,others[i],other_colors[i],nothers+1,(unsigned)i+1,other_wheels[i],other_wings[i],tires[i+1]):car_model_load_race_parts(v,t,others[i],other_colors[i],nothers+1,(unsigned)i+1,other_wheels[i],other_wings[i]);
        load_driver(v,a,a->car,0);
        for(int i=0;i<nothers;i++)load_driver(v,a,a->other[i],(unsigned)i+1);
        car_model_tables_close(t);
    }
    if(photo && !a->car){gt4_vol_close(v);free_asset(a);return NULL;}
    size_t package_size=0,env_size=0;
    uint8_t *package=gt4_vol_read(v,"/projects/arcade/US/CarRoot.gpb",&package_size);
    uint8_t *env=package?gpb_load(package,package_size,"image/arcade/car/env0.png",&env_size):NULL;
    a->env_count=env?tex1_decode(env,env_size,&a->env):0;free(env);free(package);
    gt4_vol_close(v);return a;
}

static int worker(void *ptr)
{
    race_view *v=ptr;v->pending=load(v->disc,v->course_request,v->car_request,v->color_request,v->wheel_request,v->wing_request,v->other_request,v->other_wheel,v->other_wing,v->other_color,v->other_count,v->photo_request,v->film_tires,v->film_tires_on);
    SDL_AtomicSet(&v->done,1);return 0;
}

static void drop_car(car_meshes *m)
{
    r3d_mesh_destroy(m->car);r3d_mesh_destroy(m->shadow);r3d_mesh_destroy(m->cast);r3d_mesh_destroy(m->hull);r3d_mesh_destroy(m->body);
    for(int i=0;i<4;i++){r3d_mesh_destroy(m->wheel[i]);r3d_mesh_destroy(m->brake[i]);}
    for(int i=0;i<GT4_MODEL_STEERING_MAX;i++)r3d_mesh_destroy(m->steering[i]);
    memset(m,0,sizeof *m);
}

static void drop_meshes(race_view *v)
{
    for(unsigned i=0;i<v->gadget_types;i++)r3d_mesh_destroy(v->gadgets[i]);
    free(v->gadgets);v->gadgets=NULL;v->gadget_types=0;
    if(v->now)for(int i=0;i<v->now->drivers;i++) {
        driver_asset *d=&v->now->driver[i];r3d_mesh_destroy(d->skin_mesh);d->skin_mesh=NULL;
        for(int b=0;b<GT4_DRIVER_BONES;b++){r3d_mesh_destroy(d->part_mesh[b]);d->part_mesh[b]=NULL;}
    }
    r3d_mesh_destroy(v->world);v->world=NULL;
    r3d_mesh_destroy(v->photo_map);v->photo_map=NULL;
    r3d_mesh_destroy(v->sky_mesh);v->sky_mesh=NULL;
    r3d_mesh_destroy(v->scenery);v->scenery=NULL;
    r3d_mesh_destroy(v->far);v->far=NULL;
    r3d_mesh_destroy(v->env_sky);v->env_sky=NULL;v->env_ok=0;
    r3d_mesh_destroy(v->sheen_mask);v->sheen_mask=NULL;r3d_mesh_destroy(v->sheen_set);v->sheen_set=NULL;
    r3d_mesh_destroy(v->sheen_sky);v->sheen_sky=NULL;
    drop_car(&v->mine);for(int i=0;i<OPPONENTS;i++)drop_car(&v->other[i]);
    r3d_sprite_texture_destroy(v->smoke_tex);v->smoke_tex=NULL;
    for(int i=0;i<8;i++){r3d_sprite_texture_destroy(v->leaf_tex[i]);v->leaf_tex[i]=NULL;}
}

static void make_car(car_meshes *m,const car_model *a)
{
    if(!a)return;
    m->car=r3d_mesh_create(a->car);m->shadow=r3d_mesh_create(a->shadow);m->cast=r3d_mesh_create(a->cast);
    int steering_parts=0;
    for(unsigned i=0;i<a->car->steering_count;i++)if(a->steering[i]) {
        gt4_model part=*a->steering[i];part.textures=a->car->textures;part.ntextures=a->car->ntextures;
        m->steering[i]=r3d_mesh_create_shared(&part,m->car);
        steering_parts+=m->steering[i]!=NULL;
    }
    /* Static fallback and posed body omit only uploaded steering parts.
       Textures are shared; the split and all uploads happen once at load. */
    if(steering_parts) {
    gt4_model rest=*a->car;rest.count=0;rest.triangles=malloc((size_t)a->car->count*sizeof *rest.triangles);
    if(rest.triangles) {
        for(int i=0;i<a->car->count;i++) {
            unsigned tag=a->car->triangles[i].attachment;
            if(tag<5 || !m->steering[tag-5])rest.triangles[rest.count++]=a->car->triangles[i];
        }
        if(rest.count<a->car->count) {
            r3d_mesh *mesh=r3d_mesh_create_shared(&rest,m->car);
            if(mesh){r3d_mesh_destroy(m->car);m->car=mesh;}
            else for(int i=0;i<GT4_MODEL_STEERING_MAX;i++){r3d_mesh_destroy(m->steering[i]);m->steering[i]=NULL;}
        }
        free(rest.triangles);
    }else for(int i=0;i<GT4_MODEL_STEERING_MAX;i++){r3d_mesh_destroy(m->steering[i]);m->steering[i]=NULL;}
    }
    for(int k=0;k<3;k++){m->lo[k]=1e30f;m->hi[k]=-1e30f;}
    for(int t=0;t<a->car->count;t++)for(int j=0;j<3;j++)for(int k=0;k<3;k++) {
        float c=a->car->triangles[t].v[j].p[k];
        if(c<m->lo[k])m->lo[k]=c;
        if(c>m->hi[k])m->hi[k]=c;
    }
    if(a->hull)m->hull=r3d_mesh_create(a->hull);
    if(a->body_count>0) {
        gt4_model body=*a->car;body.count=0;
        body.triangles=malloc((size_t)a->body_count*sizeof *body.triangles);
        if(body.triangles) {
            for(int i=0;i<a->body_count;i++) {
                unsigned tag=a->car->triangles[i].attachment;
                if(!tag || (tag>=5 && !m->steering[tag-5]))body.triangles[body.count++]=a->car->triangles[i];
            }
            m->body=r3d_mesh_create_shared(&body,m->car);free(body.triangles);
        }
        for(int i=0;i<4;i++)if(a->wheel[i])m->wheel[i]=r3d_mesh_create(a->wheel[i]);
        for(int i=0;i<4;i++)if(a->brake[i]) {
            gt4_model brake=*a->brake[i];brake.textures=a->car->textures;brake.ntextures=a->car->ntextures;
            m->brake[i]=r3d_mesh_create_shared(&brake,m->car);
        }
    }
}

/* Main thread (GL context current): the meshes of a loaded course and car. */
static void install(race_view *v,race_asset *a)
{
    drop_meshes(v);free_asset(v->now);v->now=a;v->gadget_section=-1;
    if(a->gadgets){
        v->gadget_types=a->gadgets->types;v->gadgets=calloc(v->gadget_types,sizeof *v->gadgets);
        if(v->gadgets)for(unsigned i=0;i<v->gadget_types;i++){
            v->gadgets[i]=r3d_mesh_create_shared(a->gadgets->model[i],i?v->gadgets[0]:NULL);
            gt4_model_free(a->gadgets->model[i]);a->gadgets->model[i]=NULL;
        }else v->gadget_types=0;
        fprintf(stderr,"course objects: %u models, %u placements, %u sections\n",a->gadgets->types,a->gadgets->count,a->gadgets->sections);
    }
    v->focal=224/tanf(20*3.14159265f/180);    /* 40 degrees vertical, a native choice */
    v->orbit=0;v->distance=6.5f;
    gt4_grid g=gt4_course_grid(&a->course,0);v->ground=g.pos[1];
    gt4_course_height(&a->course,g.pos[0],g.pos[2],g.pos[1]+2,&v->ground);
    v->world=r3d_mesh_create(a->world);
    v->photo_map=r3d_mesh_create(a->photo_map);gt4_model_free(a->photo_map);a->photo_map=NULL;
    v->sky_mesh=r3d_mesh_create(a->sky);v->scenery=r3d_mesh_create(a->scenery);v->far=r3d_mesh_create(a->far);
    v->env_sky=r3d_mesh_create(a->env_sky);
    v->sheen_mask=r3d_mesh_create(a->sheen_mask);v->sheen_set=r3d_mesh_create(a->sheen_set);v->sheen_sky=r3d_mesh_create(a->sheen_sky);
    make_car(&v->mine,a->car);
    v->smoke_tex=a->smoke_count>0?r3d_sprite_texture_create(&a->smoke[0]):NULL;
    for(int i=0;i<8;i++)v->leaf_tex[i]=a->bit_count>i+1?r3d_sprite_texture_create(&a->bits[i+1]):NULL;
    memset(v->wheel_interval,0,sizeof v->wheel_interval);
    memset(v->other_pose,0,sizeof v->other_pose);
    for(int i=0;i<OPPONENTS;i++){make_car(&v->other[i],a->other[i]);v->other_pose[i].on=0;}
    for(int i=0;i<a->drivers;i++) {
        driver_asset *d=&a->driver[i];d->skin_mesh=r3d_mesh_create(d->skin);
        r3d_mesh *textures=d->skin_mesh;
        for(int b=0;b<GT4_DRIVER_BONES;b++)if(d->part[b]) {
            d->part_mesh[b]=r3d_mesh_create_shared(d->part[b],textures);
            if(!textures)textures=d->part_mesh[b];
            if(d->part_mesh[b]){gt4_model_free(d->part[b]);d->part[b]=NULL;}
        }
        if(d->skin_mesh){gt4_model_free(d->skin);d->skin=NULL;}
    }
    fprintf(stderr,"course: world=%d triangles, %d grid positions\n",a->world->count,a->course.grid_count);
    /* Uploaded meshes own GPU data. Keep the course/RNW4 buffer for physics
       and surface queries, but release the large decoded CPU model copies. */
    if(v->world){gt4_model_free(a->world);a->world=NULL;}
    if(v->sky_mesh){gt4_model_free(a->sky);a->sky=NULL;}
    if(v->scenery){gt4_model_free(a->scenery);a->scenery=NULL;}
    if(v->far){gt4_model_free(a->far);a->far=NULL;}
    if(v->env_sky){gt4_model_free(a->env_sky);a->env_sky=NULL;}
    if(v->sheen_mask){gt4_model_free(a->sheen_mask);a->sheen_mask=NULL;}
    if(v->sheen_set){gt4_model_free(a->sheen_set);a->sheen_set=NULL;}
    if(v->sheen_sky){gt4_model_free(a->sheen_sky);a->sheen_sky=NULL;}
}

race_view *race_view_open(const char *disc)
{
    race_view *v=calloc(1,sizeof *v);if(!v)return NULL;
    v->disc=strdup(disc);if(!v->disc){free(v);return NULL;}
    v->distance=6.5f;return v;
}

void race_view_close(race_view *v)
{
    if(!v)return;
    if(v->worker){SDL_WaitThread(v->worker,NULL);free_asset(v->pending);}
    for(int f=0;f<6;f++)r3d_mesh_destroy(v->afterfire[f]);
    drop_meshes(v);free_asset(v->now);plat_texture_destroy(v->sky);free(v->env_faces);free(v->env_map.rgba);free(v->disc);free(v);
}

/* the player's car's fitted wheel and wing for the next request (an
   owned car's; NULL or empty the stock ones) */
void race_view_set_parts(race_view *v,const char *wheel,const char *wing)
{
    if(!v || v->worker)return;
    snprintf(v->wheel_request,sizeof v->wheel_request,"%s",wheel && strlen(wheel)<sizeof v->wheel_request?wheel:"");
    snprintf(v->wing_request,sizeof v->wing_request,"%s",wing && strlen(wing)<sizeof v->wing_request?wing:"");
}
/* CDTL 0033A1B0 +78..A8 calls 003B5AF8/003B5B80 for fitted wheel/wing.
   Apply every stored car, not just the active profile's player. */
int race_view_set_film_parts(race_view *v,unsigned entry,const char *wheel,const char *wing)
{
    if(!v || v->worker || entry>OPPONENTS || !wheel || !wing || strlen(wheel)>=32 || strlen(wing)>=32 ||
       strchr(wheel,'/') || strchr(wheel,'\\') || strchr(wing,'/') || strchr(wing,'\\'))return 0;
    char *w=entry?v->other_wheel[entry-1]:v->wheel_request,*g=entry?v->other_wing[entry-1]:v->wing_request;
    strcpy(w,wheel);strcpy(g,wing);return 1;
}
int race_view_set_film_tires(race_view *v,unsigned entry,const float tires[4][3])
{
    if(!v || v->worker || entry>OPPONENTS)return 0;
    if(tires)for(int i=0;i<4;i++)if(!isfinite(tires[i][0]) || !isfinite(tires[i][1]) || !isfinite(tires[i][2]) || tires[i][0]<=0 || tires[i][2]<=0 || tires[i][1]<tires[i][2])return 0;
    v->film_tires_on[entry]=tires!=NULL;if(tires)memcpy(v->film_tires[entry],tires,sizeof v->film_tires[entry]);return 1;
}
int race_view_request(race_view *v,const char *course,const char *car,int color)
{
    if(!v || v->worker || !course || !*course || strlen(course)>80 || strchr(course,'/') || strchr(course,'\\'))return 0;
    if(car && strlen(car)>=sizeof v->car_request)return 0;
    strcpy(v->course_request,course);snprintf(v->car_request,sizeof v->car_request,"%s",car?car:"");v->color_request=color;
    v->photo_request=0;
    SDL_AtomicSet(&v->done,0);
    v->worker=SDL_CreateThread(worker,"race view loader",v);
    return v->worker!=NULL;
}

int race_view_poll(race_view *v,int cancel)
{
    if(!v || !v->worker)return -1;
    if(!SDL_AtomicGet(&v->done))return 0;
    SDL_WaitThread(v->worker,NULL);v->worker=NULL;
    race_asset *a=v->pending;v->pending=NULL;
    if(!a)return -1;
    if(cancel)free_asset(a);else install(v,a);
    return 1;
}

int race_view_request_photo(race_view *v,const char *course,const char *car,int color)
{
    if(!v || v->worker || !course || strncmp(course,"ss_",3) || strlen(course)>=sizeof v->course_request || strchr(course,'/') || strchr(course,'\\') ||
       !car || !*car || strlen(car)>=sizeof v->car_request)return 0;
    strcpy(v->course_request,course);strcpy(v->car_request,car);v->color_request=color;v->other_count=0;v->photo_request=1;
    SDL_AtomicSet(&v->done,0);v->worker=SDL_CreateThread(worker,"Photo Travel loader",v);return v->worker!=NULL;
}

int race_view_request_film(race_view *v,const char *course,const char *car,int color)
{
    /* 00335248 reconstructs the stored scene, also on normal race courses.
       CDTL wheel/wing and DRVR snapshots are applied separately.
       A: body LOD selection still uses the native race model loader. */
    if(!v || v->worker || !course || !*course || strlen(course)>=sizeof v->course_request || strchr(course,'/') || strchr(course,'\\') ||
       !car || !*car || strlen(car)>=sizeof v->car_request)return 0;
    strcpy(v->course_request,course);strcpy(v->car_request,car);v->color_request=color;v->photo_request=2;
    v->photo_history=0;v->photo_sampling=0;v->hide_car=0;
    SDL_AtomicSet(&v->done,0);v->worker=SDL_CreateThread(worker,"Photo film loader",v);return v->worker!=NULL;
}

void race_view_clear(race_view *v)
{
    if(v){v->photo_history=0;v->photo_sampling=0;drop_meshes(v);free_asset(v->now);v->now=NULL;v->pose_on=v->wheels_on=0;}
}

int race_view_set_opponents(race_view *v,const char *const *cars,const int *colors,int n)
{
    if(!v || v->worker || n<0 || n>OPPONENTS)return 0;
    for(int i=0;i<n;i++)if(cars[i] && strlen(cars[i])>=sizeof v->other_request[i])return 0;
    for(int i=0;i<n;i++){snprintf(v->other_request[i],sizeof v->other_request[i],"%s",cars[i]?cars[i]:"");v->other_color[i]=colors?colors[i]:0;}
    memset(v->other_wheel,0,sizeof v->other_wheel);memset(v->other_wing,0,sizeof v->other_wing);
    v->other_count=n;return 1;
}

void race_view_set_opponent(race_view *v,int i,const float pos[3],const float rot[9],const float wheels[4][3])
{
    if(!v || i<0 || i>=OPPONENTS)return;
    car_pose *p=&v->other_pose[i];
    p->on=pos!=NULL;p->wheels_on=wheels!=NULL;p->snapshot_on=0;
    if(pos){memcpy(p->pos,pos,sizeof p->pos);memcpy(p->rot,rot,sizeof p->rot);}
    if(wheels)memcpy(p->wheel_pose,wheels,sizeof p->wheel_pose);
}

void race_view_set_ghost(race_view *v,int i,float alpha)
{if(v && i>=0 && i<OPPONENTS)v->other_pose[i].ghost=alpha;}

void race_view_camera(race_view *v,float orbit,float zoom)
{if(v){v->orbit+=orbit;v->distance=fmaxf(3,fminf(15,v->distance+zoom));}}

int race_view_camera_point_entry(const race_view *v,int entry,int i,float out[8])
{
    if(!v || !v->now || entry<0 || entry>OPPONENTS)return 0;
    car_model *m=entry?v->now->other[entry-1]:v->now->car;
    if(!m || i<0 || i>=m->cam_points)return 0;
    memcpy(out,m->cam_point[i],8*sizeof *out);return 1;
}

int race_view_camera_point(const race_view *v,int i,float out[8])
{
    if(!v || !v->now || !v->now->car || i<0 || i>=v->now->car->cam_points)return 0;
    memcpy(out,v->now->car->cam_point[i],8*sizeof *out);return 1;
}

void race_view_show_car(race_view *v,int on){if(v)v->hide_car=!on;}
int race_view_car_visible(const race_view *v){return v && !v->hide_car;}
void race_view_set_grade(race_view *v,const float grade[6]){if(v){v->grade_on=grade!=NULL;if(grade)memcpy(v->grade,grade,sizeof v->grade);}}
void race_view_set_afterfire(race_view *v,unsigned entry,const float flames[4][3],int on)
{
 if(!v || entry>OPPONENTS)return;
 v->flame_on[entry]=on;if(on)memcpy(v->flame[entry],flames,sizeof v->flame[entry]);
}

/* func_00452AC8, the race's pass (a2 0): each outlet k takes particle k & 3
   (time, size, turn from the car's parameters +0xD8); burning (time above
   0), the outlet's place, Ry(atan2(dx, dz)) Rx(-asin dy) Rz(turn) and
   D_008465D8 (1.0) x size, then model 0 of race/after_fire.mdl whose
   AfterFireCallback (00452F28) picks frame (int)(time x 6). A: the
   frames' tween (00456850, the fraction) and 004A60E0's colour not done */
static r3d_view body_place(const car_model *a,const r3d_view *car);
static void draw_afterfire(race_view *v,const car_model *a,const r3d_view *car,const float flames[4][3])
{
    if(!a || !a->exhaust_points)return;
    if(!v->afterfire_tried){
        v->afterfire_tried=1;const char *err=NULL;gt4_vol *vol=gt4_vol_open(v->disc,&err);size_t size=0;
        uint8_t *bytes=vol?gt4_vol_read(vol,"/race/after_fire.mdl",&size):NULL;
        for(int f=0;bytes && f<6;f++){gt4_model *m=gt4_model_decode_branch(bytes,size,0,f);
            if(m){v->afterfire[f]=r3d_mesh_create(m);gt4_model_free(m);}}
        free(bytes);if(vol)gt4_vol_close(vol);
        if(!v->afterfire[0])fprintf(stderr,"race: no after fire model\n");
    }
    r3d_view body=body_place(a,car);
    for(unsigned k=0;k<a->exhaust_points;k++){
        const float *q=flames[k&3],*e=a->exhaust[k];
        if(!(q[0]>0))continue;
        int frame=(int)(q[0]*6);if(frame>5)frame=5;
        if(!v->afterfire[frame])continue;
        const float rad=0.01745329252f;
        float yaw=atan2f(e[3],e[5]),pitch=-asinf(fmaxf(-1,fminf(1,e[4]))),roll=q[2]*rad,s=q[1];
        float cy=cosf(yaw),sy=sinf(yaw),cp=cosf(pitch),sp=sinf(pitch),cr=cosf(roll),sr=sinf(roll);
        float ry[9]={cy,0,sy, 0,1,0, -sy,0,cy},rx[9]={1,0,0, 0,cp,-sp, 0,sp,cp},rz[9]={cr,-sr,0, sr,cr,0, 0,0,1},yx[9],local[9];
        for(int r=0;r<3;r++)for(int c=0;c<3;c++){yx[r*3+c]=0;for(int i=0;i<3;i++)yx[r*3+c]+=ry[r*3+i]*rx[i*3+c];}
        for(int r=0;r<3;r++)for(int c=0;c<3;c++){local[r*3+c]=0;for(int i=0;i<3;i++)local[r*3+c]+=yx[r*3+i]*rz[i*3+c];local[r*3+c]*=s;}
        r3d_view w=body;
        for(int r=0;r<3;r++){
            w.pos[r]=body.pos[r];
            for(int c=0;c<3;c++){w.rot[r*3+c]=0;for(int i=0;i<3;i++)w.rot[r*3+c]+=body.rot[r*3+i]*local[i*3+c];w.pos[r]+=body.rot[r*3+c]*e[c];}
        }
        w.brake=0;r3d_draw(v->afterfire[frame],&w);
    }
}

unsigned race_view_exhaust_count(const race_view *v,int entry)
{
 if(!v || !v->now || entry<0 || entry>OPPONENTS)return 0;
 const car_model *car=entry?v->now->other[entry-1]:v->now->car;
 return car?car->exhaust_count:0;
}
const gt4_course *race_view_course(const race_view *v){return v && v->now?&v->now->course:NULL;}
const car_model *race_view_car_model(const race_view *v,int entry){
 if(!v || !v->now || entry<0 || entry>OPPONENTS)return NULL;
 return entry?v->now->other[entry-1]:v->now->car;
}
const gt4_driver_car *race_view_car_geometry(const race_view *v,int entry){
 if(!v || !v->now || entry<0 || entry>OPPONENTS)return NULL;
 const car_model*c=entry?v->now->other[entry-1]:v->now->car;return c?&c->driver:NULL;
}

void race_view_set_camera(race_view *v,const float eye[3],const float rows[9],float sx,float sy,float near_clip)
{
    if(!v)return;
    v->cam_on=eye!=NULL;
    if(eye){memcpy(v->cam_eye,eye,sizeof v->cam_eye);memcpy(v->cam_rows,rows,sizeof v->cam_rows);v->cam_sx=sx;v->cam_sy=sy;v->cam_near=near_clip;}
}

int race_view_get_camera(const race_view *v,float eye[3],float rows[9],float projection[3])
{
    if(!v || !v->cam_on)return 0;
    memcpy(eye,v->cam_eye,sizeof v->cam_eye);memcpy(rows,v->cam_rows,sizeof v->cam_rows);
    projection[0]=v->cam_sx;projection[1]=v->cam_sy;projection[2]=v->cam_near;return 1;
}

void race_view_set_shadow(race_view *v,int entry,const float pos[3],const float rot[9],float sun,float level)
{
    if(!v || entry<0 || entry>OPPONENTS)return;
    car_shadow *s=&v->shadow[entry];
    s->on=pos!=NULL;
    if(pos){memcpy(s->pos,pos,sizeof s->pos);memcpy(s->rot,rot,sizeof s->rot);s->sun=sun;s->level=level;}
}

void race_view_set_pose(race_view *v,const float pos[3],const float rot[9])
{
    if(!v)return;
    v->pose_on=pos!=NULL;
    if(pos){memcpy(v->pose_pos,pos,sizeof v->pose_pos);memcpy(v->pose_rot,rot,sizeof v->pose_rot);}
}

void race_view_set_wheels(race_view *v,const float wheels[4][3])
{
    if(!v)return;
    v->wheels_on=wheels!=NULL;v->wheel_snapshot_on=0;
    if(wheels)memcpy(v->wheel_pose,wheels,sizeof v->wheel_pose);
}

void race_view_set_wheel_motion(race_view *v,unsigned entry,const float current[4],const float previous[4],float interpolation)
{
    if(!v || !v->now || entry>OPPONENTS)return;
    float *interval=entry?v->other_pose[entry-1].interval:v->wheel_interval;
    for(int i=0;i<4;i++)interval[i]=current && previous?
        gt4_wheel_motion_degrees(current[i],previous[i],interpolation,v->now->wheel_exposure):0;
}
/* CPRM +3C angles / +48 hub XYZ, stride +34. 00452008 +4C..CC
   passes both vectors to 004514A0/00451518 without axis conversion. */
void race_view_set_wheel_snapshot(race_view *v,unsigned entry,const float wheels[4][8])
{
    if(!v || entry>OPPONENTS)return;
    if(entry){car_pose *p=&v->other_pose[entry-1];p->snapshot_on=wheels!=NULL;p->wheels_on=wheels!=NULL;if(wheels)memcpy(p->snapshot,wheels,sizeof p->snapshot);}
    else {v->wheel_snapshot_on=wheels!=NULL;v->wheels_on=wheels!=NULL;if(wheels)memcpy(v->wheel_snapshot,wheels,sizeof v->wheel_snapshot);}
}
/* 00339CF8 +104..114 restores the two recorded finger closure values.
   003B1248 publishes hand bones; the hand part renderer consumes grasp. */
void race_view_set_driver_grasp(race_view *v,unsigned entry,const float grasp[2])
{if(v && v->now && entry<=OPPONENTS && grasp)memcpy(v->now->pilot[entry].grasp,grasp,8);}
int race_view_set_driver_snapshot(race_view *v,unsigned entry,const gt4_driver_skeleton *s)
{
    if(!v || !v->now || entry>OPPONENTS)return 0;
    driver_actor *p=&v->now->pilot[entry];if(!s){p->ready=0;return 1;}if(!p->asset)return 1;
    float pose[20][16],palette[20][16];memcpy(palette,p->palette,sizeof palette);
    if(!gt4_driver_skeleton_matrices(s,pose) || (p->asset->rig.slots && !gt4_driver_palette(&p->asset->rig,pose,palette)))return 0;
    p->skeleton=*s;memcpy(p->pose,pose,sizeof pose);memcpy(p->palette,palette,sizeof palette);
    p->animation.initialized=0;p->dirty=0;p->ready=1;return 1;
}
int race_view_set_driver_frame(race_view *v,unsigned entry,const float channels[64])
{
    if(!v || !v->now || entry>OPPONENTS || !channels)return 0;
    driver_actor *p=&v->now->pilot[entry];const car_model *c=entry?v->now->other[entry-1]:v->now->car;
    if(!c || !isfinite(channels[0x34/4]))return 0;
    if(!p->asset){p->steering=channels[0x34/4];return 1;}
    gt4_driver_skeleton next=p->skeleton;float pose[20][16],palette[20][16];memcpy(palette,p->palette,sizeof palette);
    if(!gt4_driver_race_pose(&c->driver,channels,&next,pose) || (p->asset->rig.slots && !gt4_driver_palette(&p->asset->rig,pose,palette)))return 0;
    p->skeleton=next;memcpy(p->pose,pose,sizeof pose);memcpy(p->palette,palette,sizeof palette);
    p->steering=channels[0x34/4];p->grasp[0]=channels[0x80/4];p->grasp[1]=channels[0x84/4];p->ready=1;return 1;
}
int race_view_step_driver(race_view *v,unsigned entry,const int transmission[4],const gt4_driver_animation_input *input)
{
    if(!v || !v->now || entry>OPPONENTS || !transmission || !input)return 0;
    driver_actor *p=&v->now->pilot[entry];const car_model *c=entry?v->now->other[entry-1]:v->now->car;
    p->brake=input->brake>0;
    if(!c || !isfinite(input->steering))return 0;
    if(!p->animation.initialized && !gt4_driver_animation_init(&p->animation,&c->driver,transmission,input->gear)) {
        /* Ordinary wheel rotation does not require a drawable character. */
        if(gt4_driver_control_type(&c->driver)!=0)return 0;
        float limit=c->driver.shift_type==1?1.570796251296997f:8.377579689025879f;
        p->steering=fminf(limit,fmaxf(-limit,input->steering));return 1;
    }
    if(!gt4_driver_animation_step(&p->animation,&c->driver,input))return 0;
    p->steering=p->animation.channels[0x34/4];p->dirty=p->ready;return 1;
}

/* Car placement and chase camera: the pose when set, otherwise grid
   position 0 on the road height under it. */
static void place(const race_view *v,float pos[3],float rot[9],float cam[3],float target[3])
{
    if(v->pose_on) {
        /* forward is -z of the model: minus the third column */
        float fx=-v->pose_rot[2],fz=-v->pose_rot[8],len=sqrtf(fx*fx+fz*fz),h;
        if(len<1e-4f){fx=0;fz=1;len=1;}
        fx/=len;fz/=len;h=atan2f(fx,fz)+v->orbit;
        memcpy(pos,v->pose_pos,3*sizeof *pos);memcpy(rot,v->pose_rot,9*sizeof *rot);
        cam[0]=pos[0]-sinf(h)*v->distance;cam[1]=pos[1]+2.2f;cam[2]=pos[2]-cosf(h)*v->distance;
        target[0]=pos[0]+fx*2;target[1]=pos[1]+.8f;target[2]=pos[2]+fz*2;
        return;
    }
    gt4_grid g=gt4_course_grid(&v->now->course,0);float ground=v->ground;
    float fx=sinf(g.heading),fz=cosf(g.heading),yaw=g.heading+3.14159265f,sn=sinf(yaw),cs=cosf(yaw);
    float r[9]={cs,0,sn, 0,1,0, -sn,0,cs};memcpy(rot,r,sizeof r);
    pos[0]=g.pos[0];pos[1]=ground;pos[2]=g.pos[2];
    cam[0]=g.pos[0]-sinf(g.heading+v->orbit)*v->distance;cam[1]=ground+2.2f;
    cam[2]=g.pos[2]-cosf(g.heading+v->orbit)*v->distance;
    target[0]=g.pos[0]+fx*2;target[1]=ground+.8f;target[2]=g.pos[2]+fz*2;
}

static float dot3(const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}

static void look_at(r3d_view *r,const float cam[3],const float target[3])
{
    float f[3],len;
    for(int k=0;k<3;k++){r->cam[k]=cam[k];f[k]=target[k]-cam[k];}
    len=sqrtf(dot3(f,f));for(int k=0;k<3;k++)r->f[k]=f[k]/len;
    r->r[0]=-r->f[2];r->r[1]=0;r->r[2]=r->f[0];len=sqrtf(dot3(r->r,r->r));for(int k=0;k<3;k++)r->r[k]/=len;
    r->u[0]=r->r[1]*r->f[2]-r->r[2]*r->f[1];r->u[1]=r->r[2]*r->f[0]-r->r[0]*r->f[2];r->u[2]=r->r[0]*r->f[1]-r->r[1]*r->f[0];
}

/* A wheel's placement from the car's: turned by its steer about the model's
   up axis and by its rotation about the axle (Ry * Rx), its hub lifted. */
static void wheel_place(const car_model *a,const float pose[3],const float *snapshot,int i,const r3d_view *car,r3d_view *w)
{
    float cy=cosf(pose[0]),sy=sinf(pose[0]),cx=cosf(pose[1]),sx=sinf(pose[1]);
    float local[9]={cy,sy*sx,sy*cx, 0,cx,-sx, -sy,cy*sx,cy*cx};
    float hub[3]={a->hub[i][0],a->hub[i][1]+pose[2],a->hub[i][2]};
    if(snapshot){
        /* 00454110 +28..50: hub, RotateY(steer), RotateZ(camber);
           004541A0 +08: RotateX(spin). Hub-to-rim width is already baked
           into the PC wheel mesh. 004514A0 clamps Y after interpolation. */
        float cy=cosf(snapshot[4]),sy=sinf(snapshot[4]),cz=cosf(snapshot[5]),sz=sinf(snapshot[5]),cx=cosf(snapshot[3]),sx=sinf(snapshot[3]);
        float m[9]={cy*cz,-cy*sz*cx+sy*sx,cy*sz*sx+sy*cx,sz,cz*cx,-cz*sx,-sy*cz,sy*sz*cx+cy*sx,-sy*sz*sx+cy*cx};
        memcpy(local,m,sizeof m);memcpy(hub,snapshot,12);hub[1]=gt4_photo_wheel_height(hub[1],a->wheel_limit[i/2],snapshot[7]);
    }
    *w=*car;
    for(int r=0;r<3;r++) {
        w->pos[r]=car->pos[r];
        for(int c=0;c<3;c++){
            w->rot[r*3+c]=0;
            for(int k=0;k<3;k++)w->rot[r*3+c]+=car->rot[r*3+k]*local[k*3+c];
            w->pos[r]+=car->rot[r*3+c]*hub[c];
        }
    }
}
void race_view_set_pit_crew(race_view*v,unsigned entry,const gt4_pit_crew*c){if(v && entry<=OPPONENTS)v->pit_crew[entry]=c;}
static void car_draw(r3d_mesh *mesh,const r3d_view *view);
static int draw_pit_tire(const gt4_pit_crew*c,unsigned role,const r3d_view*car,r3d_mesh*mesh){
 if(!c || c->stage<1 || !c->placed)return 0;
 const char *labels[]={"tire0","tire1"};float markers[2][16];
 if(gt4_license_motion_frames_blend(c->motion,role,c->blend[role],labels,2,markers)!=2)return 0;
 unsigned cars=GT4_U32(GT4_U32(c->car+4)+0xf8a0);int model=gt4_pit_actor_model(c,role,cars);
 for(unsigned tire=0;tire<2;tire++){
  if(!gt4_pit_tire_visible(c->stage,tire))continue;
  float matrix[16];if(!gt4_pit_tire_draw_matrix(c,role,tire,model,markers[tire],matrix))continue;
  r3d_view w=*car;memcpy(w.pos,matrix+12,12);
  for(unsigned r=0;r<3;r++)for(unsigned col=0;col<3;col++)w.rot[r*3+col]=matrix[col*4+r]*((role&1) && col!=1?-1.f:1.f);
  /* car_model's resident wheel already contains the right-side mirror.
     Cancel 003D1488's final right-side half-turn before reusing it. */
  w.wheel_blur=0;w.wheel_interval=0;car_draw(mesh,&w);
 }
 /* 003D2AF8 / 003CF7C8: at approach/departure the car still owns
    the mounted wheel; only stage 3 replaces that draw with both props. */
 return c->stage==3;
}

/* A car: its shadow, its dark hull (hides the far wheels and the
   underbody), then the body with its wheels turned, or the whole model */
static r3d_view body_place(const car_model *a,const r3d_view *car)
{
    r3d_view body=*car;float factory[9]={1,0,0,0,a->body_cosine,a->body_sine,0,-a->body_sine,a->body_cosine};
    for(int row=0;row<3;row++) {
        body.pos[row]=car->pos[row]+car->rot[row*3+1]*a->body_lift;
        for(int column=0;column<3;column++){body.rot[row*3+column]=0;for(int k=0;k<3;k++)body.rot[row*3+column]+=car->rot[row*3+k]*factory[k*3+column];}
    }
    return body;
}
static r3d_view pit_body_place(const car_model*a,const r3d_view*car,const gt4_pit_crew*pit,int baked)
{
    r3d_view out=*car;float root[16];
    if(!pit || !pit->stage || !gt4_pit_car_matrix(pit,root))return baked?out:body_place(a,car);
    /* 004595C8/00459610 draw the body from the live reference frame,
       which rises on the jack. car_model baked the showroom's fixed
       suspension lift/tilt into its vertices: undo that fixed transform
       here before applying the original live parent, without physics writes. */
    const float inverse[9]={1,0,0,0,a->body_cosine,-a->body_sine,0,a->body_sine,a->body_cosine};
    for(unsigned r=0;r<3;r++){
        out.pos[r]=root[12+r];
        for(unsigned c=0;c<3;c++){
            out.rot[r*3+c]=0;for(unsigned k=0;k<3;k++)out.rot[r*3+c]+=root[k*4+r]*(baked?inverse[k*3+c]:(k==c?1.f:0.f));
        }
        if(baked)out.pos[r]-=out.rot[r*3+1]*a->body_lift;
    }
    return out;
}
static void pit_mounted_place(const gt4_pit_crew*pit,unsigned role,r3d_view*w)
{
    float m[16];if(pit && pit->stage && gt4_pit_mounted_matrix(pit,role,m))memcpy(w->pos,m+12,12);
}
/* the lights the cars are drawn with this frame (the course's, or none) */
static const r3d_lights *car_lights;
static void car_draw(r3d_mesh *mesh,const r3d_view *view){if(car_lights)r3d_draw_lit(mesh,view,car_lights);else r3d_draw(mesh,view);}
/* A car's shadow, as the scene draws it before the car (func_00340950 ->
   func_003907A0 -> func_00451D18), in the shadow's own frame (the car's
   place, its heading and the ground's slope), raised 0.02 (the pose's
   +0x80, func_003D7448) and 0.008 nearer in depth (func_00452418):
   - func_004524B8: model 1, the car's dark volume, laid flat along the
     sun (x += y * way.x, z += y * way.z, y = 0, the way taken into the
     shadow's frame), the shadow's colour added, its alpha times the
     ground's factor. The race's model (/car/lod) fades that volume's
     alpha with height, 77 of 128 at the ground down to 0 at the roof;
   - func_00452660: model 2, the soft blot under the car, with the alpha
     as it is.
   Both blend (Cs - Cd) * As + Cd without writing depth. Not done: the
   courses with a table of lamps (func_0038F168: one flat volume a lamp,
   func_00451EE0). Without the
   race's frame (the course viewer) the shadow is laid on the surface
   under the car and the ground counts as lit. */
static void draw_shadow(const race_asset *env,const car_meshes *m,const gt4_course_surface *surface,const r3d_view *car,const car_shadow *from)
{
    r3d_view shadow=*car;shadow.shadow=1;
    float ground,normal[3],sun=1,level=1;
    if(from && from->on) {
        memcpy(shadow.pos,from->pos,sizeof shadow.pos);memcpy(shadow.rot,from->rot,sizeof shadow.rot);
        sun=from->sun;level=from->level;
    } else if(gt4_course_surface_at(surface,car->pos[0],car->pos[2],car->pos[1]+2,&ground,normal)) {
        shadow.pos[1]=ground;
        float back[3]={car->rot[2],car->rot[5],car->rot[8]},d=dot3(back,normal);
        for(int k=0;k<3;k++)back[k]-=normal[k]*d;
        float len=sqrtf(dot3(back,back));
        if(len>1e-6f) {
            for(int k=0;k<3;k++)back[k]/=len;
            float right[3]={normal[1]*back[2]-normal[2]*back[1],normal[2]*back[0]-normal[0]*back[2],normal[0]*back[1]-normal[1]*back[0]};
            for(int k=0;k<3;k++){shadow.rot[k*3]=right[k];shadow.rot[k*3+1]=normal[k];shadow.rot[k*3+2]=back[k];}
        }
    }
    for(int k=0;k<3;k++)shadow.pos[k]+=shadow.rot[k*3+1]*.02f;
    shadow.depth_bias=.008f;
    if(env && env->shadow) {
        memcpy(shadow.shadow_rgb,env->shadow_rgb,sizeof shadow.shadow_rgb);
        float alpha=env->shadow_alpha*(sun*(env->shadow_sun-env->shadow_shade)+env->shadow_shade)*level;
        if(m->cast && alpha>0 && !getenv("OPENGT4_NO_CAST_SHADOW")) {
            r3d_view flat=shadow;float way[3]={0,0,0};
            for(int c=0;c<3;c++)for(int k=0;k<3;k++)way[c]+=shadow.rot[k*3+c]*env->shadow_way[k];
            for(int k=0;k<3;k++)flat.rot[k*3+1]=shadow.rot[k*3]*way[0]+shadow.rot[k*3+2]*way[2];
            flat.shadow_alpha=alpha;
            r3d_draw(m->cast,&flat);
        }
        shadow.shadow_alpha=env->shadow_alpha;
    }
    r3d_draw(m->shadow,&shadow);
}

static void draw_driver(const driver_actor *pilot,const car_model *a,const r3d_view *car,const gt4_pit_crew *pit);

static void draw_car(const race_asset *env,const car_meshes *m,const car_model *a,const gt4_course_surface *surface,const r3d_view *seen,const car_shadow *from,int posed,const float wheels[4][3],const float snapshot[4][8],const float interval[4],int wheel_samples,float steering,const gt4_pit_crew*pit,const driver_actor *pilot)
{
    r3d_lights lights;r3d_view lit=*seen;const r3d_view *car=&lit;
    if(env){lit.env_color[0]=env->env_color[0];lit.env_color[1]=env->env_color[1];}
    if(!seen->ghost)draw_shadow(env,m,surface,car,from);   /* the ghost casts none (PCSX2, Laguna Seca TT) */
    int triangle=-1;float ground;
    /* A: immutable surface adapter selects the triangle beneath the drawn
       pose. Solver formulas/parameters and its contact cache stay untouched. */
    if(surface && env && env->data[env->course.runway+0x40]+env->data[env->course.runway+0x41])
        gt4_course_surface_triangle(surface,seen->pos[0],seen->pos[2],seen->pos[1]+5,&ground,NULL,&triangle);
    int on=from && from->on;
    if(env && race_lights_sample_environment(&env->course,triangle,seen->pos,on?from->sun:1,on?from->level:1,
        env->photo_environment_on?env->photo_environment:NULL,sizeof env->photo_environment,&lights))car_lights=&lights;
    else if(env && env->lights.count) {
        car_light(&env->lights,env->light_fade,on?from->sun:1,on?from->level:1,&lights);car_lights=&lights;
    } else car_lights=NULL;
    /* The GS window stream retains its authored depth writes. Populate the
       cockpit first, so the blended glass covers the pilot and steering
       wheel instead of rejecting them later. Same depth dependency as brake/rim below.
       Native pass ordering; full PS2 car/driver command sequence still A. */
    if(pilot)draw_driver(pilot,a,car,pit);
    r3d_view body=pit_body_place(a,car,pit,0);
    for(unsigned i=0;i<a->car->steering_count;i++)if(m->steering[i]) {
        float matrix[16];if(!gt4_model_steering_matrix(a->car,i,steering,matrix))continue;
        r3d_view part=body;
        for(int row=0;row<3;row++) {
            part.pos[row]=body.pos[row];for(int k=0;k<3;k++)part.pos[row]+=body.rot[row*3+k]*matrix[12+k];
            for(int col=0;col<3;col++){part.rot[row*3+col]=0;for(int k=0;k<3;k++)part.rot[row*3+col]+=body.rot[row*3+k]*matrix[col*4+k];}
        }
        car_draw(m->steering[i],&part);
    }
    if(m->hull)r3d_draw(m->hull,car);
    if(m->body && posed) {
        r3d_view body=pit_body_place(a,car,pit,1);car_draw(m->body,&body);
        /* each wheel's brake before the wheel, as the race body's MDLS calls
           its brake (callbacks 11..14) before its wheel (15..18): the rim's
           see-through texels write depth, so a brake drawn after it would
           not show between the spokes (Legacy B4, user report) */
        for(int i=0;i<4;i++) {
            if(m->brake[i]) {
                float pose[3]={wheels[i][0],0,wheels[i][2]};r3d_view w;
                float brake_snapshot[8];if(snapshot){memcpy(brake_snapshot,snapshot[i],sizeof brake_snapshot);brake_snapshot[3]=0;}
                wheel_place(a,pose,snapshot?brake_snapshot:NULL,i,car,&w);pit_mounted_place(pit,(unsigned)(i^1),&w);car_draw(m->brake[i],&w);
            }
            if(m->wheel[i]) {
                if(draw_pit_tire(pit,(unsigned)(i^1),car,m->wheel[i]))continue;
                r3d_view w;wheel_place(a,wheels[i],snapshot?snapshot[i]:NULL,i,car,&w);float degrees=interval?interval[i]:0;
                pit_mounted_place(pit,(unsigned)(i^1),&w);
                w.wheel_blur=wheel_samples>1?wheel_samples:gt4_wheel_motion_blurred(degrees);
                w.wheel_interval=gt4_wheel_motion_radians(degrees);car_draw(m->wheel[i],&w);
            }
        }
    } else car_draw(m->car,car);

}
static void draw_driver(const driver_actor *pilot,const car_model *a,const r3d_view *car,const gt4_pit_crew*pit)
{
    if(!pilot->ready || !pilot->asset)return;
    r3d_view body=pit_body_place(a,car,pit,0);
    if(pilot->asset->skin_mesh){body.skin_palette=pilot->palette;r3d_draw(pilot->asset->skin_mesh,&body);}
    body.skin_palette=NULL;
    for(int b=0;b<GT4_DRIVER_BONES;b++)if(pilot->asset->part_mesh[b]) {
        r3d_view bone=body;const float *m=pilot->pose[b];
        bone.grasp=b==GT4_LHAND?pilot->grasp[0]:b==GT4_RHAND?pilot->grasp[1]:0;
        for(int row=0;row<3;row++) {
            bone.pos[row]=body.pos[row];for(int k=0;k<3;k++)bone.pos[row]+=body.rot[row*3+k]*m[12+k];
            for(int column=0;column<3;column++){bone.rot[row*3+column]=0;for(int k=0;k<3;k++)bone.rot[row*3+column]+=body.rot[row*3+k]*m[column*4+k];}
        }
        r3d_draw(pilot->asset->part_mesh[b],&bone);
    }
}

static plat_texture *sky(void)
{
    uint8_t *px=malloc((size_t)W*H*4);if(!px)return NULL;
    for(int y=0;y<H;y++)for(int x=0;x<W;x++) {
        uint8_t *p=px+((size_t)y*W+x)*4;float k=(float)y/H;
        p[0]=(uint8_t)(150-40*k);p[1]=(uint8_t)(175-45*k);p[2]=(uint8_t)(200-50*k);p[3]=255;
    }
    plat_texture *t=plat_texture_create(W,H,px);free(px);return t;
}

/* The cars' reflection of the course, every frame (func_0044E660 with
   func_0044E528): from 0.8 above the car (the reflection camera,
   func_0044EEA8), the environment's sky (+0x5C, turned) and the
   environment set (+0x14). The PS2 renders three 120-degree views and
   remaps them with a mesh (func_0044EB10, D_006A7D00); as the showroom
   does for its studio, the six faces of a cube are rendered here and each
   sphere-map texel looks up its reflected direction, as seen by the
   view's camera, in them. */
enum{ENV_FACE=128,ENV_SIZE=128};
static void course_reflection(race_view *v,const float centre[3],const r3d_view *eye)
{
    static const float axes[6][2][3]={{{1,0,0},{0,1,0}},{{-1,0,0},{0,1,0}},{{0,0,1},{0,1,0}},{{0,0,-1},{0,1,0}},{{0,1,0},{0,0,-1}},{{0,-1,0},{0,0,1}}};
    const float d=3.14159265f/180,ax=v->now->env_turn[0]*d,ay=v->now->env_turn[1]*d;
    const float cx=cosf(ax),sx=sinf(ax),cy=cosf(ay),sy=sinf(ay);
    const float turn[9]={cy,0,sy, sx*sy,cx,-sx*cy, -cx*sy,sx,cx*cy};   /* Rx * Ry */
    if(!v->env_sky && !v->scenery)return;
    if(!v->env_faces && !(v->env_faces=malloc((size_t)6*ENV_FACE*ENV_FACE*4)))return;
    if(!v->env_map.rgba) {
        if(!(v->env_map.rgba=malloc((size_t)ENV_SIZE*ENV_SIZE*4)))return;
        v->env_map.width=v->env_map.height=v->env_map.tex_w=v->env_map.tex_h=ENV_SIZE;
        v->env_map.max_u=v->env_map.max_v=ENV_SIZE-1;
    }
    for(int face=0;face<6;face++) {
        const float *ff=axes[face][0],*uu=axes[face][1];
        r3d_view f={{1,0,0,0,1,0,0,0,1}};
        f.focal=320;f.focal_y=224;f.near_clip=.05f;f.unlit=1;   /* as the GS has it in memory: not doubled */
        memcpy(f.f,ff,12);memcpy(f.u,uu,12);
        f.r[0]=ff[1]*uu[2]-ff[2]*uu[1];f.r[1]=ff[2]*uu[0]-ff[0]*uu[2];f.r[2]=ff[0]*uu[1]-ff[1]*uu[0];
        if(!r3d_begin_target(ENV_FACE))return;
        if(v->env_sky) {
            r3d_view s=f;memcpy(s.rot,turn,sizeof turn);
            r3d_draw(v->env_sky,&s);
            r3d_clear_depth();
        }
        f.cam[0]=centre[0];f.cam[1]=centre[1]+.8f;f.cam[2]=centre[2];
        r3d_draw(v->scenery,&f);
        r3d_end_target(v->env_faces+(size_t)face*ENV_FACE*ENV_FACE*4);
    }
    for(int y=0;y<ENV_SIZE;y++)for(int x=0;x<ENV_SIZE;x++) {
        /* sphere-map texel -> view-space reflection -> world direction (the showroom's) */
        float mx=((x+.5f)/ENV_SIZE-.5f)*2,my=-((y+.5f)/ENV_SIZE-.5f)*2,m2=mx*mx+my*my;
        if(m2>1){float k=1/sqrtf(m2);mx*=k;my*=k;m2=1;}
        float nz=sqrtf(1-m2),R[3]={2*nz*mx,2*nz*my,2*nz*nz-1},D[3];
        for(int k=0;k<3;k++)D[k]=R[0]*eye->r[k]+R[1]*eye->u[k]-R[2]*eye->f[k];
        int best=0;float big=-1;
        for(int face=0;face<6;face++){const float *ff=axes[face][0];float dd=D[0]*ff[0]+D[1]*ff[1]+D[2]*ff[2];if(dd>big){big=dd;best=face;}}
        const float *ff=axes[best][0],*uu=axes[best][1];
        float rr[3]={ff[1]*uu[2]-ff[2]*uu[1],ff[2]*uu[0]-ff[0]*uu[2],ff[0]*uu[1]-ff[1]*uu[0]};
        float qx=(D[0]*rr[0]+D[1]*rr[1]+D[2]*rr[2])/big,qy=(D[0]*uu[0]+D[1]*uu[1]+D[2]*uu[2])/big;
        /* filtered between the face's four nearest pixels: the PS2 remaps
           its views drawn as textures on the mesh (its TEX1 not read) */
        float fx=fminf(ENV_FACE-1,fmaxf(0,ENV_FACE/2+qx*ENV_FACE/2-.5f)),fy=fminf(ENV_FACE-1,fmaxf(0,ENV_FACE/2-qy*ENV_FACE/2-.5f));
        int x0=(int)fx,y0=(int)fy,x1=x0+1<ENV_FACE?x0+1:x0,y1=y0+1<ENV_FACE?y0+1:y0;float wx=fx-x0,wy=fy-y0;
        const uint8_t *face=v->env_faces+(size_t)best*ENV_FACE*ENV_FACE*4;
        uint8_t *out=v->env_map.rgba+((size_t)y*ENV_SIZE+x)*4;
        for(int c=0;c<3;c++){
            float a=face[(y0*ENV_FACE+x0)*4+c]*(1-wx)+face[(y0*ENV_FACE+x1)*4+c]*wx;
            float b=face[(y1*ENV_FACE+x0)*4+c]*(1-wx)+face[(y1*ENV_FACE+x1)*4+c]*wx;
            out[c]=(uint8_t)(a*(1-wy)+b*wy+.5f);
        }
        out[3]=255;
    }
    v->env_ok=1;
    r3d_environment_changed();
}

/* The road's sheen, after the world and before the cars (func_003411D0):
   the frame's alpha cleared (func_00342370) and the mask set drawn into
   it (it writes alpha alone, against the world's depth); the reflection's
   sky (+0xC8, as the sky: about the eye, no fog) and set (+0x24, with its
   own fog) drawn from the same eye into a small picture, in the GS's own
   colours; that picture blurred and added over the view through the mask
   (r3d_end_target_add). */
static void road_sheen(race_view *v,const r3d_view *world)
{
    const race_asset *a=v->now;
    r3d_view m=*world,t=*world;
    if(!v->sheen_mask || a->sheen_size<=0 || getenv("OPENGT4_NO_SHEEN"))return;
    r3d_clear_alpha(0);
    m.fog=0;
    r3d_draw(v->sheen_mask,&m);
    if(!r3d_begin_target_view(a->sheen_size))return;
    t.color_scale=1;
    if(v->sheen_sky) {
        r3d_view s=t;memset(s.cam,0,sizeof s.cam);s.near_clip=.01f;s.fog=0;
        r3d_draw(v->sheen_sky,&s);
        r3d_clear_depth();
    }
    t.fog=a->sheen_fog;t.fog_distance=a->sheen_fog_distance;t.fog_max=a->sheen_fog_max;
    memcpy(t.fog_rgb,a->sheen_fog_rgb,sizeof t.fog_rgb);
    if(v->sheen_set)r3d_draw(v->sheen_set,&t);
    r3d_end_target_add(a->sheen_rgb,a->sheen_blur);
}

static int split_mode;
static int split_entry; /* active drawing pass, never a simulation index */
static int draw_scene_box(race_view *v,int pregrid,int menu,int photo,float x,float y,float w,float h)
{
    if(!v || !v->now)return 0;
    if(!r3d_available()){if(!v->warned++)fprintf(stderr,"race view: needs the GPU renderer\n");return 0;}
    for(unsigned entry=0;entry<=OPPONENTS;entry++) {
        driver_actor *p=&v->now->pilot[entry];
        if(pregrid || ((int)entry==split_entry && v->hide_car) || (entry && !v->other_pose[entry-1].on))continue;
        if(p->dirty){race_view_set_driver_frame(v,entry,p->animation.channels);p->dirty=0;}
    }
    const uint8_t tint[4]={255,255,255,255};
    if(!v->sky)v->sky=sky();
    plat_draw_quad(v->sky,x,y,w,h,0,0,1,1,tint);
    float pos[3],rot[9],cam[3],target[3];
    place(v,pos,rot,cam,target);
    r3d_view world={{1,0,0,0,1,0,0,0,1}};world.focal=v->focal;world.unlit=1;look_at(&world,cam,target);
    /* The course comes out doubled: textures are authored at half
       intensity (texels up to 127). Against PCSX2's screenshots of the
       same moments: Laguna Seca's textured sky (s03, colours ~128,
       texels <= 127) is 2.00x the GS's texel * colour / 128 in every
       channel, and Tsukuba's untextured sky (b_reta) twice its vertex
       colours (71 -> 142). Where the game doubles (its vertex program or
       a pass over the frame) is not read yet. */
    world.color_scale=2;
    world.fog=v->now->fog && !getenv("OPENGT4_NO_FOG");world.fog_distance=v->now->fog_distance;world.fog_max=v->now->fog_max;
    memcpy(world.fog_rgb,v->now->fog_rgb,sizeof world.fog_rgb);
    if(v->cam_on) {   /* the game's camera: its axes are the rotation's rows, looking down -z */
        for(int k=0;k<3;k++){world.cam[k]=v->cam_eye[k];world.r[k]=v->cam_rows[k];world.u[k]=v->cam_rows[3+k];world.f[k]=-v->cam_rows[6+k];}
        /* the game's scales are of the 4:3 picture: in the renderer's pixels of
           the 640x448 view (whose width it widens by 640 / (448 * 4/3)) */
        world.focal=v->cam_sx*(menu?320:(H*4.f/3)/2);world.focal_y=v->cam_sy*H/2;world.near_clip=v->cam_near;
    }
    memcpy(world.jitter,v->photo_jitter,sizeof world.jitter);
    world.screen_reflect=v->screen_reflect;
    r3d_view car=world;car.unlit=0;car.color_scale=0;
    memcpy(car.rot,rot,sizeof rot);memcpy(car.pos,pos,sizeof pos);
    if(!pregrid)course_reflection(v,pos,&world);
    /* the Dive's filter on the course, not the cars: rp_04 (PCSX2) shows
       its fence at (1, 0.6, 0.35) of the grey and the cars' paint as it
       is (A: the pass that does it is not read) */
    if(!menu && v->grade_on)memcpy(world.grade,v->grade,sizeof world.grade);
    if(photo)r3d_begin_ui_environment(x,y,w,h,v->env_ok?&v->env_map:v->now->env_count>0?v->now->env:NULL);
    else r3d_begin(x,y,w,h,v->env_ok?&v->env_map:v->now->env_count>0?v->now->env:NULL);
    if(v->sky_mesh) {
        /* A sky centred on the eye keeps its authored angular size and
           orientation while the camera travels through the course. */
        r3d_view s=world;memset(s.cam,0,sizeof s.cam);s.near_clip=.01f;
        s.fog=0;   /* func_00396B80: the sky with the fog at distance 0 and floor 1, none */
        r3d_draw(v->sky_mesh,&s);
        /* The dome (some 40 m across) is behind everything: the distant
           set (+0x58, kilometres away, the hills on the horizon of
           the PCSX2 captures) draws over it. The original's way of
           doing it is not read yet; the picture is the captures'. */
        r3d_clear_depth();
    }
    r3d_draw(v->far,&world);
    r3d_view selected=world;
    unsigned pass=v->screen_reflect?1:split_mode?2:0;
    selected.model_list=gt4_course_vision_select(v->now->vision+pass,world.cam,NULL);
    r3d_draw(v->world,&selected);
    road_sheen(v,&world);
    /* 003E5338 / 003E4A28: normal GDGT models in the object frame,
       Env0 lighting from the original per-object RNW4 light bytes.
       Geometry/textures stay resident; each draw changes only its pose. */
    if(v->gadgets && v->now->gadgets && !getenv("OPENGT4_TEST_HIDE_GADGETS")){
        gt4_course_gadgets *g=v->now->gadgets;
        for(unsigned pass=0;pass<2;pass++){
            if(pass && (v->gadget_section<0 || (unsigned)v->gadget_section>=g->sections))continue;
            unsigned slot=pass?(unsigned)v->gadget_section+1:0;
            for(unsigned j=0;j<g->length[slot];j++){
                gt4_course_gadget *p=g->instance+g->first[slot]+j;if(!gt4_course_gadget_visible(p))continue;r3d_view object=world;
                memcpy(object.pos,p->pos,12);memcpy(object.rot,p->rot,36);object.unlit=0;object.color_scale=0;
                r3d_lights lights;car_light(&v->now->lights,v->now->light_fade,p->sun,p->shade,&lights);
                r3d_draw_lit(v->gadgets[p->type],&object,&lights);
            }
        }
    }
    car_lights=NULL;
    /* 00340950 +564/+59C: camera car requests 16, other cars 1. */
    car.brake=v->now->pilot[0].brake;
    if(!pregrid && v->now->car && (split_entry || !v->hide_car)){draw_car(v->now,&v->mine,v->now->car,v->now->surface,&car,&v->shadow[0],v->pose_on && v->wheels_on,v->wheel_pose,v->wheel_snapshot_on?v->wheel_snapshot:NULL,v->wheel_interval,split_entry==0?16:1,v->now->pilot[0].steering,v->pit_crew[0],&v->now->pilot[0]);}
    for(int i=0;!pregrid && i<OPPONENTS;i++) {
        const car_pose *p=&v->other_pose[i];
        if(!v->now->other[i] || !p->on || (split_entry==i+1 && v->hide_car))continue;
        r3d_view o=car;memcpy(o.rot,p->rot,sizeof o.rot);memcpy(o.pos,p->pos,sizeof o.pos);
        o.brake=v->now->pilot[i+1].brake;
        /* the ghost: car and driver's depth first, then their nearest
           surface blended over the scene */
        o.ghost_alpha=p->ghost;
        for(int pass=p->ghost>0;pass<=(p->ghost>0?2:0);pass++) {
            o.ghost=pass;
            draw_car(v->now,&v->other[i],v->now->other[i],v->now->surface,&o,&v->shadow[i+1],p->wheels_on,p->wheel_pose,p->snapshot_on?p->snapshot:NULL,p->interval,split_entry==i+1?16:1,v->now->pilot[i+1].steering,v->pit_crew[i+1],&v->now->pilot[i+1]);
        }
    }
    /* the backfire's flames after the cars, added over them */
    if(!pregrid && v->now->car && (split_entry || !v->hide_car) && v->flame_on[0])draw_afterfire(v,v->now->car,&car,v->flame[0]);
    for(int i=0;!pregrid && i<OPPONENTS;i++) {
        const car_pose *p=&v->other_pose[i];
        if(!v->now->other[i] || !p->on || p->ghost>0 || !v->flame_on[i+1])continue;
        r3d_view o=car;memcpy(o.rot,p->rot,sizeof o.rot);memcpy(o.pos,p->pos,sizeof o.pos);
        draw_afterfire(v,v->now->other[i],&o,v->flame[i+1]);
    }
    if(!pregrid && v->leaves) {   /* func_003C2838 from the cars' draw (0033EAB8): each leaf, its picture */
        static race_leaves_quad leaves[128];
        static float corners[4*9];
        float rows[9];
        for(int k=0;k<3;k++){rows[k]=world.r[k];rows[3+k]=world.u[k];rows[6+k]=-world.f[k];}
        int n=race_leaves_quads(v->leaves,world.cam,rows,leaves,128);
        for(int q=0;q<n;q++) {
            const r3d_sprite_texture *t=leaves[q].picture>=0 && leaves[q].picture<8?v->leaf_tex[leaves[q].picture]:NULL;
            if(!t)continue;
            for(int c=0;c<4;c++) {
                float *o=corners+c*9;
                memcpy(o,leaves[q].xyz[c],3*sizeof(float));memcpy(o+3,leaves[q].uv[c],2*sizeof(float));
                memcpy(o+5,leaves[q].rgb,3*sizeof(float));o[8]=1;
            }
            r3d_draw_sprites(t,&world,corners,1,2);
        }
    }
    if(!pregrid && v->smoke && v->smoke_tex) {   /* func_003E7CC8 from the cars' draw (0033F2E8) */
        static race_smoke_quad quads[256];
        static float corners[256*4*9];
        float rows[9];
        for(int k=0;k<3;k++){rows[k]=world.r[k];rows[3+k]=world.u[k];rows[6+k]=-world.f[k];}
        int n=race_smoke_quads(v->smoke,world.cam,rows,quads,256);
        for(int q=0;q<n;q++)for(int c=0;c<4;c++) {
            float *o=corners+((size_t)q*4+c)*9;
            memcpy(o,quads[q].xyz[c],3*sizeof(float));memcpy(o+3,quads[q].uv[c],2*sizeof(float));memcpy(o+5,quads[q].rgba,4*sizeof(float));
        }
        r3d_draw_sprites(v->smoke_tex,&world,corners,n,race_smoke_blend(v->smoke)==0x48);
    }
    r3d_end();
    return v->now->car!=NULL;
}

gt4_course_gadgets *race_view_gadgets(race_view *v,int section)
{if(!v || !v->now)return NULL;v->gadget_section=section;return v->now->gadgets;}

void race_view_set_smoke(race_view *v,const race_smoke *s){if(v)v->smoke=s;}
void race_view_set_leaves(race_view *v,const race_leaves *l){if(v)v->leaves=l;}

static int draw_scene(race_view *v,int pregrid,int menu){return draw_scene_box(v,pregrid,menu,0,0,0,W,menu?480:H);}
int race_view_draw(race_view *v){split_entry=0;return draw_scene(v,0,0);}
int race_view_draw_player(race_view *v,int entry,int show,float y,float h)
{
    if(!v || entry<0 || entry>OPPONENTS)return 0;
    int hidden=v->hide_car;split_mode=1;split_entry=entry;v->hide_car=!show;
    int drawn=draw_scene_box(v,0,0,0,0,y,W,h);
    split_mode=0;split_entry=0;v->hide_car=hidden;return drawn;
}
int race_view_draw_mirror(race_view *v,int entry,float x,float y,float w,float h)
{
    if(!v || entry<0 || entry>OPPONENTS)return 0;
    int hidden=v->hide_car,reflection=v->screen_reflect;split_entry=entry;v->hide_car=1;v->screen_reflect=1;
    int ok=draw_scene_box(v,0,0,0,x,y,w,h);
    split_entry=0;v->hide_car=hidden;v->screen_reflect=reflection;return ok;
}
static void photo_get(const race_view *v,photo_frame *f)
{
    memset(f,0,sizeof *f);car_pose *c=&f->car[0];
    c->on=v->pose_on;c->wheels_on=v->wheels_on;c->snapshot_on=v->wheel_snapshot_on;memcpy(c->snapshot,v->wheel_snapshot,sizeof c->snapshot);
    memcpy(c->pos,v->pose_pos,sizeof c->pos);memcpy(c->rot,v->pose_rot,sizeof c->rot);
    memcpy(c->wheel_pose,v->wheel_pose,sizeof c->wheel_pose);memcpy(c->interval,v->wheel_interval,sizeof c->interval);
    memcpy(f->car+1,v->other_pose,sizeof v->other_pose);memcpy(f->shadow,v->shadow,sizeof v->shadow);
    if(v->now)memcpy(f->pilot,v->now->pilot,sizeof f->pilot);
    f->camera=v->cam_on;f->hide=v->hide_car;memcpy(f->eye,v->cam_eye,sizeof f->eye);memcpy(f->rows,v->cam_rows,sizeof f->rows);
    f->projection[0]=v->cam_sx;f->projection[1]=v->cam_sy;f->projection[2]=v->cam_near;
}
static void photo_put(race_view *v,const photo_frame *f)
{
    const car_pose *c=&f->car[0];v->pose_on=c->on;v->wheels_on=c->wheels_on;v->wheel_snapshot_on=c->snapshot_on;memcpy(v->wheel_snapshot,c->snapshot,sizeof c->snapshot);
    memcpy(v->pose_pos,c->pos,sizeof c->pos);memcpy(v->pose_rot,c->rot,sizeof c->rot);
    memcpy(v->wheel_pose,c->wheel_pose,sizeof c->wheel_pose);memcpy(v->wheel_interval,c->interval,sizeof c->interval);
    memcpy(v->other_pose,f->car+1,sizeof v->other_pose);memcpy(v->shadow,f->shadow,sizeof v->shadow);
    if(v->now)memcpy(v->now->pilot,f->pilot,sizeof f->pilot);
    v->cam_on=f->camera;v->hide_car=f->hide;memcpy(v->cam_eye,f->eye,sizeof f->eye);memcpy(v->cam_rows,f->rows,sizeof f->rows);
    v->cam_sx=f->projection[0];v->cam_sy=f->projection[1];v->cam_near=f->projection[2];
}
void race_view_photo_record(race_view *v)
{
    if(!v || v->photo_sampling)return;
    v->photo_previous=v->photo_current;photo_get(v,&v->photo_current);
    if(!v->photo_history)v->photo_previous=v->photo_current;
    v->photo_history=1;
}
void race_view_photo_begin(race_view *v)
{if(v && !v->photo_sampling){photo_get(v,&v->photo_saved);v->photo_sampling=1;}}
void race_view_photo_end(race_view *v)
{if(v && v->photo_sampling){photo_put(v,&v->photo_saved);v->photo_sampling=0;}}
void race_view_photo_sample(race_view *v,float time,float eye[3],float rows[9])
{
    if(!v || !v->photo_sampling)return;
    photo_frame f=v->photo_saved;
    if(v->photo_history && time>0){
        for(int i=0;i<=OPPONENTS;i++){
            car_pose *a=&f.car[i];const car_pose *b=&v->photo_previous.car[i];
            if(a->on && b->on){
                gt4_photo_lerp(a->pos,f.car[i].pos,b->pos,3,time);
                gt4_photo_lerp(a->rot,f.car[i].rot,b->rot,9,time);
                if(a->snapshot_on && b->snapshot_on)for(int wheel=0;wheel<4;wheel++){
                    float *q=a->snapshot[wheel];const float *p=b->snapshot[wheel];gt4_photo_lerp(q,q,p,3,time);
                    /* 0045AAA0 +70..8C overrides interpolated wheel spin
                       with current spin minus the recorded angular delta. */
                    q[3]-=q[6]*time;
                    for(int k=4;k<6;k++)q[k]=gt4_photo_angle(q[k]*57.29577951308232f,p[k]*57.29577951308232f,time)*.01745329252f;
                }
                gt4_photo_lerp(&a->wheel_pose[0][0],&f.car[i].wheel_pose[0][0],&b->wheel_pose[0][0],12,time);
            }
            if(f.shadow[i].on && v->photo_previous.shadow[i].on){
                gt4_photo_lerp(f.shadow[i].pos,f.shadow[i].pos,v->photo_previous.shadow[i].pos,3,time);
                gt4_photo_lerp(f.shadow[i].rot,f.shadow[i].rot,v->photo_previous.shadow[i].rot,9,time);
            }
        }
        /* 0033B3B0 uses the camera's two nine-float frames. A: apply the
           stored camera delta to the user's edited camera, preserving its tool pose. */
        if(v->photo_current.camera && v->photo_previous.camera){
            for(int k=0;k<3;k++)f.eye[k]+=(v->photo_previous.eye[k]-v->photo_current.eye[k])*time;
            for(int k=0;k<9;k++)f.rows[k]+=(v->photo_previous.rows[k]-v->photo_current.rows[k])*time;
        }
    }
    photo_put(v,&f);
    if(v->now && v->photo_history && time>0)for(int i=0;i<=OPPONENTS;i++){
        const driver_actor *a=&v->photo_saved.pilot[i],*b=&v->photo_previous.pilot[i];float channels[64];
        if(!a->animation.initialized && !b->animation.initialized && a->ready && b->ready){gt4_driver_skeleton skeleton;if(gt4_driver_skeleton_blend(&skeleton,&a->skeleton,&b->skeleton,time))race_view_set_driver_snapshot(v,(unsigned)i,&skeleton);float grasp[2];gt4_photo_lerp(grasp,a->grasp,b->grasp,2,time);race_view_set_driver_grasp(v,(unsigned)i,grasp);}
        if(a->animation.initialized && b->animation.initialized){
            gt4_photo_lerp(channels,a->animation.channels,b->animation.channels,64,time);
            race_view_set_driver_frame(v,(unsigned)i,channels);
        }
    }
    if(eye)memcpy(eye,v->cam_eye,12);if(rows)memcpy(rows,v->cam_rows,36);
}

void race_view_photo_jitter(race_view *v,const float j[2]){if(v){if(j)memcpy(v->photo_jitter,j,8);else memset(v->photo_jitter,0,8);}}
int race_view_photo_pose(const race_view *v,float p[3],float r[9]){if(!v || !v->pose_on)return 0;memcpy(p,v->pose_pos,12);memcpy(r,v->pose_rot,36);return 1;}
int race_view_draw_photo(race_view *v,float x,float y,float w,float h){return draw_scene_box(v,0,1,1,x,y,w,h);}
int race_view_draw_photo_map(race_view *v,const gt4_photo_mode *p,float x,float y,float w,float h)
{
    if(!v || !v->photo_map || !p || p->location.map_length<=0 || w<=0 || h<=0)return 0;
    /* A: orthographic map adapter. Authored MDLS retained, x/z becomes
       a flat UI plane at z=-1; native polygon highlighting remains F. */
    float scale=2/p->location.map_length;
    r3d_view view={0};view.rot[0]=scale;view.rot[4]=scale*w/h;
    view.pos[0]=-p->eye[0]*scale;view.pos[1]=p->eye[2]*scale*w/h;view.pos[2]=-1;
    view.r[0]=1;view.u[1]=1;view.f[2]=-1;view.focal=320;view.focal_y=224;view.unlit=1;view.color_scale=2;view.near_clip=.01f;
    r3d_begin_ui(x,y,w,h);r3d_draw(v->photo_map,&view);r3d_end();return 1;
}
/* SceneViewFace (0012BC18), the pregrid's 640x480 world-only pass.
   001119A0 +0x24..+0x80 removes the race object before entering the menu.
   Cars and their cube reflection passes are unnecessary in this mode. */
int race_view_draw_pregrid(race_view *v){return draw_scene(v,1,1);}
/* 0011D1B8 +0x44..0x60 keeps RaceLicense in Scene mode 2;
   0012BFE0 +0x28..0x7C draws its full scene (slots 348/358/360).
   The car/wheels/driver/reflections are retained, unlike 001119A0's
   empty Arcade menu. A: inherits the existing SceneViewFace pixel adapter. */
int race_view_draw_license_menu(race_view *v){return draw_scene(v,0,1);}

/* The icon maker's other picture (func_00393D20's perspective branch, the
   96x64 one 00393790 draws for B-Spec's current_car with mode 0): the car
   turned at least 158 degrees about y (00393CF0 the rest) and 2 about x,
   seen through a narrow lens (half angle about 0.157, 004A59B8), lit as
   the side view. A: 22 degrees off its nose on its left (180 - 158), 5
   up, as the race's gap icon in a PCSX2 print (nearly head-on, the front
   at the picture's left),
   the size in the box from the Race Monitor print (00393CF0's own turn
   not read) */
void race_view_draw_icon_front(race_view *v,int i,float x,float y,float w,float h)
{
    const car_model *a;const car_meshes *m;
    if(!v || !v->now || i<0 || i>OPPONENTS || !r3d_available() || w<=0 || h<=0)return;
    a=i==0?v->now->car:v->now->other[i-1];m=i==0?&v->mine:&v->other[i-1];
    if(!a || !m->car)return;
    float c3[3],R=0;
    for(int k=0;k<3;k++){c3[k]=(m->lo[k]+m->hi[k])*.5f;float e=(m->hi[k]-m->lo[k])*.5f;R+=e*e;}
    R=sqrtf(R);if(!(R>0))return;
    const float yaw=-22*3.14159265f/180,pitch=5*3.14159265f/180;
    const float t=tanf(0.157f),dist=R/t*0.55f;
    float d[3]={sinf(yaw)*cosf(pitch),sinf(pitch),-cosf(yaw)*cosf(pitch)};   /* from the car to the eye */
    r3d_view c={{1,0,0,0,1,0,0,0,1}};
    for(int k=0;k<3;k++){c.cam[k]=c3[k]+d[k]*dist;c.f[k]=-d[k];}
    {   /* r = f x up, u = r x f */
        float up[3]={0,1,0};
        c.r[0]=c.f[1]*up[2]-c.f[2]*up[1];c.r[1]=c.f[2]*up[0]-c.f[0]*up[2];c.r[2]=c.f[0]*up[1]-c.f[1]*up[0];
        float n=sqrtf(c.r[0]*c.r[0]+c.r[1]*c.r[1]+c.r[2]*c.r[2]);for(int k=0;k<3;k++)c.r[k]/=n;
        c.u[0]=c.r[1]*c.f[2]-c.r[2]*c.f[1];c.u[1]=c.r[2]*c.f[0]-c.r[0]*c.f[2];c.u[2]=c.r[0]*c.f[1]-c.r[1]*c.f[0];
    }
    /* the picture is 96x64 (00393B70), stretched to whatever box shows
       it (the gap's 64x32, func_003A6678) */
    c.focal=320*(64.f/96)/t;c.focal_y=224/t;c.near_clip=dist-R*2>0.1f?dist-R*2:0.1f;
    c.env_color[0]=v->now->env_plain[0];c.env_color[1]=v->now->env_plain[1];
    r3d_lights lights;memset(&lights,0,sizeof lights);
    {
        float e[3]={-10.5f,.5f,-1},n=sqrtf(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]);
        for(int j=0;j<3;j++){
            lights.direction[0][j]=(e[0]*c.r[j]+e[1]*c.u[j]-e[2]*c.f[j])/n;
            lights.ambient[0][j]=.8f;lights.diffuse[0][j]=1.5f;lights.specular[0][j]=1;
        }
        lights.count=1;lights.vertex_limit=255.f/128;
    }
    r3d_begin_ui(x,y,w,h);
    if(m->hull)r3d_draw_lit(m->hull,&c,&lights);
    r3d_draw_lit(m->car,&c,&lights);
    r3d_end();
}

/* A car's icon (the game's CarIconMaker, func_00393D20, the side view the
   results show): the model from its side, facing left, without
   perspective (a far camera), its length or height at 0.85 of the box
   (func_004A7890), its height's middle at the box's (func_004A7844). */
void race_view_draw_icon(race_view *v,int i,float x,float y,float w,float h)
{
    const car_model *a;const car_meshes *m;
    const float D=200;
    if(!v || !v->now || i<0 || i>OPPONENTS || !r3d_available() || w<=0 || h<=0)return;
    a=i==0?v->now->car:v->now->other[i-1];m=i==0?&v->mine:&v->other[i-1];
    if(!a || !m->car)return;
    float len=m->hi[2]-m->lo[2],ht=m->hi[1]-m->lo[1];
    if(!(len>0 && ht>0))return;
    /* The icon's reflection picture (func_00393D20, 0x00393E88..0x00393F40):
       a 128x128 target cleared black, its top 64 rows filled in eight bands
       of 8 from white to black ((7 - i) / 7), copied and bound as the
       model's environment (vtable +0xA8 -> func_004568F0 slot 0) */
    static uint8_t gradient[128*128*4];static tex1_image environment;
    if(!environment.rgba) {
        for(int y=0;y<128;y++)for(int x=0;x<128;x++) {
            uint8_t *p=gradient+(y*128+x)*4;
            int c=y<64?(int)((7-y/8)/7.f*255+.5f):0;
            p[0]=p[1]=p[2]=(uint8_t)c;p[3]=255;
        }
        environment.width=environment.height=environment.tex_w=environment.tex_h=128;environment.rgba=gradient;
    }
    float k=.85f*fminf(w/len,h/ht);   /* box units per metre */
    r3d_view c={{1,0,0,0,1,0,0,0,1}};
    c.cam[0]=-D;c.cam[1]=(m->lo[1]+m->hi[1])*.5f;c.cam[2]=0;
    c.r[0]=0;c.r[1]=0;c.r[2]=1;c.u[0]=0;c.u[1]=1;c.u[2]=0;c.f[0]=1;c.f[1]=0;c.f[2]=0;
    c.focal=320*2*k/w*D;c.focal_y=224*2*k/h*D;c.near_clip=D-50;
    /* the model's reflection colour as the race leaves it (00455464), in
       the icon's own target: not the frame, so not doubled */
    c.env_color[0]=v->now->env_plain[0];c.env_color[1]=v->now->env_plain[1];
    /* func_00394A10: light 1 alone, ambient 0.8, diffuse 1.5, specular 1
       (D_00621480/90/A0), its way (1, 0.5, -10.5) (D_00621470) normalized
       and taken through the modelview it is set under (func_004ABEC0):
       the side view's RotY(90) then its scale, s in the 2x target's pixels
       a metre across and 1 in depth. Read here as a direction again. */
    r3d_lights lights;memset(&lights,0,sizeof lights);
    {
        float s=2*k,e[3]={-10.5f*s,.5f*s,-1},n=sqrtf(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]);
        for(int j=0;j<3;j++) {
            lights.direction[0][j]=(e[0]*c.r[j]+e[1]*c.u[j]-e[2]*c.f[j])/n;
            lights.ambient[0][j]=.8f;lights.diffuse[0][j]=1.5f;lights.specular[0][j]=1;
        }
        lights.count=1;lights.vertex_limit=255.f/128;   /* the icon's target is not doubled */
    }
    r3d_begin_ui_environment(x,y,w,h,&environment);
    if(m->hull)r3d_draw_lit(m->hull,&c,&lights);
    r3d_draw_lit(m->car,&c,&lights);
    r3d_end();
}

int race_view_photo_environment(race_view *v,const uint8_t *data,size_t size){
 /* 0033A150/0033A838 copy the complete Env0 parameter record (17F4).
    Reuse the renderer's existing course environment boundary, never mutate
    the borrowed course. A: lighting/reflection output still needs PS2 pixels. */
 if(!v || !v->now || !data || size!=0x17f4 || memcmp(data,"Env0",4))return 0;
 memcpy(v->now->photo_environment,data,size);v->now->photo_environment_on=1;
 uint8_t framed[0x90+0x17f4]={0};uint32_t offset=0x90;memcpy(framed+0x80,&offset,4);memcpy(framed+0x90,data,size);
 course_lights(framed,sizeof framed,&v->now->lights,v->now->light_fade);course_fog(framed,sizeof framed,v->now);
 course_sheen(framed,sizeof framed,v->now);course_env_colour(framed,sizeof framed,v->now);course_shadow(framed,sizeof framed,v->now);v->env_ok=0;return 1;
}
