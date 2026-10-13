#include "showroom.h"
#include "showroom_math.h"
#include "model.h"
#include "gpb.h"
#include "car_model.h"
#include "mot1.h"
#include "physics/phys.h"
#include "render3d.h"
#include "video_gl.h"
#include "vol.h"
#include "platform/platform.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 448
typedef car_model car_asset;
typedef struct {const uint8_t *data;uint64_t serial;r3d_mesh *mesh;} service_mesh;
struct showroom {
    char *disc;SDL_Thread *worker;SDL_mutex *lock;SDL_cond *wake;
    int stop,choice;unsigned generation,completed,displayed;char request[96],code[96],wheel[96],wing[96];
    car_asset *pending,*active,*retired;
    tex1_image *environment;int environment_count;
    uint8_t pixels[W*H*4];float depth[W*H],destination_alpha[W*H];plat_texture *texture;
    float angle,fade;char caption[256];
    char placed[96];                     /* car whose entrance (move_car) last played */
    float car_z;                          /* the car's offset toward the camera (script tz, scene -z) */
    float spin;                           /* turntable speed, degrees per second */
    const showroom_face *face;            /* set while a normal-mode CarFace draws */
    plat_texture *face_page;              /* the page under a normal CarFace, this frame */
    int showing;                          /* active is the requested car and may be drawn */
    uint8_t previous[W*H*4];
    uint64_t frames,total_ns,max_ns;
    float focal;                          /* projection: pixels per unit at depth 1 */
    /* GPU renderer (default; OPENGT4_RENDERER=software selects the reference) */
    int gpu;const car_asset *gpu_for;r3d_mesh *gpu_car,*gpu_shadow,*gpu_hull;plat_texture *backdrops[2];
    const uint8_t *crew_for;uint64_t crew_serial;r3d_mesh *gpu_crew;
    service_mesh *service_meshes;int service_mesh_count;
    r3d_mesh *service_body,*service_wheel[4],*service_brake[4];
    /* The PS2 studio (docs/SHOWROOM_SCENE.md): bg.mdl's lit layers weighted
       by light.mot. Layers are rendered once (the room does not move). */
    gt4_model *studio[4];int studio_count;gt4_mot1 *light;
    uint8_t *layers[4];int layers_ready;uint8_t *studio_px;plat_texture *studio_tex;
    float light_time;int restart_light;
    /* The camera: eye, right/up/forward axes; yaw turns the car so its
       starting pose faces the camera the same way whichever camera is used. */
    float eye[3],cam_r[3],cam_u[3],cam_f[3],yaw;
    r3d_lights lit;                       /* light.mot's lights at light_time; count 0: CarFace lights */
    /* Soft floor shadow cast from light 0 (half resolution, as the PS2's). */
    float shadow_mask[(W/2)*(H/2)],shadow_blur[(W/2)*(H/2)];uint8_t shadow_px[(W/2)*(H/2)*4];
    plat_texture *shadow_tex;int shadow_on;
    /* Reflections: env.mdl's four panel models seen from the car, each as a
       sphere map (the convention of the environment lookup), summed per
       frame by the Alpha weights into studio_env. */
    gt4_model *panels[4];int panel_count;uint8_t *panel_maps[4];int panel_ready;tex1_image studio_env;
    const tex1_image *env_now;            /* this frame's environment map */
};
static void clear_service_car(showroom *s){
    r3d_mesh_destroy(s->service_body);s->service_body=NULL;
    for(int i=0;i<4;i++){
        r3d_mesh_destroy(s->service_wheel[i]);s->service_wheel[i]=NULL;
        r3d_mesh_destroy(s->service_brake[i]);s->service_brake[i]=NULL;
    }
}
/* The arcade CarFace camera: translation (0,.98,9), pitch -3 degrees. The
   studio replaces it with camera.mot's once that is loaded. */
static void carface_camera(showroom *s)
{
    float st=sinf(.05235988f),ct=cosf(.05235988f);
    const float eye[3]={0,.98f,9},r[3]={1,0,0},u[3]={0,ct,-st},f[3]={0,-st,-ct};
    memcpy(s->eye,eye,sizeof eye);memcpy(s->cam_r,r,sizeof r);memcpy(s->cam_u,u,sizeof u);memcpy(s->cam_f,f,sizeof f);
    s->yaw=0;s->focal=1010.48f;
}
/* camera.mot (docs/SHOWROOM_SCENE.md): a constant look-at (eye, target, up)
   and vertical fov. The scene projects with aspect 640/448 (0x001D5C00 ->
   0x0049AC00), so one focal length serves both axes of the 640x448 view. */
static int studio_camera(showroom *s,const gt4_mot1 *m)
{
    float look[9],fov=0;
    if(!gt4_mot1_camera(m,0,look,&fov) || fov<=0 || fov>=3)return 0;
    float f[3]={look[3]-look[0],look[4]-look[1],look[5]-look[2]},lf=sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
    if(lf<1e-6f)return 0;
    for(int k=0;k<3;k++)f[k]/=lf;
    /* right = forward x up, as the CarFace camera's axes are */
    float r[3]={f[1]*look[8]-f[2]*look[7],f[2]*look[6]-f[0]*look[8],f[0]*look[7]-f[1]*look[6]},lr=sqrtf(r[0]*r[0]+r[1]*r[1]+r[2]*r[2]);
    if(lr<1e-6f)return 0;
    for(int k=0;k<3;k++)r[k]/=lr;
    float u[3]={r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]};
    memcpy(s->eye,look,3*sizeof(float));memcpy(s->cam_r,r,sizeof r);memcpy(s->cam_u,u,sizeof u);memcpy(s->cam_f,f,sizeof f);
    s->yaw=atan2f(-f[0],-f[2]);s->focal=224/tanf(fov/2);
    return 1;
}
static void destroy_asset(car_asset *a){car_model_free(a);}
static int worker(void *ptr)
{
    showroom *s=ptr;const char *err=NULL;gt4_vol *v=gt4_vol_open(s->disc,&err);
    if(!v)return 0;
    car_model_tables *tables=car_model_tables_open(v);
    size_t package_size=0,env_size=0;
    uint8_t *package=gt4_vol_read(v,"/projects/arcade/US/CarRoot.gpb",&package_size);
    uint8_t *env=package?gpb_load(package,package_size,"image/arcade/car/env0.png",&env_size):NULL;
    tex1_image *environment=NULL;int env_count=env?tex1_decode(env,env_size,&environment):0;free(env);free(package);
    SDL_LockMutex(s->lock);s->environment=environment;s->environment_count=env_count;SDL_UnlockMutex(s->lock);
    {
        /* Studio room (4 lit layers, one model each) and its light motion. */
        size_t bg_size=0,light_size=0,camera_size=0;
        uint8_t *bg=gt4_vol_read(v,"/menu/select/bg.mdl",&bg_size),*light=gt4_vol_read(v,"/menu/select/light.mot",&light_size);
        uint8_t *camera_data=gt4_vol_read(v,"/menu/select/camera.mot",&camera_size);
        gt4_mot1 *camera=camera_data?gt4_mot1_open(camera_data,camera_size):NULL;free(camera_data);
        gt4_model *layers[4]={0};int count=0;
        for(int i=0;bg && i<4;i++){layers[i]=gt4_model_decode_part(bg,bg_size,0,i);if(!layers[i])break;count++;}
        gt4_mot1 *motion=light?gt4_mot1_open(light,light_size):NULL;
        free(bg);free(light);
        SDL_LockMutex(s->lock);
        if(count!=4 || !motion || !studio_camera(s,camera)){for(int i=0;i<4;i++)gt4_model_free(layers[i]);gt4_mot1_close(motion);count=0;motion=NULL;
            memset(layers,0,sizeof layers);carface_camera(s);
            fprintf(stderr,"showroom: studio assets unavailable, using the plain backdrop\n");}
        memcpy(s->studio,layers,sizeof layers);s->studio_count=count;s->light=motion;SDL_UnlockMutex(s->lock);
        gt4_mot1_close(camera);
        /* The studio's reflections: env.mdl, one light-panel model per layer. */
        size_t env_mdl_size=0;uint8_t *env_mdl=count==4?gt4_vol_read(v,"/menu/select/env.mdl",&env_mdl_size):NULL;
        gt4_model *panels[4]={0};int panel_count=0;
        for(int i=0;env_mdl && i<4;i++){panels[i]=gt4_model_decode_part(env_mdl,env_mdl_size,0,i);if(!panels[i])break;panel_count++;}
        free(env_mdl);
        if(panel_count!=4){for(int i=0;i<4;i++)gt4_model_free(panels[i]);memset(panels,0,sizeof panels);panel_count=0;}
        SDL_LockMutex(s->lock);memcpy(s->panels,panels,sizeof panels);s->panel_count=panel_count;SDL_UnlockMutex(s->lock);
    }
    unsigned handled=0;
    for(;;) {
        SDL_LockMutex(s->lock);
        while(!s->stop && s->generation==handled && !s->retired)SDL_CondWait(s->wake,s->lock);
        car_asset *old=s->retired;s->retired=NULL;
        if(s->stop){SDL_UnlockMutex(s->lock);destroy_asset(old);break;}
        unsigned gen=s->generation;int choice=s->choice;char code[96],wheel[96],wing[96];strcpy(code,s->request);strcpy(wheel,s->wheel);strcpy(wing,s->wing);
        SDL_UnlockMutex(s->lock);destroy_asset(old);if(gen==handled)continue;
        car_asset *next=car_model_load_parts(v,tables,code,choice,wheel,wing);
        SDL_LockMutex(s->lock);handled=gen;
        if(gen==s->generation){old=s->pending;s->pending=next;s->completed=gen;next=NULL;}else old=NULL;
        SDL_UnlockMutex(s->lock);destroy_asset(old);destroy_asset(next);
    }
    car_model_tables_close(tables);gt4_vol_close(v);return 0;
}
/* Perspective-correct native rasterizer with a depth buffer. It renders
   ordinary triangles and RGBA textures, without interpreting PS2 programs. */
typedef struct {float x,y,z,u,v,c[3],spec[3],env[3],alpha;} point;
static float edge(point a,point b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);}
static void raster(showroom *s,point a,point b,point c,const tex1_image *im,const gt4_triangle *triangle)
{
    int reflection_pass=triangle->texture==-2;const gt4_draw_state *state=&triangle->state;
    point *p[3]={&a,&b,&c};
    for(int i=0;i<3;i++){float q=1/p[i]->z;p[i]->z=q;p[i]->x=320+p[i]->x*q*s->focal;p[i]->y=224-p[i]->y*q*s->focal;p[i]->u*=q;p[i]->v*=q;for(int k=0;k<3;k++){p[i]->c[k]*=q;p[i]->spec[k]*=q;}p[i]->env[0]*=q;p[i]->env[1]*=q;p[i]->env[2]*=q;p[i]->alpha*=q;}
    float ar=edge(a,b,c.x,c.y);if(fabsf(ar)<0.001f || (state->cull==1 && ar<=0) || (state->cull==2 && ar>=0))return;
    /* Half-open edges prevent a transparent strip's shared edge blending twice. */
    if(ar<0){point tmp=b;b=c;c=tmp;ar=-ar;}
    int ab=(b.y>a.y)||(b.y==a.y && b.x<a.x);
    int bc=(c.y>b.y)||(c.y==b.y && c.x<b.x);
    int ca=(a.y>c.y)||(a.y==c.y && a.x<c.x);
    int x0=(int)fmaxf(0,floorf(fminf(a.x,fminf(b.x,c.x)))),x1=(int)fminf(W-1,ceilf(fmaxf(a.x,fmaxf(b.x,c.x))));
    int y0=(int)fmaxf(0,floorf(fminf(a.y,fminf(b.y,c.y)))),y1=(int)fminf(H-1,ceilf(fmaxf(a.y,fmaxf(b.y,c.y))));
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) {
        float ea=edge(b,c,x+.5f,y+.5f),eb=edge(c,a,x+.5f,y+.5f),ec=edge(a,b,x+.5f,y+.5f);
        if(ea<0 || eb<0 || ec<0 || (ea==0 && !bc) || (eb==0 && !ca) || (ec==0 && !ab))continue;
        float wa=ea/ar,wb=eb/ar,wc=ec/ar;float z=wa*a.z+wb*b.z+wc*c.z;int at=y*W+x;
        if(state->depth_test) {
            if(state->depth_func==0)continue;
            if(state->depth_func==2 && z+1e-7f<s->depth[at])continue;
            if(state->depth_func==3 && (state->depth_write?z<=s->depth[at]:z+z*z*.001f<s->depth[at]))continue;
        }float inv=1/z;uint8_t white[4]={255,255,255,255};const uint8_t *t=white;float texture_alpha=128;
        if(im) {
            float u=(wa*a.u+wb*b.u+wc*c.u)*inv,v=(wa*a.v+wb*b.v+wc*c.v)*inv;
            float fx=u*im->tex_w-.5f,fy=v*im->tex_h-.5f;int tx=(int)floorf(fx),ty=(int)floorf(fy);fx-=floorf(fx);fy-=floorf(fy);
            int nx=showroom_texel(tx+1,im->width,im->wrap_u,im->min_u,im->max_u);
            int ny=showroom_texel(ty+1,im->height,im->wrap_v,im->min_v,im->max_v);
            tx=showroom_texel(tx,im->width,im->wrap_u,im->min_u,im->max_u);
            ty=showroom_texel(ty,im->height,im->wrap_v,im->min_v,im->max_v);
            const uint8_t *q[4]={im->rgba+(ty*im->width+tx)*4,im->rgba+(ty*im->width+nx)*4,im->rgba+(ny*im->width+tx)*4,im->rgba+(ny*im->width+nx)*4};
            if(im->native_alpha){
                const uint8_t *a=im->native_alpha;
                texture_alpha=(a[ty*im->width+tx]*(1-fx)+a[ty*im->width+nx]*fx)*(1-fy)+(a[ny*im->width+tx]*(1-fx)+a[ny*im->width+nx]*fx)*fy;
            }
            for(int k=0;k<4;k++)white[k]=(uint8_t)((q[0][k]*(1-fx)+q[1][k]*fx)*(1-fy)+(q[2][k]*(1-fx)+q[3][k]*fx)*fy);t=white;
        }
        float vertex_alpha=(wa*a.alpha+wb*b.alpha+wc*c.alpha)*inv;
        float native_alpha=vertex_alpha;
        if(im && im->color_component) {
            native_alpha=texture_alpha/128;
            if(im->texture_function==0)native_alpha*=vertex_alpha;
            else if(im->texture_function==2)native_alpha+=vertex_alpha;
        }
        /* Quantize before DATE: interpolating opaque alpha must not randomly
           turn bit 7 off through a 127.99999 rounding error. */
        native_alpha=showroom_alpha_byte(native_alpha)/128.f;
        int write_color=1,write_alpha=1,write_depth=state->depth_write;
        if(state->alpha_test) {
            float a=native_alpha*128,ref=state->alpha_ref;int pass=0;
            switch(state->alpha_func){case 0:break;case 1:pass=1;break;case 2:pass=a<ref;break;case 3:pass=a<=ref;break;case 4:pass=a==ref;break;case 5:pass=a>=ref;break;case 6:pass=a>ref;break;case 7:pass=a!=ref;break;}
            if(!pass){
                if(state->alpha_fail==0)continue;
                write_color=state->alpha_fail!=2;
                write_alpha=state->alpha_fail==1;
                write_depth=state->depth_write && state->alpha_fail==2;
            }
        }
        if(state->destination_test && (s->destination_alpha[at]>=1)!=state->destination_bit)continue;
        uint8_t *dst=s->pixels+at*4;
        float env_sample[3]={0};const float *environment=NULL;
        if(reflection_pass && s->env_now) {
            const tex1_image *e=s->env_now;
            float nx=wa*a.env[0]+wb*b.env[0]+wc*c.env[0],ny=wa*a.env[1]+wb*b.env[1]+wc*c.env[1],nz=wa*a.env[2]+wb*b.env[2]+wc*c.env[2];
            float nl=sqrtf(nx*nx+ny*ny+nz*nz);if(nl<1e-8f)nl=1;nx/=nl;ny/=nl;nz/=nl;
            float ix=(x+.5f-320)/s->focal,iy=(224-y-.5f)/s->focal,iz=-1;
            float il=sqrtf(ix*ix+iy*iy+1);ix/=il;iy/=il;iz/=il;
            float dot=ix*nx+iy*ny+iz*nz,rx=ix-2*dot*nx,ry=iy-2*dot*ny,rz=iz-2*dot*nz+1;
            float den=2*sqrtf(rx*rx+ry*ry+rz*rz);
            float eu=den>1e-8f?.5f+rx/den:.5f,ev=den>1e-8f?.5f-ry/den:.5f;
            float ex=fmaxf(0,fminf(e->width-1,eu*e->width-.5f)),ey=fmaxf(0,fminf(e->height-1,ev*e->height-.5f));
            int x0=(int)ex,y0=(int)ey,x1=x0+1<e->width?x0+1:x0,y1=y0+1<e->height?y0+1:y0;
            float fx=ex-x0,fy=ey-y0;
            for(int k=0;k<3;k++)env_sample[k]=(e->rgba[(y0*e->width+x0)*4+k]*(1-fx)+e->rgba[(y0*e->width+x1)*4+k]*fx)*(1-fy)+(e->rgba[(y1*e->width+x0)*4+k]*(1-fx)+e->rgba[(y1*e->width+x1)*4+k]*fx)*fy;
            environment=env_sample;
        }
        for(int k=0;k<3 && write_color;k++) {
            float light=(wa*a.c[k]+wb*b.c[k]+wc*c.c[k])*inv;
            float spec=(wa*a.spec[k]+wb*b.spec[k]+wc*c.spec[k])*inv;
            float source=reflection_pass?(environment?environment[k]:0)*light:fminf(255,t[k]*light+255*spec);
            uint8_t next=showroom_blend_channel(source,dst[k],native_alpha,s->destination_alpha[at],state->blend,state->fixed_alpha,triangle->blend_enabled);
            uint8_t mask=(uint8_t)(state->color_mask>>(k*8));
            dst[k]=(next & ~mask)|(dst[k]&mask);
        }
        if(write_alpha){
            unsigned mask=state->color_mask>>24;
            unsigned old=(unsigned)(s->destination_alpha[at]*128),next=(unsigned)(native_alpha*128);
            s->destination_alpha[at]=((next&~mask)|(old&mask))/128.f;
        }
        dst[3]=255;if(write_depth)s->depth[at]=z;
    }
}
static point lerp(point a,point b,float t){point p;float *o=(float *)&p,*x=(float *)&a,*y=(float *)&b;for(size_t i=0;i<sizeof(point)/sizeof(float);i++)o[i]=x[i]+t*(y[i]-x[i]);return p;}
/* The car's contact shadow model's opacity: light.mot's bottomShadow in the
   studio (0.7 on this disc, our reading of the name), the arcade 0.55
   otherwise. */
static float contact_shadow(const showroom *s)
{
    float v;
    if(s->studio_count==4 && s->light && gt4_mot1_value(s->light,"bottomShadow",gt4_mot1_wrap(s->light,s->light_time),&v) && v>0)return fminf(1,v);
    return .55f;
}
static void render(showroom *s,const gt4_model *m,float rotation,int background)
{
    if(!m)return;float sn=sinf(rotation),cs=cosf(rotation);
    const float *e=s->eye,*cr=s->cam_r,*cu=s->cam_u,*cf=s->cam_f;
    for(int i=0;i<m->count;i++) {
        int pass=m->triangles[i].texture==-2;
        const gt4_triangle *t=&m->triangles[i];point p[5],clipped[5];
        float edge1[3],edge2[3],face[3];
        for(int k=0;k<3;k++){edge1[k]=t->v[1].p[k]-t->v[0].p[k];edge2[k]=t->v[2].p[k]-t->v[0].p[k];}
        face[0]=edge1[1]*edge2[2]-edge1[2]*edge2[1];face[1]=edge1[2]*edge2[0]-edge1[0]*edge2[2];face[2]=edge1[0]*edge2[1]-edge1[1]*edge2[0];
        for(int j=0;j<3;j++) {
            const gt4_vertex *v=&t->v[j];float x=v->p[0]*cs+v->p[2]*sn-e[0],y=v->p[1]-e[1],z=-v->p[0]*sn+v->p[2]*cs-e[2]-(background==2?0:s->car_z);
            p[j].x=x*cr[0]+y*cr[1]+z*cr[2];p[j].y=x*cu[0]+y*cu[1]+z*cu[2];p[j].z=x*cf[0]+y*cf[1]+z*cf[2];
            /* background: 1 contact shadow, 2 studio layer (unlit) */
            p[j].u=v->uv[0];p[j].v=v->uv[1];p[j].alpha=background==1?contact_shadow(s):v->alpha;
            float nx=v->normal[0],ny=v->normal[1],nz=v->normal[2];
            float len=sqrtf(nx*nx+ny*ny+nz*nz);
            if(len<1e-6f){nx=face[0];ny=face[1];nz=face[2];len=sqrtf(nx*nx+ny*ny+nz*nz);}
            if(len<1e-6f)len=1;nx/=len;ny/=len;nz/=len;
            float rx=nx*cs+nz*sn,rz=-nx*sn+nz*cs;
            float vx=-cf[0],vy=-cf[1],vz=-cf[2];
            p[j].env[0]=rx*cr[0]+ny*cr[1]+rz*cr[2];p[j].env[1]=rx*cu[0]+ny*cu[1]+rz*cu[2];p[j].env[2]=-(rx*cf[0]+ny*cf[1]+rz*cf[2]);
            if(s->lit.count) {
                /* the studio's light.mot lights, as the GPU path (render3d.c) */
                float amb[3]={0},dif[3]={0},spe[3]={0};
                for(int l=0;l<s->lit.count;l++) {
                    const float *d=s->lit.direction[l];float dl=sqrtf(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
                    for(int k=0;k<3;k++)amb[k]+=s->lit.ambient[l][k];
                    if(dl<1e-6f)continue;
                    float lx=d[0]/dl,ly=d[1]/dl,lz=d[2]/dl,dot=fmaxf(0,rx*lx+ny*ly+rz*lz);
                    float hx=lx+vx,hy=ly+vy,hz=lz+vz,hl=sqrtf(hx*hx+hy*hy+hz*hz);
                    float sp=dot>0 && hl>1e-6f?powf(fmaxf(0,(rx*hx+ny*hy+rz*hz)/hl),v->shine):0;
                    for(int k=0;k<3;k++){dif[k]+=s->lit.diffuse[l][k]*dot;spe[k]+=s->lit.specular[l][k]*sp*s->lit.specular_scale;}
                }
                for(int k=0;k<3;k++) {
                    int plain=pass || background || t->kind==0 || t->kind==5;
                    p[j].c[k]=plain?v->rgb[k]:v->ambient[k]*amb[k]+v->rgb[k]*dif[k];
                    p[j].spec[k]=plain?0:v->specular[k]*spe[k];
                    if(!plain && t->kind==4){p[j].c[k]=p[j].spec[k];p[j].spec[k]=0;}
                }
                continue;
            }
            /* Lights from the authored CarFace, in scene coordinates. */
            const float lights[3][3]={{.57735f,.57735f,.57735f},{0,.9759f,-.2182f},{-1,0,0}};
            const float diffuse[3]={1,60.f/255,216.f/255},specular[3]={239.f/255,108.f/255,198.f/255};
            float diffuse_sum=0,spec_sum=0;
            for(int l=0;l<3;l++) {
                float dot=rx*lights[l][0]+ny*lights[l][1]+rz*lights[l][2];
                diffuse_sum+=diffuse[l]*fmaxf(0,dot);
                float hx=lights[l][0]+vx,hy=lights[l][1]+vy,hz=lights[l][2]+vz;
                float hl=sqrtf(hx*hx+hy*hy+hz*hz);
                if(dot>0 && hl>1e-6f)spec_sum+=specular[l]*powf(fmaxf(0,(rx*hx+ny*hy+rz*hz)/hl),v->shine);
            }
            for(int k=0;k<3;k++) {
                p[j].c[k]=pass?v->rgb[k]:50.f/255*v->ambient[k]+v->rgb[k]*diffuse_sum;
                if(background || t->kind==0 || t->kind==5)p[j].c[k]=v->rgb[k];
                p[j].spec[k]=(background || t->kind==0 || t->kind==5)?0:v->specular[k]*spec_sum*.18f;
                if(!background && !pass && t->kind==4){p[j].c[k]=p[j].spec[k];p[j].spec[k]=0;}
            }
        }
        int count=0;for(int j=0;j<3;j++){point a=p[j],b=p[(j+1)%3];int ain=a.z>=.2f,bin=b.z>=.2f;if(ain)clipped[count++]=a;if(ain!=bin)clipped[count++]=lerp(a,b,(.2f-a.z)/(b.z-a.z));}
        const tex1_image *im=t->texture>=0 && t->texture<m->ntextures?&m->textures[t->texture]:NULL;
        for(int j=1;j+1<count;j++)raster(s,clipped[0],clipped[j],clipped[j+1],im,t);
    }
}
/* Studio layers: each of bg.mdl's four models, unlit, through the studio
   camera, rendered once into its own image (the room never moves). */
static void render_layers(showroom *s)
{
    for(int i=0;i<4;i++) {
        s->layers[i]=malloc((size_t)W*H*4);
        if(!s->layers[i]){s->layers_ready=-1;return;}   /* not retried */
        memset(s->pixels,0,sizeof s->pixels);
        for(int at=0;at<W*H;at++){s->pixels[at*4+3]=255;s->depth[at]=0;s->destination_alpha[at]=1;}
        render(s,s->studio[i],0,2);
        memcpy(s->layers[i],s->pixels,sizeof s->pixels);
    }
    s->layers_ready=1;
}
/* The studio at the light time: the layers averaged by their Alpha weights
   (the PS2 draws each nonzero layer over the others with weight / running
   sum, 0x001D5C00), dimmed while the weights sum below 1 (its 1 - sum pass;
   the intro starts black), times ColorScale (our reading; not traced). */
static const uint8_t *compose_studio(showroom *s)
{
    if(s->layers_ready!=1)return NULL;
    if(!s->studio_px && !(s->studio_px=malloc((size_t)W*H*4)))return NULL;
    float t=gt4_mot1_wrap(s->light,s->light_time),w[4],sum=0,scale=1;
    static const char *const names[4]={"Alpha0","Alpha1","Alpha2","Alpha3"};
    for(int i=0;i<4;i++){if(!gt4_mot1_value(s->light,names[i],t,&w[i]) || w[i]<0)w[i]=0;sum+=w[i];}
    gt4_mot1_value(s->light,"ColorScale",t,&scale);
    float k=(sum>1?1/sum:1)*scale;
    for(int i=0;i<4;i++)w[i]*=k;
    for(size_t at=0;at<(size_t)W*H*4;at+=4) {
        for(int c=0;c<3;c++) {
            float v=0;
            for(int i=0;i<4;i++)if(w[i]>0)v+=w[i]*s->layers[i][at+c];
            s->studio_px[at+c]=(uint8_t)(v>255?255:v);
        }
        s->studio_px[at+3]=255;
    }
    return s->studio_px;
}

/* The composed studio as a texture for this frame; NULL until available. */
static plat_texture *studio_texture(showroom *s)
{
    const uint8_t *px=compose_studio(s);
    if(!px)return NULL;
    if(s->studio_tex)plat_texture_update(s->studio_tex,px);else s->studio_tex=plat_texture_create(W,H,px);
    return s->studio_tex;
}

/* env.mdl's panels as sphere maps, rendered once. The PS2's reflection
   camera (0x0044EEA8/0x0044EF18) sits 0.8 above the car and turns the
   panels 180 degrees about y (it also tilts 15 degrees, not reproduced), so
   the panels authored at +z face the viewer's side. Each panel model is
   drawn that way, unlit, onto the six faces of a cube (90 degrees each:
   focal 224 over the central 448x448 of the view), then each sphere-map
   texel looks up its reflected direction, as seen by the studio camera, in
   that cube. The PS2 instead renders three 120-degree views and remaps
   them with a mesh (0x0044EB10, D_006A7D00). */
enum{ENV_SIZE=128};
/* CarRoot's CarFace::move_car(0, 1), run by set_car on every car change
   (arcade.adc): warp to tz 8.5, ry 180 (the turntable restarts), then
   tx = tz = 0 with posInterpolateSpeed 0.025; the pose eases
   pos += (target - pos) x speed each frame (0x00153BB8) and the owner
   negates z for the scene. */
#define STUDIO_START_TZ 8.5f
/* The car's turntable angle. The arcade CarFace pose starts at -2.42 rad
   (a three-quarter view); the studio starts nose-on to its camera
   (move_car's ry 180, as PCSX2 shows). */
/* A new car plays move_car's entrance (warp, turntable from the start);
   a new colour of the same car only fades in (set_col_select does not
   call move_car). */
static void enter_car(showroom *s)
{
    if(!s->active || !strcmp(s->placed,s->active->code))return;
    snprintf(s->placed,sizeof s->placed,"%s",s->active->code);
    s->car_z=STUDIO_START_TZ;s->angle=0;
}
static float car_rotation(const showroom *s)
{
    /* A normal CarFace (GT Mode) moves the same way (move_car); its camera
       looks down -z, the studio's down +z, so its nose-on start is half a
       turn further. */
    if(s->face && s->face->pose_valid)return s->face->car_angle;
    if(s->face)return s->studio_count==4?s->angle:s->angle+3.14159265f;
    return s->studio_count==4?s->angle:s->angle-2.42f+s->yaw;
}
static void render_panels(showroom *s)
{
    static const float ENV_EYE[3]={0,.8f,0};
    const float axes[6][2][3]={{{1,0,0},{0,1,0}},{{-1,0,0},{0,1,0}},{{0,0,1},{0,1,0}},{{0,0,-1},{0,1,0}},{{0,1,0},{0,0,-1}},{{0,-1,0},{0,0,1}}};
    float eye[3],r[3],u[3],f[3],focal=s->focal,yaw=s->yaw;
    memcpy(eye,s->eye,12);memcpy(r,s->cam_r,12);memcpy(u,s->cam_u,12);memcpy(f,s->cam_f,12);
    uint8_t *faces=malloc((size_t)6*448*448*4);
    for(int i=0;i<4;i++)if(!(s->panel_maps[i]=malloc((size_t)ENV_SIZE*ENV_SIZE*4))){free(faces);faces=NULL;break;}
    if(!faces){s->panel_ready=-1;return;}   /* not retried */
    for(int i=0;i<4;i++) {
        for(int face=0;face<6;face++) {
            const float *ff=axes[face][0],*uu=axes[face][1];
            memcpy(s->eye,ENV_EYE,12);memcpy(s->cam_f,ff,12);memcpy(s->cam_u,uu,12);
            s->cam_r[0]=ff[1]*uu[2]-ff[2]*uu[1];s->cam_r[1]=ff[2]*uu[0]-ff[0]*uu[2];s->cam_r[2]=ff[0]*uu[1]-ff[1]*uu[0];
            s->focal=224;
            memset(s->pixels,0,sizeof s->pixels);
            for(int at=0;at<W*H;at++){s->pixels[at*4+3]=255;s->depth[at]=0;s->destination_alpha[at]=1;}
            render(s,s->panels[i],3.14159265f,2);
            for(int y=0;y<448;y++)memcpy(faces+((size_t)face*448+y)*448*4,s->pixels+((size_t)y*W+96)*4,448*4);
        }
        for(int y=0;y<ENV_SIZE;y++)for(int x=0;x<ENV_SIZE;x++) {
            /* sphere-map texel -> view-space reflection -> world direction */
            float mx=((x+.5f)/ENV_SIZE-.5f)*2,my=-((y+.5f)/ENV_SIZE-.5f)*2,m2=mx*mx+my*my;
            if(m2>1){float k=1/sqrtf(m2);mx*=k;my*=k;m2=1;}
            float nz=sqrtf(1-m2),R[3]={2*nz*mx,2*nz*my,2*nz*nz-1};
            float D[3];for(int k=0;k<3;k++)D[k]=R[0]*r[k]+R[1]*u[k]-R[2]*f[k];
            int best=0;float big=-1;
            for(int face=0;face<6;face++){const float *ff=axes[face][0];float d=D[0]*ff[0]+D[1]*ff[1]+D[2]*ff[2];if(d>big){big=d;best=face;}}
            const float *ff=axes[best][0],*uu=axes[best][1];
            float rr[3]={ff[1]*uu[2]-ff[2]*uu[1],ff[2]*uu[0]-ff[0]*uu[2],ff[0]*uu[1]-ff[1]*uu[0]};
            float sx=(D[0]*rr[0]+D[1]*rr[1]+D[2]*rr[2])/big,sy=(D[0]*uu[0]+D[1]*uu[1]+D[2]*uu[2])/big;
            int px=(int)fminf(447,fmaxf(0,224+sx*224)),py=(int)fminf(447,fmaxf(0,224-sy*224));
            memcpy(s->panel_maps[i]+((size_t)y*ENV_SIZE+x)*4,faces+(((size_t)best*448+py)*448+px)*4,4);
        }
    }
    free(faces);
    memcpy(s->eye,eye,12);memcpy(s->cam_r,r,12);memcpy(s->cam_u,u,12);memcpy(s->cam_f,f,12);s->focal=focal;s->yaw=yaw;
    s->studio_env.width=s->studio_env.height=s->studio_env.tex_w=s->studio_env.tex_h=ENV_SIZE;
    s->studio_env.max_u=s->studio_env.max_v=ENV_SIZE-1;
    s->studio_env.rgba=malloc((size_t)ENV_SIZE*ENV_SIZE*4);
    s->panel_ready=s->studio_env.rgba?1:-1;
}
/* The reflections at time t: the panel maps summed with their raw Alpha
   weights (0x001D5B08 draws each nonzero layer with its weight, no
   normalisation). */
static const tex1_image *compose_env(showroom *s,float t)
{
    if(s->panel_ready!=1 || !s->light)return NULL;
    static const char *const names[4]={"Alpha0","Alpha1","Alpha2","Alpha3"};
    float w[4];
    for(int i=0;i<4;i++)if(!gt4_mot1_value(s->light,names[i],t,&w[i]) || w[i]<0)w[i]=0;
    uint8_t *o=s->studio_env.rgba;
    for(size_t at=0;at<(size_t)ENV_SIZE*ENV_SIZE*4;at+=4) {
        for(int c=0;c<3;c++){float v=0;for(int i=0;i<4;i++)v+=w[i]*s->panel_maps[i][at+c];o[at+c]=(uint8_t)(v>255?255:v);}
        o[at+3]=255;
    }
    return &s->studio_env;
}

/* light.mot's four lights at time t, in scene coordinates. Records 0x0C,
   0x0D, 0x0E, 0x0F read as diffuse, ambient, specular and position (our
   reading: the fill lights then have no ambient and the total ambient is
   light 0's 0.4; read as ambient first it summed to 0.9 and lit the wheel
   barrels and every shaded face flat). Each position is used as a
   direction from the car, like ModelFace's l0_pos. */
static void studio_lights(showroom *s,float t)
{
    memset(&s->lit,0,sizeof s->lit);
    if(!s->light)return;
    for(int l=0;l<4;l++) {
        float c[4]={0};
        if(gt4_mot1_light(s->light,0x0C,l,t,c)>=3)memcpy(s->lit.diffuse[l],c,12);
        if(gt4_mot1_light(s->light,0x0D,l,t,c)>=3)memcpy(s->lit.ambient[l],c,12);
        if(gt4_mot1_light(s->light,0x0E,l,t,c)>=3)memcpy(s->lit.specular[l],c,12);
        if(gt4_mot1_light(s->light,0x0F,l,t,c)==3)memcpy(s->lit.direction[l],c,12);
    }
    s->lit.count=4;
    s->lit.specular_scale=.18f;   /* the arcade CarFace path's calibration (PCSX2), not traced */
}

/* The studio's soft floor shadow: the car's silhouette cast from light 0's
   position onto the floor (y = 0), blurred and darkened by shadowBlur0 /
   shadowDarkness0 (0x001D6968 renders it at half resolution from six
   jittered light positions; one position and a box blur here, our
   approximation). */
/* Box average of radius r along one line (zero outside). */
static void box_line(const float *in,int in_step,float *out,int out_step,int n,int r)
{
    float acc=0;
    for(int i=0;i<r && i<n;i++)acc+=in[i*in_step];
    for(int i=0;i<n;i++) {
        if(i+r<n)acc+=in[(i+r)*in_step];
        if(i-r-1>=0)acc-=in[(i-r-1)*in_step];
        out[i*out_step]=acc/(2*r+1);
    }
}
static void studio_shadow(showroom *s,const gt4_model *car,float rotation,float t)
{
    enum{SW=W/2,SH=H/2};
    float L[4]={0},darkness=0,blur=0;
    s->shadow_on=0;
    if(!car || !s->light || gt4_mot1_light(s->light,0x0F,0,t,L)!=3 || L[1]<=.1f)return;
    if(!gt4_mot1_value(s->light,"shadowDarkness0",t,&darkness) || darkness<=0)return;
    gt4_mot1_value(s->light,"shadowBlur0",t,&blur);
    memset(s->shadow_mask,0,sizeof s->shadow_mask);
    float sn=sinf(rotation),cs=cosf(rotation),half=s->focal/2;
    for(int i=0;i<car->count;i++) {
        const gt4_triangle *tr=&car->triangles[i];float q[3][2];int ok=1;
        for(int j=0;j<3 && ok;j++) {
            const float *p=tr->v[j].p;float P[3]={p[0]*cs+p[2]*sn,p[1],-p[0]*sn+p[2]*cs-s->car_z};
            if(P[1]>=L[1]-.01f){ok=0;break;}
            float k=L[1]/(L[1]-P[1]),Q[3]={L[0]+(P[0]-L[0])*k-s->eye[0],-s->eye[1],L[2]+(P[2]-L[2])*k-s->eye[2]};
            float x=Q[0]*s->cam_r[0]+Q[1]*s->cam_r[1]+Q[2]*s->cam_r[2],y=Q[0]*s->cam_u[0]+Q[1]*s->cam_u[1]+Q[2]*s->cam_u[2];
            float z=Q[0]*s->cam_f[0]+Q[1]*s->cam_f[1]+Q[2]*s->cam_f[2];
            if(z<.2f){ok=0;break;}
            q[j][0]=SW/2+x/z*half;q[j][1]=SH/2-y/z*half;
        }
        if(!ok)continue;
        float area=(q[1][0]-q[0][0])*(q[2][1]-q[0][1])-(q[2][0]-q[0][0])*(q[1][1]-q[0][1]);
        if(fabsf(area)<1e-6f)continue;
        int x0=(int)fmaxf(0,floorf(fminf(q[0][0],fminf(q[1][0],q[2][0])))),x1=(int)fminf(SW-1,ceilf(fmaxf(q[0][0],fmaxf(q[1][0],q[2][0]))));
        int y0=(int)fmaxf(0,floorf(fminf(q[0][1],fminf(q[1][1],q[2][1])))),y1=(int)fminf(SH-1,ceilf(fmaxf(q[0][1],fmaxf(q[1][1],q[2][1]))));
        for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) {
            float px=x+.5f,py=y+.5f,w0=(q[1][0]-px)*(q[2][1]-py)-(q[2][0]-px)*(q[1][1]-py);
            float w1=(q[2][0]-px)*(q[0][1]-py)-(q[0][0]-px)*(q[2][1]-py),w2=(q[0][0]-px)*(q[1][1]-py)-(q[1][0]-px)*(q[0][1]-py);
            if(area>0?(w0>=0 && w1>=0 && w2>=0):(w0<=0 && w1<=0 && w2<=0))s->shadow_mask[y*SW+x]=1;
        }
    }
    /* two separable box passes of radius shadowBlur0 / 4 (half-res pixels;
       our scale: /2 washed the core out under a small car) */
    int r=(int)(blur*.25f+.5f);
    for(int pass=0;pass<2 && r>0;pass++) {
        for(int y=0;y<SH;y++)box_line(s->shadow_mask+y*SW,1,s->shadow_blur+y*SW,1,SW,r);
        for(int x=0;x<SW;x++)box_line(s->shadow_blur+x,SW,s->shadow_mask+x,SW,SH,r);
    }
    for(int i=0;i<SW*SH;i++){uint8_t *o=s->shadow_px+i*4;o[0]=o[1]=o[2]=0;o[3]=(uint8_t)(fminf(1,s->shadow_mask[i])*darkness*255);}
    s->shadow_on=1;
}

/* ---- GPU path (render3d.c): same scenes, drawn by OpenGL at the window's
   resolution. The software renderer above stays as the reference. ---- */

/* The studio backdrop as an image, from the same formula. */
static plat_texture *backdrop(int kind)
{
    uint8_t *px=malloc((size_t)W*H*4);if(!px)return NULL;
    for(int y=0;y<H;y++)for(int x=0;x<W;x++) {
        uint8_t *p=px+((size_t)y*W+x)*4;
        float horizon=expf(-powf((y-253)/65.f,2)),shade=111+60*horizon-22*(float)y/H;
        float dx=(x-320)/185.f,dy=(y-298)/22.f;
        if(kind==1)shade*=1-.32f*expf(-2*(dx*dx+dy*dy));
        p[0]=p[1]=p[2]=(uint8_t)shade;
        p[3]=255;
    }
    plat_texture *t=plat_texture_create(W,H,px);free(px);return t;
}

static void draw_gpu(showroom *s)
{
    if(s->gpu_for!=s->active) {
        clear_service_car(s);
        r3d_mesh_destroy(s->gpu_car);r3d_mesh_destroy(s->gpu_shadow);r3d_mesh_destroy(s->gpu_hull);s->gpu_car=s->gpu_shadow=s->gpu_hull=NULL;
        if(s->active){s->gpu_car=r3d_mesh_create(s->active->car);s->gpu_shadow=r3d_mesh_create(s->active->shadow);if(s->active->hull)s->gpu_hull=r3d_mesh_create(s->active->hull);}
        s->gpu_for=s->active;
    }
    const uint8_t tint[4]={255,255,255,255};
    const tex1_image *env=s->env_now;
    r3d_view car={{1,0,0,0,1,0,0,0,1}};car.focal=s->focal;
    plat_texture *room=studio_texture(s);
    {
        int kind=!s->active || !s->active->shadow;
        if(!room && !s->backdrops[kind])s->backdrops[kind]=backdrop(kind);
        plat_draw_quad(room?room:s->backdrops[kind],0,0,W,H,0,0,1,1,tint);
        if(s->shadow_on && s->active && s->showing) {
            if(s->shadow_tex)plat_texture_update(s->shadow_tex,s->shadow_px);else s->shadow_tex=plat_texture_create(W/2,H/2,s->shadow_px);
            if(s->shadow_tex)plat_draw_quad(s->shadow_tex,0,0,W,H,0,0,1,1,tint);
        }
        float rotation=car_rotation(s),sn=sinf(rotation),cs=cosf(rotation);
        float rot[9]={cs,0,sn, 0,1,0, -sn,0,cs};memcpy(car.rot,rot,sizeof rot);car.pos[2]=-s->car_z;
        memcpy(car.cam,s->eye,sizeof car.cam);memcpy(car.r,s->cam_r,sizeof car.r);memcpy(car.u,s->cam_u,sizeof car.u);memcpy(car.f,s->cam_f,sizeof car.f);
        r3d_begin(0,0,W,H,env);
    }
    if(s->active && s->showing) {
        if(s->gpu && !getenv("OPENGT4_SHADOW_CPU") && s->studio_count==4 && s->light) {
            float L[4]={0},darkness=0,blur=0,t=gt4_mot1_wrap(s->light,s->light_time);
            if(gt4_mot1_light(s->light,0x0F,0,t,L)==3 && gt4_mot1_value(s->light,"shadowDarkness0",t,&darkness)) {
                gt4_mot1_value(s->light,"shadowBlur0",t,&blur);
                r3d_floor_shadow(s->gpu_car,&car,L,darkness,blur);
            }
        }
        r3d_view shadow=car;shadow.shadow=1;shadow.shadow_alpha=contact_shadow(s);
        r3d_draw(s->gpu_shadow,&shadow);
        if(s->gpu_hull)r3d_draw(s->gpu_hull,&car);
        r3d_draw_lit(s->gpu_car,&car,s->lit.count?&s->lit:NULL);
    }
    r3d_end();
    /* The car appears out of the studio: its backdrop over it, fading away. */
    if(s->showing && s->fade<1) {
        int kind=!s->active || !s->active->shadow;
        float o=s->fade*s->fade*(3-2*s->fade);uint8_t veil[4]={255,255,255,(uint8_t)(255*(1-o))};
        plat_draw_quad(room?room:s->backdrops[kind],0,0,W,H,0,0,1,1,veil);
    }
}

/* A normal-mode CarFace: the car over the page, in the widget's rectangle,
   with its camera (translation, then pitch/yaw in degrees; vertical field
   view_angle) and lights. */
static unsigned crew_u32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static void draw_service_models(showroom *s,const r3d_view *car,const r3d_lights *lights){
 const showroom_face *f=s->face;
 /* 00232280 +0x138..0x168 omits the additional context below 0.02.
    Camera "fade" belongs to that context, not to the car body. */
 if(f->service_pass && f->service_additional_opacity<gt4_f32(0x3ca3d70a))return;
 if(s->service_mesh_count!=f->model_count){
  for(int i=0;i<s->service_mesh_count;i++)r3d_mesh_destroy(s->service_meshes[i].mesh);
  free(s->service_meshes);s->service_meshes=calloc((size_t)f->model_count,sizeof *s->service_meshes);
  s->service_mesh_count=s->service_meshes?f->model_count:0;
 }
 for(int i=0;i<s->service_mesh_count;i++){
  const gt4_service_model *model=&f->models[i];service_mesh *cache=&s->service_meshes[i];
  if(cache->data==model->data && cache->serial==model->serial)continue;
  r3d_mesh_destroy(cache->mesh);cache->mesh=NULL;cache->data=model->data;cache->serial=model->serial;
  unsigned binds=model->size>=48?(model->data[46]|(unsigned)model->data[47]<<8):0;
  gt4_model *decoded=binds?gt4_model_decode_skin_part(model->data,model->size,0,NULL):gt4_model_decode_part(model->data,model->size,-1,0);
  if(decoded){cache->mesh=r3d_mesh_create(decoded);gt4_model_free(decoded);}
 }
 for(int i=0;i<f->motion_count;i++){
  const gt4_service_motion *track=&f->motions[i];if(track->model<0 || track->model>=s->service_mesh_count)continue;
  const gt4_service_model *model=&f->models[track->model];r3d_mesh *mesh=s->service_meshes[track->model].mesh;
  if(!mesh || model->size<128)continue;
  unsigned count=model->data[46]|(unsigned)model->data[47]<<8;size_t table=crew_u32(model->data+72);
  if(count>20 || table>model->size || count>(model->size-table)/80)continue;
  const char *names[20]={0};const uint8_t *bind[20]={0};float pose[20][16],palette[20][16];int valid=1;
  for(unsigned n=0;n<count;n++){const uint8_t *b=model->data+table+n*80;unsigned slot=crew_u32(b+64),name=crew_u32(b+68);
   if(slot>=count || bind[slot] || name>=model->size || !memchr(model->data+name,0,model->size-name)){valid=0;break;}
   names[slot]=(const char *)model->data+name;bind[slot]=b;
  }
  /* 0042A660 +0x70..0xb8 -> 004548A8: rigid draw targets use
     MDLS+58 name/index records (8 bytes), not inverse-bind records. */
  if(!count){unsigned named=model->data[42]|(unsigned)model->data[43]<<8;size_t at=crew_u32(model->data+88);
   if(at>model->size || named>(model->size-at)/8)continue;
   for(unsigned n=0;n<named;n++){const uint8_t *b=model->data+at+n*8;unsigned name=crew_u32(b);
    if((crew_u32(b+4)&0xffff)!=0 || name>=model->size || !memchr(model->data+name,0,model->size-name))continue;
    names[0]=(const char *)model->data+name;break;
   }
  }
  float sample_time=gt4_service_sample(track->time,track->motion!=NULL,gt4_mot1_end(track->motion),gt4_mot1_loop(track->motion),track->loop);
  if(!valid || gt4_mot1_frames(track->motion,sample_time,names,count?(int)count:1,pose,gt4_sincosf)!=(count?(int)count:1))continue;
  /* 00156BF0..56E74: alignment, yaw and uniform additionalModelScale.
     Every track draws separately, even when two tracks share a model. */
  if(track->alignment && !f->service_geometry_valid)continue;
  gt4_service_placement placement=gt4_service_place(f->service_geometry,track->alignment,
                                                  track->yaw,f->additional_scale,track->hidden,car->mirror);
  if(!placement.draw)continue;
  const float *offset=placement.offset;float sn,cs;gt4_sincosf(placement.yaw*gt4_f32(0x3c8efa34),&sn,&cs);
  float scale=placement.scale[0];float rotation[9]={cs*scale,0,sn*scale,0,placement.scale[1],0,-sn*scale,0,cs*scale};
  /* 00156AA0 composes placement and 0042A828 applies the Mot1 root.
     VU matrices are columns; r3d_view.rot is rows (GL_TRUE upload). */
  r3d_view view=*car;
  for(int row=0;row<3;row++){
   view.pos[row]=car->pos[row]+car->rot[row*3]*offset[0]+car->rot[row*3+1]*offset[1]+car->rot[row*3+2]*offset[2];
   for(int col=0;col<3;col++){float v=0;for(int k=0;k<3;k++)v+=car->rot[row*3+k]*rotation[k*3+col];view.rot[row*3+col]=v;}
  }
  for(int n=0;n<20;n++)for(int k=0;k<16;k++)palette[n][k]=k%5==0?1.f:0.f;
  for(unsigned n=0;n<count;n++)for(int col=0;col<4;col++)for(int row=0;row<4;row++){
   float v=0;for(int k=0;k<4;k++){float inv;memcpy(&inv,bind[n]+(col*4+k)*4,4);v+=pose[n][k*4+row]*inv;}
   if(!isfinite(v))valid=0;palette[n][col*4+row]=v;
  }
  if(valid){
   if(count)view.skin_palette=palette;
   else{r3d_view base=view;for(int row=0;row<3;row++){
    view.pos[row]=base.pos[row];for(int k=0;k<3;k++)view.pos[row]+=base.rot[row*3+k]*pose[0][12+k];
    for(int col=0;col<3;col++){float v=0;for(int k=0;k<3;k++)v+=base.rot[row*3+k]*pose[0][col*4+k];view.rot[row*3+col]=v;}
   }}
   r3d_draw_lit(mesh,&view,lights);
  }
 }
}
static void draw_crew(showroom *s,const r3d_view *car,const r3d_lights *lights){
    const showroom_face *f=s->face;
    if(f->model_count){draw_service_models(s,car,lights);return;}
    if(s->crew_for!=f->crew_data || s->crew_serial!=f->crew_serial){
        r3d_mesh_destroy(s->gpu_crew);s->gpu_crew=NULL;s->crew_for=f->crew_data;s->crew_serial=f->crew_serial;
        if(f->crew_data && f->crew_size>=128){
            gt4_model *model=gt4_model_decode_skin_part(f->crew_data,f->crew_size,0,NULL);
            if(model){s->gpu_crew=r3d_mesh_create(model);gt4_model_free(model);}
        }
    }
    if(!s->gpu_crew || !f->crew_motion)return;
    /* MDLS bind records: model+0x2e count, +0x48 table, records 0x50;
       0042A660 +0x124..0x140 stores the frame in its bind slot. The shader
       palette multiplies by the inverse bind, as the native driver path. */
    const uint8_t *data=f->crew_data;unsigned count=data[46]|(unsigned)data[47]<<8;
    size_t table=crew_u32(data+72);if(!count || count>20 || table>f->crew_size || count>(f->crew_size-table)/80)return;
    const char *names[20]={0};const uint8_t *bind[20]={0};float pose[20][16],palette[20][16];
    for(unsigned i=0;i<count;i++){
        const uint8_t *b=data+table+i*80;unsigned slot=crew_u32(b+64),name=crew_u32(b+68);
        if(slot>=count || bind[slot] || name>=f->crew_size || !memchr(data+name,0,f->crew_size-name))return;
        names[slot]=(const char *)data+name;bind[slot]=b;
    }
    float sample_time=gt4_service_sample(f->crew_time,1,gt4_mot1_end(f->crew_motion),gt4_mot1_loop(f->crew_motion),f->crew_loop);
    if(gt4_mot1_frames(f->crew_motion,sample_time,names,(int)count,pose,gt4_sincosf)!=(int)count)return;
    for(int n=0;n<20;n++)for(int k=0;k<16;k++)palette[n][k]=k%5==0?1.f:0.f;
    for(unsigned n=0;n<count;n++)for(int c=0;c<4;c++)for(int r=0;r<4;r++){
        float v=0;for(int k=0;k<4;k++){float inv;memcpy(&inv,bind[n]+(c*4+k)*4,4);v+=pose[n][k*4+r]*inv;}
        if(!isfinite(v))return;palette[n][c*4+r]=v;
    }
    r3d_view crew=*car;crew.skin_palette=palette;r3d_draw_lit(s->gpu_crew,&crew,lights);
}
static void draw_service_car(showroom *s,const r3d_view *car,const r3d_lights *lights){
    const car_asset *a=s->active;float lift=s->face->service_lift;
    if(!(lift>0) || a->body_count<=0){
        if(s->gpu_hull)r3d_draw(s->gpu_hull,car);
        r3d_draw_lit(s->gpu_car,car,lights);return;
    }
    /* Prepare only on the first lifted frame; static pages keep their single
       combined mesh. Subsets share body textures and remain resident. */
    if(!s->service_body){
        gt4_model body=*a->car;body.count=0;body.skin=NULL;body.morph=NULL;
        body.triangles=malloc((size_t)a->body_count*sizeof *body.triangles);
        if(body.triangles){
            for(int i=0;i<a->body_count;i++){
                const gt4_triangle *tr=&a->car->triangles[i];
                if(tr->attachment<1 || tr->attachment>4)body.triangles[body.count++]=*tr;
            }
            s->service_body=r3d_mesh_create_shared(&body,s->gpu_car);free(body.triangles);
        }
        for(int i=0;i<4;i++){
            if(a->wheel[i] && !s->service_wheel[i])s->service_wheel[i]=r3d_mesh_create(a->wheel[i]);
            if(a->brake[i] && !s->service_brake[i]){
                gt4_model brake=*a->brake[i];brake.textures=a->car->textures;brake.ntextures=a->car->ntextures;
                s->service_brake[i]=r3d_mesh_create_shared(&brake,s->gpu_car);
            }
        }
    }
    if(!s->service_body){r3d_draw_lit(s->gpu_car,car,lights);return;}
    float drop_limit[4],body_lift,wheel_lift[4];
    for(int i=0;i<4;i++)drop_limit[i]=s->face->service_geometry[8+(i^1)]; /* 00364B30, native wheel order */
    gt4_service_lift(lift,drop_limit,&body_lift,wheel_lift);
    r3d_view body=*car;
    for(int k=0;k<3;k++)body.pos[k]+=car->rot[k*3+1]*body_lift;
    if(s->gpu_hull)r3d_draw(s->gpu_hull,&body);
    r3d_draw_lit(s->service_body,&body,lights);
    for(int i=0;i<4;i++){
        r3d_view wheel=body;
        for(int k=0;k<3;k++)wheel.pos[k]+=car->rot[k*3]*a->hub[i][0]+
            car->rot[k*3+1]*(a->hub[i][1]+wheel_lift[i])+car->rot[k*3+2]*a->hub[i][2];
        r3d_draw_lit(s->service_wheel[i],&wheel,lights);
        r3d_draw_lit(s->service_brake[i],&wheel,lights);
    }
}
void showroom_service_channels(showroom_face *f,const gt4_mot1 *motion,float time)
{
    f->service_pass=1;f->service_opacity=1;f->service_additional_opacity=1;f->service_shadow=1;
    gt4_mot1_value(motion,"fade",time,&f->service_additional_opacity);
    gt4_mot1_value(motion,"lift",time,&f->service_lift);
    gt4_mot1_value(motion,"shadowOpacity",time,&f->service_shadow);
    f->service_additional_opacity=fminf(1,fmaxf(0,f->service_additional_opacity));
    f->service_shadow=fminf(1,fmaxf(0,f->service_shadow));
    /* 001571B4 -> 00428670 / 0021A878: authored normalized colours,
       then VU1 0x23/52/83 sums the specular term before texture modulation. */
    for(int l=0;l<f->lights.count;l++){
        float c[4];
        if(gt4_mot1_light(motion,0x0C,l,time,c)>=3)memcpy(f->lights.diffuse[l],c,12);
        if(gt4_mot1_light(motion,0x0D,l,time,c)>=3)memcpy(f->lights.ambient[l],c,12);
        if(gt4_mot1_light(motion,0x0E,l,time,c)>=3)memcpy(f->lights.specular[l],c,12);
        if(gt4_mot1_light(motion,0x0F,l,time,c)==3)memcpy(f->lights.direction[l],c,12);
    }
    f->lights.vertex_limit=255.f/128;f->lights.specular_scale=1;
}
static void draw_face_gpu(showroom *s)
{
    const showroom_face *f=s->face;
    if(s->gpu_for!=s->active) {
        clear_service_car(s);
        r3d_mesh_destroy(s->gpu_car);r3d_mesh_destroy(s->gpu_shadow);r3d_mesh_destroy(s->gpu_hull);s->gpu_car=s->gpu_shadow=s->gpu_hull=NULL;
        if(s->active){s->gpu_car=r3d_mesh_create(s->active->car);s->gpu_shadow=r3d_mesh_create(s->active->shadow);if(s->active->hull)s->gpu_hull=r3d_mesh_create(s->active->hull);}
        s->gpu_for=s->active;
    }
    if(!s->active || !s->showing)return;
    float pitch=f->rotate[0]*.01745329f,yaw=f->rotate[1]*.01745329f,st=sinf(pitch),ct=cosf(pitch),sy=sinf(yaw),cy=cosf(yaw);
    r3d_view car={{1,0,0,0,1,0,0,0,1}};
    car.focal=224/tanf((f->view_angle>1?f->view_angle:25)*.00872665f);
    if(f->service_pass){
        /* 002173AC -> 00107328 +0x30: the NTSC-U video mode (2)
           scales projection Y by 1/0x3F8C8B43. Captured 006184D0+14
           agrees in slots 6/7; this is not an adjustment from the image. */
        float correction[2];gt4_service_projection_scale(2,0,correction);
        car.focal_y=car.focal*correction[1];
    }
    float rotation=car_rotation(s),sn=sinf(rotation),cs=cosf(rotation);
    float rot[9]={cs,0,sn, 0,1,0, -sn,0,cs};memcpy(car.rot,rot,sizeof rot);
    if(s->studio_count==4 && !f->pose_valid) {
        /* PCSX2: the dealers' camera and motion are the showroom's, so the
           studio camera (camera.mot) frames the car here too */
        car.pos[2]=-s->car_z;car.focal=s->focal;
        memcpy(car.cam,s->eye,sizeof car.cam);memcpy(car.r,s->cam_r,sizeof car.r);
        memcpy(car.u,s->cam_u,sizeof car.u);memcpy(car.f,s->cam_f,sizeof car.f);
    } else {
        if(f->pose_valid)memcpy(car.pos,f->car_position,sizeof car.pos);
        else car.pos[2]=s->car_z;   /* move_car's tz, toward this camera */
        memcpy(car.cam,f->eye,sizeof car.cam);
        /* the CarFace camera looks down -z; pitch tilts it, yaw turns it */
        float r[3]={cy,0,-sy},u[3]={sy*st,ct,cy*st},fw[3]={-sy*ct,st,-cy*ct};
        memcpy(car.r,r,sizeof r);memcpy(car.u,u,sizeof u);memcpy(car.f,fw,sizeof fw);
    }
    if(f->look_valid){
        memcpy(car.cam,f->look,3*sizeof(float));
        float length=0;
        for(int k=0;k<3;k++){car.f[k]=f->look[k+3]-f->look[k];length+=car.f[k]*car.f[k];}
        length=sqrtf(length);if(length>0)for(int k=0;k<3;k++)car.f[k]/=length;
        car.r[0]=car.f[1]*f->look[8]-car.f[2]*f->look[7];
        car.r[1]=car.f[2]*f->look[6]-car.f[0]*f->look[8];
        car.r[2]=car.f[0]*f->look[7]-car.f[1]*f->look[6];
        length=sqrtf(car.r[0]*car.r[0]+car.r[1]*car.r[1]+car.r[2]*car.r[2]);
        if(length>0)for(int k=0;k<3;k++)car.r[k]/=length;
        car.u[0]=car.r[1]*car.f[2]-car.r[2]*car.f[1];
        car.u[1]=car.r[2]*car.f[0]-car.r[0]*car.f[2];
        car.u[2]=car.r[0]*car.f[1]-car.r[1]*car.f[0];
    }
    const tex1_image *env=s->environment_count>0?s->environment:NULL;
    const r3d_lights *lights=f->lights.count?&f->lights:NULL;
    if(f->service_pass){
        /* 00217058 +0xf0 / +0x578: reflected layer is added with FIX
           trunc(128 * 0x3E999999 * opacity), ALPHA 0x68. No screen-space
           floor/depth gradient. The regular layer copies the page first,
           then composes with ALPHA 0x64. Both targets stay resident. */
        car.env_color[0]=gt4_f32(0x3f8ccccc);car.env_color[1]=gt4_f32(0x3fa66666);
        /* 00217430..48 requests a separate additional-model context
           while the body's opacity is below 0x3F7D70A3. Its opacity
           comes from camera fade (00232388..A0), not the body fade. */
        int separate=f->service_opacity<gt4_f32(0x3f7d70a3);
        r3d_begin(f->x,f->y,f->w,f->h,env);
        if(r3d_begin_layer(0)){
            r3d_view mirror=car;mirror.mirror=1;
            /* 0021A878 +0xa8..0xdc flips light Y for the mirrored pass. */
            r3d_lights reflected=f->lights;for(int i=0;i<reflected.count;i++)reflected.direction[i][1]=-reflected.direction[i][1];
            const r3d_lights *mirror_lights=reflected.count?&reflected:NULL;
            draw_service_car(s,&mirror,mirror_lights);
            if(!separate)draw_crew(s,&mirror,mirror_lights);
            r3d_end_layer(f->service_opacity*gt4_f32(0x3e999999),1);
            if(separate && f->service_additional_opacity>=gt4_f32(0x3ca3d70a) && r3d_begin_layer(0)){
                draw_crew(s,&mirror,mirror_lights);
                r3d_end_layer(f->service_additional_opacity*gt4_f32(0x3e999999),1);
            }
        }
        for(int sample=0;sample<2;sample++){
          float jitter[2];gt4_service_jitter(2,sample,jitter);
          car.jitter[0]=2*jitter[0]/f->w;
          car.jitter[1]=-2*jitter[1]/(f->h*(448.f/480));
          r3d_begin(f->x,f->y,f->w,f->h,env);
          if(!r3d_begin_layer(1))break;
            /* 004524B8 +0x74..0xfc projects the authored contact mesh
               using light 0, then +0x11c..0x144 sets RGB 0 and camera
               shadowOpacity. This shadow remains on the floor during lift. */
            if(f->service_shadow>0){
                r3d_view shadow=car;shadow.shadow=1;shadow.shadow_alpha=f->service_shadow;
                float way[3]={0};
                if(lights && lights->count)for(int k=0;k<3;k++)for(int j=0;j<3;j++)
                    way[k]-=car.rot[j*3+k]*lights->direction[0][j];
                for(int k=0;k<3;k++)shadow.rot[k*3+1]=car.rot[k*3]*way[0]+car.rot[k*3+2]*way[2];
                r3d_draw(s->gpu_shadow,&shadow);
            }
            draw_service_car(s,&car,lights);
            if(!separate)draw_crew(s,&car,lights);
            r3d_end_layer_sample(f->service_opacity,0,sample,2);
        }
        car.jitter[0]=car.jitter[1]=0;
        if(separate && f->service_additional_opacity>=gt4_f32(0x3ca3d70a)){
            r3d_begin(f->x,f->y,f->w,f->h,env);
            if(r3d_begin_layer(1)){
              draw_crew(s,&car,lights);r3d_end_layer(f->service_additional_opacity,0);
            }
        }
        r3d_end();return;
    }
    /* The page under the car, to compose the car over it as the PS2 does
       (0x001D6378 draws the car offscreen and blends it with its opacity). */
    s->face_page=video_grab(s->face_page,f->x,f->y,f->w,f->h);
    /* The floor reflection: the car mirrored in y = 0, then the page drawn
       back over it from the floor line down, more opaque further down, so
       the reflection fades away from the car (as PCSX2 shows the dealers;
       the strength and fall-off are ours). */
    r3d_begin(f->x,f->y,f->w,f->h,env);
    r3d_view mirror=car;mirror.mirror=1;
    draw_service_car(s,&mirror,lights);
    /* 00156AA0 -> 00156820 +0x84..0xe0: the same additional models
       participate in the mirrored pass; hidden/flattened tracks skip it. */
    draw_crew(s,&mirror,lights);
    r3d_end();
    if(s->face_page) {
        /* the floor line under the car, in the rectangle's UI space */
        float w0[3]={-car.cam[0],-car.cam[1],car.pos[2]-car.cam[2]};
        float fy=w0[0]*car.u[0]+w0[1]*car.u[1]+w0[2]*car.u[2],fz=w0[0]*car.f[0]+w0[1]*car.f[1]+w0[2]*car.f[2];
        float floor=fz>.2f?f->y+f->h*(.5f-.5f*fy*car.focal/224/fz):f->y+f->h*.6f;
        floor=fminf(f->y+f->h,fmaxf(f->y,floor-f->h*.04f));
        float fall=f->h*.4f,bottom=f->y+f->h;
        float v_floor=1-(floor-f->y)/f->h,v_mid=1-(fminf(bottom,floor+fall)-f->y)/f->h;
        uint8_t grad[4][4]={{255,255,255,90},{255,255,255,90},{255,255,255,255},{255,255,255,255}};
        plat_draw_quad4(s->face_page,f->x,floor,f->w,fminf(bottom,floor+fall)-floor,0,v_floor,1,v_mid,grad);
        if(floor+fall<bottom){uint8_t solid[4]={255,255,255,255};plat_draw_quad(s->face_page,f->x,floor+fall,f->w,bottom-floor-fall,0,v_mid,1,0,solid);}
    }
    r3d_begin(f->x,f->y,f->w,f->h,env);
    r3d_view shadow=car;shadow.shadow=1;
    r3d_draw(s->gpu_shadow,&shadow);
    draw_service_car(s,&car,lights);
    draw_crew(s,&car,lights);
    r3d_end();
    /* The car appears with the showroom's fade: the page over it, fading. */
    float o=s->fade*s->fade*(3-2*s->fade);
    if(s->face_page && o<1){uint8_t veil[4]={255,255,255,(uint8_t)(255*(1-o))};plat_draw_quad(s->face_page,f->x,f->y,f->w,f->h,0,1,1,0,veil);}
}
int showroom_draw_face(showroom *s,const char *code,float dt,const showroom_face *face)
{
    if(!s || !face)return 0;
    s->face=face;int r=showroom_draw(s,code,dt);s->face=NULL;return r;
}

showroom *showroom_open(const char *disc)
{
    showroom *s=calloc(1,sizeof *s);if(!s)return NULL;carface_camera(s);
    {const char *r=getenv("OPENGT4_RENDERER");s->gpu=!(r && !strcmp(r,"software"));}s->disc=strdup(disc);s->lock=SDL_CreateMutex();s->wake=SDL_CreateCond();
    if(!s->disc || !s->lock || !s->wake){showroom_close(s);return NULL;}
    s->worker=SDL_CreateThread(worker,"3D asset loader",s);if(!s->worker){showroom_close(s);return NULL;}return s;
}
void showroom_close(showroom *s)
{
    if(!s)return;if(s->worker){SDL_LockMutex(s->lock);s->stop=1;SDL_CondSignal(s->wake);SDL_UnlockMutex(s->lock);SDL_WaitThread(s->worker,NULL);}
    if(s->frames)fprintf(stderr,"showroom render: frames=%llu mean=%.2f ms max=%.2f ms\n",(unsigned long long)s->frames,s->total_ns/1e6/s->frames,s->max_ns/1e6);
    destroy_asset(s->active);destroy_asset(s->pending);destroy_asset(s->retired);tex1_free(s->environment,s->environment_count);plat_texture_destroy(s->texture);
    r3d_mesh_destroy(s->gpu_car);r3d_mesh_destroy(s->gpu_shadow);r3d_mesh_destroy(s->gpu_hull);
    r3d_mesh_destroy(s->gpu_crew);
    clear_service_car(s);
    for(int i=0;i<s->service_mesh_count;i++)r3d_mesh_destroy(s->service_meshes[i].mesh);
    free(s->service_meshes);
    for(int i=0;i<2;i++)plat_texture_destroy(s->backdrops[i]);
    for(int i=0;i<4;i++){gt4_model_free(s->studio[i]);free(s->layers[i]);gt4_model_free(s->panels[i]);free(s->panel_maps[i]);}
    free(s->studio_env.rgba);
    gt4_mot1_close(s->light);free(s->studio_px);plat_texture_destroy(s->studio_tex);plat_texture_destroy(s->shadow_tex);plat_texture_destroy(s->face_page);
    if(s->wake)SDL_DestroyCond(s->wake);if(s->lock)SDL_DestroyMutex(s->lock);free(s->disc);free(s);
}
int showroom_prepare(showroom *s,const char *code)
{
    if(!s || !code)return -1;
    SDL_LockMutex(s->lock);
    if(strcmp(s->code,code)){snprintf(s->code,sizeof s->code,"%s",code);strcpy(s->request,s->code);s->choice=0;s->generation++;s->showing=0;SDL_CondSignal(s->wake);}
    if(s->completed==s->generation && s->pending && !s->retired){
        s->retired=s->active;s->active=s->pending;s->pending=NULL;s->displayed=s->generation;
        SDL_CondSignal(s->wake); /* allow the loader to release the retired asset */
    }
    int complete=s->completed==s->generation && s->displayed==s->generation;
    int ready=complete && s->active && !strcmp(s->active->code,code);
    int failed=s->completed==s->generation && !s->pending && !ready;
    SDL_UnlockMutex(s->lock);
    return ready?1:failed?-1:0;
}
int showroom_draw(showroom *s,const char *code,float dt)
{
    if(!s || !code)return 0;uint64_t started=plat_time_ns();SDL_LockMutex(s->lock);
    /* A new car hides the previous one at once (it may be another maker's)
       until its own model is loaded; a failed load leaves the studio empty. */
    if(strcmp(s->code,code)){snprintf(s->code,sizeof s->code,"%s",code);strcpy(s->request,s->code);s->choice=0;s->generation++;s->showing=0;SDL_CondSignal(s->wake);}
    if(s->completed==s->generation && s->pending && !s->retired){
        /* A new paint of the car on show replaces it in place (PCSX2: the car
           stays, only its colour changes); another car fades in. */
        int repaint=s->active && s->showing && !strcmp(s->active->code,s->pending->code);
        s->retired=s->active;s->active=s->pending;s->pending=NULL;s->displayed=s->generation;memcpy(s->previous,s->pixels,sizeof s->pixels);
        if(!repaint)s->fade=0;
        s->showing=!strcmp(s->active->code,s->code);enter_car(s);SDL_CondSignal(s->wake);
    }
    int complete=s->completed==s->generation && s->displayed==s->generation;
    if(!s->showing && complete && s->active && !strcmp(s->active->code,s->code)){s->showing=1;s->fade=0;enter_car(s);memcpy(s->previous,s->pixels,sizeof s->pixels);}
    int studio_loaded=s->studio_count==4 && s->light,panels_loaded=s->panel_count==4;
    SDL_UnlockMutex(s->lock);
    if(studio_loaded && !s->layers_ready)render_layers(s);
    if(studio_loaded && panels_loaded && !s->panel_ready)render_panels(s);
    /* The light timeline restarts when the showroom is entered (a maker is
       picked; showroom_hide), not per car (PCSX2): its intro (dark -> lit,
       ColorScale overshoot) plays then, and car changes keep the cycle. */
    if(s->showing && s->restart_light){s->light_time=0;s->restart_light=0;}
    else s->light_time+=fminf(dt,.05f);
    /* The car layer's opacity (pose +0x4C, scene +0x58: 0x001D6378 composes
       the car with it). The code read suggests 1/120 per frame (0x00153BB8 /
       0x00153B50: two seconds), but PCSX2 shows a quick, smooth fade: 0.4 s
       with an eased curve, as observed. */
    /* The turntable (pose mode 1, 0x00153BB8): each 1/60 s frame adds a
       force of 80 (0x0057AF30) to a damped integrator (0x0057AFF0, damping
       4): v += 80 dt - 4 v dt, angle += v dt, in degrees. It settles at
       20 degrees per second, starting from rest. */
    for(float left=fminf(dt,.05f);left>0;left-=1.f/60) {
        float step=fminf(left,1.f/60);
        s->spin+=(80-4*s->spin)*step;
        s->angle+=s->spin*step*.01745329f;
    }
    s->fade=fminf(1,s->fade+fminf(dt,.05f)*2.5f);
    float opacity=s->fade*s->fade*(3-2*s->fade);
    /* The zoom out: posInterpolateSpeed 0.025 per frame toward tz 0. */
    if(s->studio_count==4 || s->face)s->car_z*=powf(1-.025f,fminf(dt,.05f)*60);else s->car_z=0;
    {
        float t=s->light?gt4_mot1_wrap(s->light,s->light_time):0;
        if(s->face)s->lit=s->face->lights;
        else if(s->studio_count==4)studio_lights(s,t);else s->lit.count=0;
        /* the studio's reflections; the arcade CarFace's env0 without them */
        s->env_now=s->face?NULL:compose_env(s,t);
        if(s->env_now)r3d_environment_changed();
        else s->env_now=s->environment_count>0?s->environment:NULL;
        if(!s->gpu || getenv("OPENGT4_SHADOW_CPU"))studio_shadow(s,s->active && s->showing && s->studio_count==4 && !s->face?s->active->car:NULL,car_rotation(s),t);
        else s->shadow_on=0;
    }
    if(s->face){if(s->gpu && r3d_available())draw_face_gpu(s);}   /* no software path for faces yet */
    else if(s->gpu && r3d_available())draw_gpu(s);
    else {
    /* The studio room; the neutral formula only if its assets are missing. */
    const uint8_t *room=compose_studio(s);
    if(room)for(int at=0;at<W*H;at++){memcpy(s->pixels+at*4,room+at*4,4);s->depth[at]=0;s->destination_alpha[at]=1;}
    else for(int y=0;y<H;y++)for(int x=0;x<W;x++) {
        float horizon=expf(-powf((y-253)/65.f,2));
        float shade=111+60*horizon-22*(float)y/H;
        float dx=(x-320)/185.f,dy=(y-298)/22.f;
        if(!s->active || !s->active->shadow)shade*=1-.32f*expf(-2*(dx*dx+dy*dy));
        int at=y*W+x;for(int k=0;k<3;k++)s->pixels[at*4+k]=(uint8_t)shade;
        s->pixels[at*4+3]=255;s->depth[at]=0;s->destination_alpha[at]=1;
    }
    if(s->shadow_on)for(int y=0;y<H;y++)for(int x=0;x<W;x++) {
        float a=s->shadow_px[((y/2)*(W/2)+x/2)*4+3]/255.f;uint8_t *p=s->pixels+((size_t)y*W+x)*4;
        for(int k=0;k<3;k++)p[k]=(uint8_t)(p[k]*(1-a));
    }
    /* the car layer over the studio at the fade's opacity */
    if(s->active && s->showing && s->fade<1)memcpy(s->previous,s->pixels,sizeof s->pixels);
    if(s->active && s->showing){render(s,s->active->shadow,car_rotation(s),1);render(s,s->active->car,car_rotation(s),0);}
    if(s->active && s->showing && s->fade<1)for(int i=0;i<W*H*4;i++)s->pixels[i]=(uint8_t)(s->pixels[i]*opacity+s->previous[i]*(1-opacity));
    if(s->texture)plat_texture_update(s->texture,s->pixels);else s->texture=plat_texture_create(W,H,s->pixels);
    uint8_t tint[4]={255,255,255,255};plat_draw_quad(s->texture,0,0,W,H,0,0,1,1,tint);
    }
    if(s->active)snprintf(s->caption,sizeof s->caption,"%s  (%d/%d)",s->active->colors[s->active->choice].name,s->active->choice+1,s->active->count);
    uint64_t spent=plat_time_ns()-started;s->frames++;s->total_ns+=spent;if(spent>s->max_ns)s->max_ns=spent;
    return s->active && s->showing && !strcmp(s->active->code,code) && complete && s->fade>=1;
}
void showroom_hide(showroom *s){if(s){SDL_LockMutex(s->lock);s->showing=0;s->restart_light=1;s->placed[0]=0;SDL_UnlockMutex(s->lock);}}
void showroom_set_color(showroom *s,int index)
{if(!s || index<0)return;SDL_LockMutex(s->lock);if(s->choice!=index){s->choice=index;s->generation++;SDL_CondSignal(s->wake);}SDL_UnlockMutex(s->lock);}
void showroom_set_selection(showroom *s,const char *code,int index)
{
    if(!s || !code)return;SDL_LockMutex(s->lock);
    int changed=strcmp(s->code,code)!=0;
    int choice=index<0?(changed?0:s->choice):index;
    if(changed || s->choice!=choice){
        if(changed){snprintf(s->code,sizeof s->code,"%s",code);strcpy(s->request,s->code);s->showing=0;}
        s->choice=choice;s->generation++;SDL_CondSignal(s->wake);
    }
    SDL_UnlockMutex(s->lock);
}
void showroom_color(showroom *s,int delta){if(!s)return;SDL_LockMutex(s->lock);s->choice+=delta;s->generation++;SDL_CondSignal(s->wake);SDL_UnlockMutex(s->lock);}
const char *showroom_caption(showroom *s){return s?s->caption:"";}
int showroom_color_index(showroom *s)
{int c=0;if(s){SDL_LockMutex(s->lock);if(s->active)c=s->active->choice;SDL_UnlockMutex(s->lock);}return c;}
void showroom_set_angle(showroom *s,float angle){if(s)s->angle=angle;}

void showroom_set_wheel(showroom *s,const char *wheel){
    if(!s)return;if(!wheel)wheel="";
    SDL_LockMutex(s->lock);
    if(strcmp(s->wheel,wheel)){
        snprintf(s->wheel,sizeof s->wheel,"%s",wheel);
        s->generation++;s->showing=0;SDL_CondSignal(s->wake);
    }
    SDL_UnlockMutex(s->lock);
}

void showroom_set_wing(showroom *s,const char *wing){
    if(!s)return;if(!wing)wing="";SDL_LockMutex(s->lock);
    if(strcmp(s->wing,wing)){snprintf(s->wing,sizeof s->wing,"%s",wing);s->generation++;s->showing=0;SDL_CondSignal(s->wake);}
    SDL_UnlockMutex(s->lock);
}
