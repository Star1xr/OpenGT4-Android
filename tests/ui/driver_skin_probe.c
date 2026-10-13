/* Real GL regression: optional on machines with a video driver. Source
   geometry starts off-screen; only the weighted GPU palette moves it into
   view. Verify pixels, culling and a subsequent static draw in the same frame. */
#include "model.h"
#include "driver_model.h"
#include "vol.h"
#include "render3d.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "platform/platform.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "miniz.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned red_pixels(void)
{
    int w=0,h=0;unsigned n=0;unsigned char *px=plat_sdl_read_pixels(&w,&h);
    if(!px)return 0;
    for(int i=0;i<w*h;i++)if(px[i*4]>180 && px[i*4+1]<30 && px[i*4+2]<30)n++;
    free(px);return n;
}
static void clear(void)
{
    plat_video_begin_frame();glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    r3d_begin(0,0,640,448,NULL);
}
static int specular_mask(void)
{
    gt4_triangle tri[2]={0};const float xyz[3][3]={{-.5f,-.5f,4},{.5f,-.5f,4},{0,.5f,4}};
    tri[0].kind=1;tri[0].texture=-1;tri[0].state.depth_write=tri[0].state.depth_test=1;tri[0].state.depth_func=2;
    for(int j=0;j<3;j++){memcpy(tri[0].v[j].p,xyz[j],12);tri[0].v[j].normal[2]=-1;tri[0].v[j].ambient[0]=1;tri[0].v[j].alpha=1;tri[0].v[j].shine=16;}
    tri[1]=tri[0];tri[1].kind=4;tri[1].blend_enabled=1;tri[1].state.blend=0x58;tri[1].state.depth_write=0;
    for(int j=0;j<3;j++)tri[1].v[j].rgb[0]=1;
    gt4_model m={0};m.triangles=tri;m.count=m.capacity=1;r3d_mesh *base=r3d_mesh_create(&m);CHECK(base);
    m.count=m.capacity=2;r3d_mesh *both=r3d_mesh_create(&m);CHECK(both);
    r3d_view v={0};v.rot[0]=v.rot[4]=v.rot[8]=v.r[0]=v.u[1]=v.f[2]=1;v.focal=320;v.focal_y=224;
    clear();r3d_draw(base,&v);r3d_end();int w,h,w2,h2;unsigned char *a=plat_sdl_read_pixels(&w,&h);CHECK(a);
    clear();r3d_draw(both,&v);r3d_end();unsigned char *b=plat_sdl_read_pixels(&w2,&h2);CHECK(b && w==w2 && h==h2);
    unsigned lit=0;for(int i=0;i<w*h;i++){CHECK(!memcmp(a+i*4,b+i*4,3));if(a[i*4]>40)lit++;}CHECK(lit>500);
    free(a);free(b);r3d_mesh_destroy(base);r3d_mesh_destroy(both);
    return 0;
}
/* A blended windshield may write depth on the PS2. Its pilot has to be
   present before the glass pass, while opaque bodywork still hides him. */
static int windshield(void)
{
    gt4_triangle t={0};t.kind=0;t.texture=-1;
    t.state.depth_write=t.state.depth_test=1;t.state.depth_func=2;
    const float xyz[3][3]={{-.8f,-.8f,4},{.8f,-.8f,4},{0,.8f,4}};
    for(int j=0;j<3;j++){memcpy(t.v[j].p,xyz[j],12);t.v[j].rgb[0]=t.v[j].alpha=1;}
    gt4_model m={0};m.triangles=&t;m.count=m.capacity=1;
    r3d_mesh *pilot=r3d_mesh_create(&m);CHECK(pilot);
    /* Steering sits in front of the pilot, behind the depth-writing glass. */
    for(int j=0;j<3;j++){t.v[j].p[2]=3.5f;t.v[j].rgb[0]=0;t.v[j].rgb[1]=1;}
    r3d_mesh *steering=r3d_mesh_create(&m);CHECK(steering);
    for(int j=0;j<3;j++){t.v[j].p[2]=3;t.v[j].rgb[0]=t.v[j].rgb[1]=0;t.v[j].rgb[2]=1;t.v[j].alpha=.5f;}
    t.blend_enabled=1;t.state.blend=0x44;
    r3d_mesh *glass=r3d_mesh_create(&m);CHECK(glass);
    t.blend_enabled=0;r3d_mesh *steel=r3d_mesh_create(&m);CHECK(steel);
    r3d_view v={0};v.rot[0]=v.rot[4]=v.rot[8]=v.r[0]=v.u[1]=v.f[2]=1;v.focal=320;v.focal_y=224;
    clear();
    if(getenv("GT4_DRIVER_GLASS_NEGATIVE")){r3d_draw(glass,&v);r3d_draw(pilot,&v);}
    else{r3d_draw(pilot,&v);r3d_draw(glass,&v);}
    r3d_end();int w,h;unsigned char *px=plat_sdl_read_pixels(&w,&h);CHECK(px);
    const unsigned char *centre=px+((h/2)*w+w/2)*4;
    CHECK(centre[0]>90 && centre[0]<180 && centre[2]>90);free(px);
    clear();r3d_draw(pilot,&v);
    if(getenv("GT4_STEERING_GLASS_NEGATIVE")){r3d_draw(glass,&v);r3d_draw(steering,&v);}
    else{r3d_draw(steering,&v);r3d_draw(glass,&v);}
    r3d_end();px=plat_sdl_read_pixels(&w,&h);CHECK(px);
    centre=px+((h/2)*w+w/2)*4;
    CHECK(centre[0]<30 && centre[1]>90 && centre[1]<180 && centre[2]>90);free(px);
    clear();r3d_draw(pilot,&v);r3d_draw(steering,&v);r3d_draw(steel,&v);r3d_end();
    px=plat_sdl_read_pixels(&w,&h);CHECK(px);centre=px+((h/2)*w+w/2)*4;
    CHECK(centre[0]<30 && centre[1]<30 && centre[2]>180);free(px);
    r3d_mesh_destroy(pilot);r3d_mesh_destroy(steering);r3d_mesh_destroy(glass);r3d_mesh_destroy(steel);
    puts("driver/window: blended depth-writing glass preserves cockpit; opaque shell occludes it");return 0;
}
static int finger_morph(void)
{
    gt4_triangle t={0};gt4_morph_triangle endpoint={0};
    t.kind=0;t.texture=-1;t.state.depth_write=t.state.depth_test=1;t.state.depth_func=3;
    const float xyz[3][3]={{-.5f,-.5f,4},{.5f,-.5f,4},{0,.5f,4}};
    for(int j=0;j<3;j++){memcpy(t.v[j].p,xyz[j],12);t.v[j].p[0]+=10;t.v[j].normal[2]=-1;t.v[j].rgb[0]=t.v[j].alpha=1;
        memcpy(endpoint.v[j].p,xyz[j],12);endpoint.v[j].normal[2]=-1;}
    gt4_model m={0};m.triangles=&t;m.count=m.capacity=1;m.morph=&endpoint;m.morph_capacity=1;m.grasp_scale=.9999f;
    r3d_mesh *hand=r3d_mesh_create(&m);CHECK(hand);m.morph=NULL;r3d_mesh *fixed=r3d_mesh_create_shared(&m,hand);CHECK(fixed);
    r3d_view v={0};v.rot[0]=v.rot[4]=v.rot[8]=v.r[0]=v.u[1]=v.f[2]=1;v.focal=320;v.focal_y=224;
    clear();r3d_draw(hand,&v);r3d_end();CHECK(red_pixels()==0);
    v.grasp=getenv("OPENGT4_PROBE_MORPH_NEGATIVE")?0:1; /* deliberate negative control */
    clear();r3d_draw(hand,&v);r3d_end();unsigned closed=red_pixels();CHECK(closed>500 && r3d_frame_stats().culled_groups==0);
    /* A different actor can share this immutable mesh and use another ratio.
       Its shifted red-pixel centroid proves the interpolation is continuous. */
    v.grasp=.9f;clear();r3d_draw(hand,&v);r3d_end();int w,h;unsigned char *px=plat_sdl_read_pixels(&w,&h);CHECK(px);
    unsigned count=0;double sum=0;for(int i=0;i<w*h;i++)if(px[i*4]>180 && px[i*4+1]<30 && px[i*4+2]<30){count++;sum+=i%w;}
    free(px);CHECK(count>500 && sum/count>w*.5+50);
    v.grasp=1;clear();r3d_draw(hand,&v);r3d_draw(fixed,&v);r3d_end();CHECK(red_pixels()==closed);
    v.grasp=0;clear();r3d_draw(hand,&v);r3d_end();CHECK(red_pixels()==0 && glGetError()==GL_NO_ERROR);
    r3d_mesh_destroy(hand);r3d_mesh_destroy(fixed);puts("driver GPU: authored morph endpoints, continuous ratio, bounds, shared actors and static isolation passed");return 0;
}
/* Diagnostic only: the authored bind pose is not the car's solved driving
   posture. Rigid parts are in bone coordinates; invert the file's bind matrix
   to put them alongside the skin. No guessed seat/limb transforms. */
static int bind_view(const float m[16],r3d_view *v)
{
    float a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
    float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(fabsf(det)<1e-10f)return 0;
    float rot[9]={(e*i-f*h)/det,(c*h-b*i)/det,(b*f-c*e)/det,
                  (f*g-d*i)/det,(a*i-c*g)/det,(c*d-a*f)/det,
                  (d*h-e*g)/det,(b*g-a*h)/det,(a*e-b*d)/det};
    memcpy(v->rot,rot,sizeof rot);
    for(int k=0;k<3;k++)v->pos[k]=-(rot[k*3]*m[12]+rot[k*3+1]*m[13]+rot[k*3+2]*m[14]);
    return 1;
}
static int preview(const char *iso,const char *path,const char *png_path)
{
    const char *err=NULL;gt4_vol *vol=gt4_vol_open(iso,&err);CHECK(vol);
    size_t n;uint8_t *d=gt4_vol_read(vol,path,&n);gt4_vol_close(vol);CHECK(d);
    gt4_driver_rig rig;CHECK(gt4_driver_rig_read(d,n,&rig) && rig.skin_count==1);
    gt4_model *body=gt4_model_decode_skin_part(d,n,rig.skin_first,NULL);CHECK(body);
    r3d_mesh *mesh=r3d_mesh_create(body);CHECK(mesh);gt4_model_free(body);
    r3d_view v={0};v.rot[0]=v.rot[4]=v.rot[8]=1;v.focal=470;v.focal_y=470;
    v.cam[0]=1.25f;v.cam[1]=.65f;v.cam[2]=-1.45f;
    const float target[3]={0,.25f,0};float length=0;
    for(int k=0;k<3;k++){v.f[k]=target[k]-v.cam[k];length+=v.f[k]*v.f[k];}
    length=sqrtf(length);for(int k=0;k<3;k++)v.f[k]/=length;
    length=sqrtf(v.f[0]*v.f[0]+v.f[2]*v.f[2]);v.r[0]=v.f[2]/length;v.r[2]=-v.f[0]/length;
    v.u[0]=v.f[1]*v.r[2];v.u[1]=v.f[2]*v.r[0]-v.f[0]*v.r[2];v.u[2]=-v.f[1]*v.r[0];
    float palette[20][16]={0};for(int b=0;b<20;b++)for(int k=0;k<16;k++)palette[b][k]=k%5==0?1.f:0.f;
    v.skin_palette=palette;clear();r3d_draw(mesh,&v);
    for(int b=0;b<20;b++)for(int j=0;j<rig.bone[b].model_count;j++) {
        gt4_model *part=gt4_model_decode_skin_part(d,n,rig.bone[b].model_first+j,NULL);CHECK(part);
        r3d_mesh *rigid=r3d_mesh_create_shared(part,mesh);gt4_model_free(part);CHECK(rigid);
        r3d_view p=v;p.skin_palette=NULL;CHECK(bind_view(rig.bone[b].inverse_bind,&p));
        r3d_draw(rigid,&p);r3d_mesh_destroy(rigid);
    }
    r3d_end();int w=0,h=0;size_t len=0;unsigned char *px=plat_sdl_read_pixels(&w,&h);CHECK(px);
    void *png=tdefl_write_image_to_png_file_in_memory(px,w,h,4,&len);free(px);CHECK(png);
    FILE *out=fopen(png_path,"wb");CHECK(out);int ok=fwrite(png,1,len,out)==len;fclose(out);mz_free(png);
    r3d_mesh_destroy(mesh);free(d);CHECK(ok);fprintf(stderr,"authored driver bind diagnostic: %s\n",path);return 0;
}
int main(int argc,char **argv)
{
    CHECK(argc==1 || argc==4);plat_video_desc desc={640,448,0,0,0};
    CHECK(!plat_video_init(&desc));plat_video_set_view(640,448);SDL_HideWindow(SDL_GetWindowFromID(1));
    CHECK(r3d_available());
    CHECK(!specular_mask());CHECK(!finger_morph());CHECK(!windshield());
    gt4_triangle t={0};t.kind=0;t.texture=-1;t.state.depth_write=1;t.state.depth_test=1;t.state.depth_func=3;
    const float xyz[3][3]={{7.75f,-.25f,4},{8.25f,-.25f,4},{8,.25f,4}};
    gt4_skin_triangle skin={0};
    for(int j=0;j<3;j++) {
        memcpy(t.v[j].p,xyz[j],sizeof xyz[j]);t.v[j].normal[2]=-1;t.v[j].rgb[0]=t.v[j].alpha=1;
        const uint8_t slots[4]={0,5,10,19};
        for(int k=0;k<4;k++){skin.v[j].bone[k]=slots[k];skin.v[j].weight[k]=.25f;}
    }
    gt4_model m={0};m.count=m.capacity=1;m.triangles=&t;m.skin=&skin;m.skin_capacity=1;
    r3d_mesh *animated=r3d_mesh_create(&m);CHECK(animated);
    m.skin=NULL;r3d_mesh *fixed=r3d_mesh_create(&m);CHECK(fixed);
    float palette[20][16]={0};for(int b=0;b<20;b++)for(int k=0;k<16;k++)palette[b][k]=k%5==0?1.f:0.f;
    r3d_view v={0};v.rot[0]=v.rot[4]=v.rot[8]=v.r[0]=v.u[1]=v.f[2]=1;v.focal=320;v.focal_y=224;
    clear();r3d_draw(animated,&v);r3d_end();CHECK(red_pixels()==0 && r3d_frame_stats().culled_groups==1);
    /* The four influences straddle the camera: their average is exactly -8. */
    palette[0][12]=-6;palette[5][12]=-7;palette[10][12]=-9;palette[19][12]=-10;v.skin_palette=palette;
    clear();r3d_draw(animated,&v);r3d_end();unsigned visible=red_pixels();
    CHECK(visible>500 && r3d_frame_stats().triangles==1 && r3d_frame_stats().culled_groups==0);
    /* Skinning state cannot deform a static car/course VAO drawn next. */
    clear();r3d_draw(animated,&v);r3d_draw(fixed,&v);r3d_end();
    CHECK(red_pixels()==visible && r3d_frame_stats().triangles==1 && r3d_frame_stats().culled_groups==1);
    palette[0][12]=palette[5][12]=palette[10][12]=palette[19][12]=0;
    clear();r3d_draw(animated,&v);r3d_end();CHECK(red_pixels()==0 && r3d_frame_stats().culled_groups==1);
    CHECK(glGetError()==GL_NO_ERROR);
    /* Invalid public input must fail before any draw can index the palette. */
    m.skin=&skin;skin.v[0].bone[0]=20;CHECK(!r3d_mesh_create(&m));skin.v[0].bone[0]=0;
    skin.v[0].weight[0]=-1;CHECK(!r3d_mesh_create(&m));
    r3d_mesh_destroy(animated);r3d_mesh_destroy(fixed);
    if(argc==4)CHECK(!preview(argv[1],argv[2],argv[3]));
    plat_video_shutdown();
    printf("driver GPU: specular mask, four-influence palette, slot 19, moving bounds and static isolation passed (%u pixels)\n",visible);
    return 0;
}
