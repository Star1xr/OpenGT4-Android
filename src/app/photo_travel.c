#include "photo_travel.h"
#include "race_view.h"
#include "video_gl.h"
#include "save.h"
#include "pad_config.h"
#include "miniz.h"
#include "license_image.h"
#include "photo_pause.h"
#include "ps2_media.h"
#include "race_smoke.h"
#include "race_leaves.h"
#include "render3d.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct photo_travel {
    gt4_vol *volume;photo_pause *pause;menu *editor;
    uint64_t overlay_tick;uint32_t editor_buttons;
    race_view *view,*owned_view;menu *owner;plat_texture *image,*sample;int captured;
    float saved_eye[3],saved_rows[9],saved_projection[3],apertures[17];
    float samples[GT4_PHOTO_SAMPLES][4];
    int32_t shutters[19];int shutter_held,tables_ready,saved_car_visible;
    uint64_t tick;double remainder;unsigned serial;
    char saved[1200];int save_failed;
    gt4_film film;int film_active;
    race_smoke *film_smoke;race_leaves *film_leaves;
    gt4_photo_mode cameras[64];float camera_size[4],camera_distance;int camera_context;uint32_t camera_random;int zoom_frames;float drive_ev[21],drive_kelvin[11];
};

photo_travel *photo_travel_open(const char *disc)
{
    photo_travel *p=calloc(1,sizeof *p);if(!p)return NULL;
    p->view=race_view_open(disc);if(!p->view){free(p);return NULL;}p->volume=gt4_vol_open(disc,NULL);p->owned_view=p->view;p->tables_ready=gt4_photo_tables_load(disc,p->apertures,p->shutters) && gt4_photo_drive_tables_load(disc,p->drive_ev,p->drive_kelvin);gt4_photo_samples(p->samples,GT4_PHOTO_SAMPLES);return p;
}
void photo_travel_close(photo_travel *p)
{if(p){photo_travel_drive_end(p);photo_pause_close(p->pause);gt4_vol_close(p->volume);race_view_close(p->owned_view);plat_texture_destroy(p->image);plat_texture_destroy(p->sample);free(p);}}

const struct car_model_tag *photo_travel_car_model(const photo_travel *p)
{return p?race_view_car_model(p->view,0):NULL;}
static int native_photo_save(void *user);
int photo_travel_load(photo_travel *p,menu *m,const char *course,const char *car,int color,void (*pump)(void *),void *user)
{return photo_travel_load_from(p,m,m,course,car,color,pump,user);}
int photo_travel_load_from(photo_travel *p,menu *m,menu *source,const char *course,const char *car,int color,void (*pump)(void *),void *user)
{
    if(!p || !m || !source)return 0;photo_travel_drive_end(p);
    /* Vehicle body +468 (wheel), active sheet +100 (wing), 00390200:
       the source owns the selected garage body; cached PhotoMode does not. */
    {char wheel[32],wing[32];menu_photo_parts(source,wheel,wing);race_view_set_parts(p->view,wheel,wing);}   /* the garage car's fitted parts */
    if(!race_view_request_photo(p->view,course,car,color))return 0;
    int ready=0;
    while(!ready && !plat_should_quit()) {
        ready=race_view_poll(p->view,0);if(pump)pump(user);if(!ready)SDL_Delay(1);
    }
    if(ready!=1)return 0;
    menu_photo_set_selection(m,course,car,color);menu_photo_set_location(m,race_view_course(p->view));
    if(!menu_photo_state(m)->ready)return 0;
    {gt4_photo_mode *s=menu_photo_state(m);fprintf(stderr,"Photo Travel: car %.6f %.6f %.6f angle %.6f; camera %.6f %.6f %.6f yaw %.6f focal %.6f\n",s->car[0],s->car[1],s->car[2],s->car_angle,s->eye[0],s->eye[1],s->eye[2],s->yaw,s->view_angle);}
    p->owner=m;menu_set_photo_saver(m,native_photo_save,p);p->captured=0;p->saved[0]=0;p->save_failed=0;
    p->tick=plat_time_ns();p->remainder=0;
    for(int i=0;i<ui_root_count(menu_ui(m));i++)if(!strcmp(ui_root_name(menu_ui(m),i),"ShootRoot")){
        ui_widget *map=ui_widget_child(ui_root_widget(menu_ui(m),i),"Map");if(map)map->visible=1;
    }
    ui_set_photo_renderer(menu_ui(m),photo_travel_draw,p);return 1;
}

void photo_travel_shoot(photo_travel *p)
{
    if(!p || !p->owner)return;
    menu *m=p->owner;int root=menu_active_root(m);
    const char *name=ui_root_name(menu_ui(m),root);
    if(!name || strcmp(name,"ShootRoot"))return;
    ui_widget *panel=ui_widget_child(ui_root_widget(menu_ui(m),root),"Panel");
    ui_widget *shoot=ui_widget_child(panel,"Shoot");
    if(shoot){menu_native_focus(m,shoot);menu_send_input(m,MENU_ACCEPT);menu_send_key(m,65293,1);}   /* the press and its release */
}
const char *photo_travel_last_saved(const photo_travel *p){return p?p->saved:"";}
void photo_travel_feedback(photo_travel *p)
{
    if(!p || !p->owner)return;
    const char *page=ui_root_name(menu_ui(p->owner),menu_active_root(p->owner));
    gt4_photo_mode *s=menu_photo_state(p->owner);
    if(s->drive && page && !strcmp(page,"ShootRoot")){
        uint64_t now=plat_time_ns();double dt=p->overlay_tick?(now-p->overlay_tick)/1e9:0;p->overlay_tick=now;
        photo_pause_draw(p->pause,dt,video_projection_x()<1);
    }
    if(p->captured && page && !strcmp(page,"PreviewRoot") && (p->saved[0] || p->save_failed))
        ui_draw_caption(menu_ui(p->owner),menu_active_root(p->owner),p->saved[0]?"Photo saved (PNG). Esc: return":"PNG save failed. Check the save folder.");
}

static void pose(photo_travel *p,const gt4_photo_mode *s,float aspect)
{
    if(p->film_active)return; /* installed original PCAM/CARD snapshot */
    /* ScenePhoto 003844D0 +0..38: pose and FOV from PhotoMode.
       A: native trig/view adapter has not been compared with an EE state. */
    float d=3.141592502593994f/180,y=s->yaw*d,pt=s->pitch*d,r=s->roll*d;
    float cy=cosf(y),sy=sinf(y),cp=cosf(pt),sp=sinf(pt),cr=cosf(r),sr=sinf(r);
    float rows[9]={cy*cr+sy*sp*sr,cp*sr,sy*cr-cy*sp*sr,
                  -cy*sr+sy*sp*cr,cp*cr,-sy*sr-cy*sp*cr,
                  -sy*cp,sp,cy*cp};
    float focal=1/tanf(gt4_photo_fov(s->view_angle)*d/2);
    race_view_set_camera(p->view,s->eye,rows,focal/aspect,focal,.2f);
    if(s->drive){race_view_show_car(p->view,1);return;}
    float a=s->car_angle*d,c=cosf(a),sn=sinf(a);
    float rot[9]={c,0,-sn,0,1,0,sn,0,c};
    race_view_set_pose(p->view,s->car,rot);
    float wheels[4][3]={{s->wheel_angle*d,0,0},{s->wheel_angle*d,0,0},{0},{0}};
    race_view_set_wheels(p->view,wheels);
}

static int save_image(photo_travel *p)
{
    if(!p->image || !p->owner)return 0;
    gt4_photo_mode *s=menu_photo_state(p->owner);int w=s->image_width,h=s->image_height;
    if(w<=0 || h<=0 || w>16384 || h>16384)return 0;
    size_t bytes=(size_t)w*h*4;uint8_t *rgba=malloc(bytes);if(!rgba)return 0;
    /* P: lossless PC PNG at the captured viewport resolution, no widgets.
       Readback only when saving; live view and frozen preview stay on GPU. */
    video_flush2d();glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,video_texture_id(p->image));
    glPixelStorei(GL_PACK_ALIGNMENT,1);
#ifdef OPENGT4_GLES
    /* GLES has no glGetTexImage. The grabbed frame is RGBA8, so attach it
       to a one-shot framebuffer and read that; same rows, same flip below. */
    {
        GLint previous;GLuint fbo;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);glGenFramebuffers(1,&fbo);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,video_texture_id(p->image),0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE)
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        glBindFramebuffer(GL_FRAMEBUFFER,previous);glDeleteFramebuffers(1,&fbo);
    }
#else
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
#endif
    for(int y=0;y<h/2;y++)for(size_t x=0;x<(size_t)w*4;x++) {
        uint8_t *a=rgba+(size_t)y*w*4+x,*b=rgba+(size_t)(h-1-y)*w*4+x,t=*a;*a=*b;*b=t;
    }
    size_t length=0;void *png=tdefl_write_image_to_png_file_in_memory(rgba,w,h,4,&length);free(rgba);
    const char *dir=gt4_save_directory();char folder[1100];int ok=0;
    if(png && dir && *dir && snprintf(folder,sizeof folder,"%s/photos",dir)<(int)sizeof folder && gt4_save_mkdir(folder)) {
        time_t now=time(NULL);struct tm *tm=localtime(&now);char stamp[64];
        strftime(stamp,sizeof stamp,"%Y%m%d-%H%M%S",tm);
        do {
            snprintf(p->saved,sizeof p->saved,"%s/photo-%s-%03u.png",folder,stamp,++p->serial);
            FILE *exists=fopen(p->saved,"rb");if(!exists)break;fclose(exists);
        } while(p->serial<1000000);
        FILE *f=fopen(p->saved,"wb");if(f){ok=fwrite(png,1,length,f)==length;if(fclose(f))ok=0;}
        if(ok)printf("Photo Travel: saved %s (%dx%d)\n",p->saved,w,h);
    }
    mz_free(png);if(!ok)p->saved[0]=0;return ok;
}

static int native_photo_save(void *user){return save_image(user);}

static void input(photo_travel *p,gt4_photo_mode *s)
{
    uint64_t now=plat_time_ns();double elapsed=(now-p->tick)/1e9;p->tick=now;
    p->remainder+=fmin(elapsed,.25);
    const Uint8 *k=SDL_GetKeyboardState(NULL);
    plat_pad_state editor_pad={0};plat_input_poll(0,&editor_pad);
    if(s->drive && p->owner==p->editor){
        /* P: PC keys select authored tools. A: physical PhotoCamera pad
           configuration and repeat acceleration still need an EE oracle. */
        uint32_t buttons=(k[SDL_SCANCODE_EQUALS]?1u:0)|(k[SDL_SCANCODE_MINUS]?2u:0)|
            (k[SDL_SCANCODE_1]?4u:0)|(k[SDL_SCANCODE_2]?8u:0)|
            (k[SDL_SCANCODE_3]?16u:0)|(k[SDL_SCANCODE_4]?32u:0)|
            (k[SDL_SCANCODE_C]?64u:0)|(k[SDL_SCANCODE_R]?128u:0)|(k[SDL_SCANCODE_F]?256u:0)|
            (k[SDL_SCANCODE_PAGEUP]?512u:0)|(k[SDL_SCANCODE_PAGEDOWN]?1024u:0)|(k[SDL_SCANCODE_M]?2048u:0);
        /* P: native controller shortcuts alongside the PC keys. */
        if(editor_pad.buttons&(1u<<GT4_PAD_L1))buttons|=512;
        if(editor_pad.buttons&(1u<<GT4_PAD_R1))buttons|=1024;
        if(editor_pad.buttons&(1u<<GT4_PAD_SQUARE))buttons|=256;
        uint32_t press=buttons&~p->editor_buttons;p->editor_buttons=buttons;
        s->aperture=fmaxf(1,fminf(16,s->aperture+!!(press&1)-!!(press&2)));
        s->drive_ev+=!!(press&8)-!!(press&4);s->drive_ev=s->drive_ev<0?0:s->drive_ev>20?20:s->drive_ev;s->brightness=p->drive_ev[s->drive_ev]*8;
        s->drive_white+=!!(press&32)-!!(press&16);s->drive_white=s->drive_white<0?0:s->drive_white>10?10:s->drive_white;s->drive_kelvin=p->drive_kelvin[s->drive_white];s->white_balance=s->drive_white-6;
        if(press&64){const char *next=!strcmp(s->color_filter,"none")?"monochrome":!strcmp(s->color_filter,"monochrome")?"mature":"none";strcpy(s->color_filter,next);}
        if(press&128){s->stand=!s->stand;s->roll=s->stand?90:0;}
        if(press&256){s->manual_focus=0;s->focus_requested=1;}
        if(press&2048)s->manual_focus=!s->manual_focus;
        if(press&1536)photo_travel_drive_camera(p,(press&1024)?1:-1);
        /* P: Alt selects the existing 00371A58 roll tool. */
        strcpy(s->operate_mode,k[SDL_SCANCODE_LALT]?"roll":k[SDL_SCANCODE_LSHIFT]?"pan":k[SDL_SCANCODE_LCTRL]?"climb":"move");
        photo_pause_help(p->pause,k[SDL_SCANCODE_H]?2:k[SDL_SCANCODE_LSHIFT]?0:k[SDL_SCANCODE_LCTRL]?1:-1);
    }
    /* P: brackets select 0036F628's authored shutter indices; no invented times. */
    int shutter=(k[SDL_SCANCODE_RIGHTBRACKET]?1:0)-(k[SDL_SCANCODE_LEFTBRACKET]?1:0);
    if(s->drive && shutter && shutter!=p->shutter_held){s->shutter+=shutter;if(s->shutter<0)s->shutter=0;if(s->shutter>18)s->shutter=18;}
    p->shutter_held=shutter;
    /* P: WASD / Q,E map the PC keys to the original photo pad axes.
       Authoring controls run at 60 Hz independently of rendering. */
    float x=(float)(k[SDL_SCANCODE_D]-k[SDL_SCANCODE_A]+k[SDL_SCANCODE_RIGHT]-k[SDL_SCANCODE_LEFT]);
    float z=(float)(k[SDL_SCANCODE_S]-k[SDL_SCANCODE_W]+k[SDL_SCANCODE_DOWN]-k[SDL_SCANCODE_UP]);
    float turn=(float)(k[SDL_SCANCODE_E]-k[SDL_SCANCODE_Q]);
    /* the pad as 003FAFE8 reads it (the controller's mode 1): each axis
       its buttons' +-1 plus twice the stick's (controller +0xDC/+0xE0 the
       left stick, +0xE4 the right's, -0.5..0.5): x, z from the left
       stick, turn subtracts right X (003FAFE8 +1D4..1E4). D-pad
       contributes to X/Z while scripts retain focus when a tool is held.
       A: remaining internal button bits and complete pad oracle pending. */
    {
        plat_pad_state pad;
        if(plat_input_poll(0,&pad)){
            x=fmaxf(-1,fminf(1,x+pad.axes[0]+!!(pad.buttons&(1u<<GT4_PAD_RIGHT))-!!(pad.buttons&(1u<<GT4_PAD_LEFT))));
            z=fmaxf(-1,fminf(1,z+pad.axes[1]+!!(pad.buttons&(1u<<GT4_PAD_DOWN))-!!(pad.buttons&(1u<<GT4_PAD_UP))));
            turn=fmaxf(-1,fminf(1,turn-pad.axes[2]));
        }
    }
    /* photo_shoot.ad L258..346 and L350..442 own tool activation:
       Cross/shoulder press grabs it, release restores operate_mode=none.
       Merely focusing a tool must never move the camera. */
    while(p->remainder>=1./60) {
        if(s->drive && p->owner==p->editor){
            /* 00380100..184, 0036F6A0: zoom ramp 18 frames, factor
               1+abs(axis)*0.049999997. A: native free camera lacks the
               per-preset dynamic minimum derived by wall intersection. */
            int zoom=k[SDL_SCANCODE_X]-k[SDL_SCANCODE_Z]+!!(editor_pad.buttons&(1u<<GT4_PAD_L2))-!!(editor_pad.buttons&(1u<<GT4_PAD_R2));
            if(zoom)p->zoom_frames=p->zoom_frames<18?p->zoom_frames+1:18;else p->zoom_frames=0;
            if(zoom){float fov=gt4_photo_fov(s->view_angle),factor=1+.04999999701976776f*p->zoom_frames/18;
                fov=zoom>0?fov*factor:fov/factor;fov=fmaxf(2,fminf(83.19999694824219f,fov));s->view_angle=12/tanf(fov*.01745329252f*.5f);}
        }
        gt4_photo_step(s,race_view_course(p->view),x,z,turn);p->remainder-=1./60;
    }
}

int photo_travel_draw(void *user,const ui_widget *widget,float x,float y,float w,float h,float opacity)
{
    photo_travel *p=user;if(!p || !p->owner || !widget || opacity<=0 || w<=0 || h<=0)return 0;
    gt4_photo_mode *s=menu_photo_state(p->owner);if(!s->ready)return 0;
    if(!strcmp(widget->type,"PhotoMapWindow")) {
        input(p,s);
        /* A: project map positions in the current map viewport; native
           003704D0 scroll margins/region highlight remain pending. */
        ui_widget *map=(ui_widget *)widget;
        const char *icons[]={"car","camera"};
        const float *positions[]={s->car,s->eye};
        for(int i=0;i<2;i++) {
            ui_widget *icon=ui_widget_child(map,icons[i]);if(!icon)continue;
            float scale=map->w/s->location.map_length;
            icon->x=map->w*.5f+(positions[i][0]-s->eye[0])*scale-icon->w*.5f;
            icon->y=map->h*.5f+(positions[i][2]-s->eye[2])*scale-icon->h*.5f;
            ui_widget *body=ui_widget_child(icon,"body");if(body)body->rotation=i?s->yaw:s->car_angle;
        }
        return race_view_draw_photo_map(p->view,s,x,y,w,h);
    }
    uint8_t tint[4]={255,255,255,(uint8_t)(255*opacity)};
    if(!strcmp(widget->type,"PhotoViewFace")) {
        p->captured=0;p->saved[0]=0;p->save_failed=0;s->image_width=s->image_height=0;
        input(p,s);pose(p,s,w/h);if(!race_view_draw_photo(p->view,x,y,w,h))return 0;
        if(s->focus_requested || s->focus_distance<=0){
            /* 001952D0 centres focus at (0.5,0.5). A: native depth readback
               replaces GS/ZBUF decoding; one pixel, only on focus. */
            int vx,vy,vw,vh;float depth=0;video_viewport(x,y,w,h,&vx,&vy,&vw,&vh);
            video_flush2d();glReadPixels(vx+vw/2,vy+vh/2,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
            float n=.2f,f=20000;extern int gl_reversed_z;
            s->focus_distance=gl_reversed_z?n*f/(depth*(f-n)+n):2*n*f/(f+n-(depth*2-1)*(f-n));
            s->focus_requested=0;
        }
        /* Original CLUT + GS channel composition; no CPU image readback. */
        float color[6];uint8_t lut[256][3],mix[3][3];gt4_photo_color(s,color);
        gt4_license_image_lut(color,lut);gt4_license_image_mix(color[5],mix);
        p->sample=video_grab(p->sample,x,y,w,h);video_license_image(p->sample,x,y,w,h,lut,mix);return 1;
    }
    if(!strcmp(widget->type,"PhotoRenderFace")) {
        if(p->film_active){
            /* FILM dimensions / portrait specify the picture, independently
               of the native menu viewport. P: fit that aspect into the panel. */
            float aspect=(float)p->film.width/p->film.height;if(p->film.portrait)aspect=1/aspect;
            if(w/h>aspect){float width=roundf(h*aspect);x+=(w-width)*.5f;w=width;}
            else {float height=roundf(w/aspect);y+=(h-height)*.5f;h=height;}
        }
        if(!p->captured) {
            /* PreviewRoot L2873..2878 presents the rendered photo before
               its tooltip. P: capture the scene once, excluding UI. */
            pose(p,s,w/h);if(!race_view_draw_photo(p->view,x,y,w,h))return 0;
            if(!p->film_active && s->focus_distance<=0){
                int vx,vy,vw,vh;float depth=0;video_viewport(x,y,w,h,&vx,&vy,&vw,&vh);
                video_flush2d();glReadPixels(vx+vw/2,vy+vh/2,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
                float e[3],r[9],pr[3];race_view_get_camera(p->view,e,r,pr);float near_clip=pr[2],far_clip=20000;extern int gl_reversed_z;
                s->focus_distance=gl_reversed_z?near_clip*far_clip/(depth*(far_clip-near_clip)+near_clip):2*near_clip*far_clip/(far_clip+near_clip-(depth*2-1)*(far_clip-near_clip));
            }
            float eye[3],rows[9],pr[3];if(!race_view_get_camera(p->view,eye,rows,pr))return 0;
            int index=(int)s->aperture;index=index<1?1:index>16?16:index;
            float aperture=p->film_active?p->film.frame[p->film.frames-1].camera[13]:p->apertures[index];if(aperture<=0 || !p->tables_ready)return 0;
            int shutter=s->shutter<0?0:s->shutter>18?18:s->shutter;
            float exposure=p->film_active?p->film.exposure:s->drive?gt4_photo_exposure(gt4_photo_shutter_seconds(p->shutters[shutter])):0;
            if(s->drive)race_view_photo_begin(p->view);
            /* PhotoPause 003C4A48 +350..354: development has 32 samples;
               +338..34c: 640x480, 2x2 tiles, sensor aspect 4/3.
               P: native viewport replaces tile stitching. A: lens adapter. */
            int count=p->film_active?p->film.samples:GT4_PHOTO_SAMPLES;
            float film_samples[127][4];if(p->film_active)gt4_photo_samples(film_samples,count);
            for(int i=0;i<count;i++){
                float shift[2],jitter[2],e[3],motion_eye[3],motion_rows[9];
                memcpy(motion_eye,eye,sizeof eye);memcpy(motion_rows,rows,sizeof rows);
                if(s->drive)race_view_photo_sample(p->view,gt4_photo_time(exposure,i,count),motion_eye,motion_rows);
                gt4_photo_lens(s->view_angle,aperture,s->focus_distance,p->film_active?p->film.aspect:w/h,pr[1],p->film_active?film_samples[i]:p->samples[i],shift,jitter);
                for(int k=0;k<3;k++)e[k]=motion_eye[k]+motion_rows[k]*shift[0]+motion_rows[3+k]*shift[1];
                race_view_set_camera(p->view,e,motion_rows,pr[0],pr[1],pr[2]);race_view_photo_jitter(p->view,jitter);
                if(!race_view_draw_photo(p->view,x,y,w,h))goto sample_failed;
                p->sample=video_grab(p->sample,x,y,w,h);if(!p->sample)goto sample_failed;
                if(i){plat_draw_quad(p->image,x,y,w,h,0,1,1,0,tint);video_flush2d();}
                /* 00333A00 +a0..c0: running mean 1/(sample+1).
                   P: native linear alpha accumulation avoids CPU pixels. */
                plat_draw_quad_blend(p->sample,x,y,w,h,0,1,1,0,tint,1.f/(i+1));
                p->image=video_grab(p->image,x,y,w,h);if(!p->image)goto sample_failed;
            }
            if(s->drive)race_view_photo_end(p->view);
            race_view_photo_jitter(p->view,NULL);race_view_set_camera(p->view,eye,rows,pr[0],pr[1],pr[2]);
            float color[6];uint8_t lut[256][3],mix[3][3];if(p->film_active)gt4_media_film_color(&p->film,color);else gt4_photo_color(s,color);
            gt4_license_image_lut(color,lut);gt4_license_image_mix(color[5],mix);
            video_license_image(p->image,x,y,w,h,lut,mix);
            p->image=video_grab(p->image,x,y,w,h);if(!p->image)return 0;
            glBindTexture(GL_TEXTURE_2D,video_texture_id(p->image));
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&s->image_width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&s->image_height);
            p->captured=1;
            goto sample_done;
sample_failed:
            if(s->drive)race_view_photo_end(p->view);
            race_view_photo_jitter(p->view,NULL);race_view_set_camera(p->view,eye,rows,pr[0],pr[1],pr[2]);return 0;
sample_done:;
        }
        /* P: viewport-scaled inspection of the frozen PNG. The original
           imageControl.ad L55..99 still supplies zoom/offset values. */
        float scale=isfinite(s->image_scale)?fminf(16,fmaxf(1,s->image_scale)):1;
        plat_set_clip(x,y,w,h);
        plat_draw_quad(p->image,x+(w-w*scale)*.5f+s->image_x*h/960,
            y+(h-h*scale)*.5f+s->image_y*h/960,w*scale,h*scale,0,1,1,0,tint);
        plat_set_clip(0,0,640,480);
        if(s->save_requested){s->save_requested=0;p->save_failed=!save_image(p);if(p->save_failed)fprintf(stderr,"Photo Travel: PNG save failed\n");}
        return 1;
    }
    return 0;
}

void photo_travel_drive_setup(photo_travel *p,const float size[4],float distance){
 if(p && size){memcpy(p->camera_size,size,16);p->camera_distance=distance;p->camera_context=1;}
}
static float camera_random(photo_travel *p){
 /* 00579330 / 00578598; private seed is host time (P), never race RNG. */
 uint32_t v=p->camera_random*17u+17u;p->camera_random=v;v^=v<<16|v>>16;if(!v)return 0;
 int leading=-1;if(!(v&0x80000000u)){leading=0;uint32_t b=v;while(!(b&0x80000000u)){b<<=1;leading++;}leading--;}
 uint32_t exponent=125-leading,bits=(exponent<<23)|((v<<((0u-exponent)&31u))>>9);float f;memcpy(&f,&bits,4);return f;
}
static float camera_word(const uint8_t *p){float f;memcpy(&f,p,4);return f;}
static uint32_t camera_u32(const uint8_t *p){uint32_t u;memcpy(&u,p,4);return u;}
static void camera_add(photo_travel *p,gt4_photo_mode *s,const float eye[3]){
 if(s->drive_cameras>=64)return;gt4_photo_mode *c=&p->cameras[s->drive_cameras];*c=*s;memcpy(c->eye,eye,12);
 float v[3],distance=0;for(int k=0;k<3;k++){v[k]=s->car[k]-eye[k];distance+=v[k]*v[k];}distance=sqrtf(distance);if(distance<.1f)distance=.1f;
 /* 003FFC68: 2*atan(8/distance), clamp 2..83.2 degrees.
    003FFD10 +2C0..390 applies car aiming and defaults. A: host trig,
    aim/wall restriction and selected point partition have no EE proof. */
 float fov=fmaxf(2,fminf(83.19999694824219f,atanf(8/distance)*114.591552734375f));
 c->view_angle=12/tanf(fov*.01745329252f*.5f);c->yaw=atan2f(v[0],-v[2])*57.2957763671875f;
 c->pitch=-atan2f(v[1],sqrtf(v[0]*v[0]+v[2]*v[2]))*57.2957763671875f;c->roll=0;c->stand=0;
 c->focus_distance=distance;c->aperture=6;c->shutter=3;c->brightness=0;c->white_balance=0;c->drive_white=6;c->drive_ev=10;c->drive_kelvin=p->drive_kelvin[6];c->drive_camera=s->drive_cameras++;
}
static void camera_bank(photo_travel *p,gt4_photo_mode *s){
 p->camera_random=(uint32_t)plat_time_ns();s->drive_cameras=1;p->cameras[0]=*s;
 const gt4_course *course=race_view_course(p->view);if(!p->camera_context || !course)return;
 /* 003FFD10: incoming camera, active REP4 points (up to 63), then
    randomized car-relative cameras until the original capacity 64.
    A: native road height replaces original triangle/wall queries;
    the course point partition and collision rejection still need EE proof. */
 uint32_t base=course->size>=0xb4?camera_u32(course->data+0xb0):0;
 if(base && base<course->size && course->size-base>=0x34){
  const uint8_t *rep=course->data+base;uint32_t count=camera_u32(rep+0x2c),at=camera_u32(rep+0x30);
  if(at<=course->size-base && count<=(course->size-base-at)/48)
   for(uint32_t k=0;k<count && s->drive_cameras<64;k++){
    const uint8_t *point=rep+at+k*48;
    if(camera_word(point+0x10)<=p->camera_distance && p->camera_distance<camera_word(point+0x14)){
     float eye[3];memcpy(eye,point+4,12);camera_add(p,s,eye);
    }
   }
 }
 float rot[9],pos[3];if(!race_view_photo_pose(p->view,pos,rot))return;
 float radius=fmaxf(sqrtf(p->camera_size[0]*p->camera_size[0]+p->camera_size[2]*p->camera_size[2]),sqrtf(p->camera_size[1]*p->camera_size[1]+p->camera_size[2]*p->camera_size[2]));
 int n=64-s->drive_cameras;float angle=camera_random(p)*360;
 for(int k=0;k<n;k++){
  /* 00400188..204 / 003FFAFC..B9C: randomized azimuth, radius and
     local height .8; transform by the frozen car's orientation. */
  float a=(angle+(k+camera_random(p)*.699999988079071f)*360/n)*.01745329252f;
  float d=radius+camera_random(p)*8+.199999988079071f,local[3]={sinf(a)*d,.7999999523162842f,cosf(a)*d},eye[3];
  for(int j=0;j<3;j++)eye[j]=pos[j]+rot[j*3]*local[0]+rot[j*3+1]*local[1]+rot[j*3+2]*local[2];
  float height;if(!gt4_course_height(course,eye[0],eye[2],20000,&height))continue;eye[1]=fmaxf(height+.3999999761581421f,fminf(eye[1],height+3));
  camera_add(p,s,eye);
 }
 for(int k=0;k<s->drive_cameras;k++)p->cameras[k].drive_cameras=s->drive_cameras;
}
void photo_travel_drive_camera(photo_travel *p,int delta){
 if(!p || p->owner!=p->editor || !delta)return;gt4_photo_mode *s=menu_photo_state(p->owner);
 /* 00400408 +0..4C: wrap, retain edits independently in each bank. */
 p->cameras[s->drive_camera]=*s;int i=s->drive_camera+delta;if(i<0)i=s->drive_cameras-1;if(i>=s->drive_cameras)i=0;
 *s=p->cameras[i];s->drive_camera=i;p->zoom_frames=0;
}
int photo_travel_drive(photo_travel *p,menu *m,race_view *view)
{
    if(!p || !m || !view || !race_view_get_camera(view,p->saved_eye,p->saved_rows,p->saved_projection))return 0;
    p->saved_car_visible=race_view_car_visible(view);
    p->view=view;p->owner=p->editor=m;gt4_photo_mode *s=menu_photo_state(m);memset(s,0,sizeof *s);
    memcpy(s->eye,p->saved_eye,12);float rot[9];race_view_photo_pose(view,s->car,rot);
    /* ScenePhoto 003844D0; A: host Euler extraction of frozen race camera.
       This only changes renderer state, never the race car or command cursor. */
    const float d=57.29577951308232f;
    s->pitch=asinf(fmaxf(-1,fminf(1,p->saved_rows[7])))*d;
    s->yaw=atan2f(-p->saved_rows[6],p->saved_rows[8])*d;
    s->roll=atan2f(p->saved_rows[1],p->saved_rows[4])*d;
    s->view_angle=12*p->saved_projection[1];s->aperture=6;s->shutter=3;s->lightness=10;s->drive_ev=10;s->drive_white=6;s->drive_kelvin=p->drive_kelvin[6];
    /* Initial camera is the frozen resident race view. */
    s->drive_cameras=1;s->image_scale=1;s->drive=s->ready=1;strcpy(s->operate_mode,"none");strcpy(s->color_filter,"none");
    camera_bank(p,s);
    menu_set_photo_saver(m,native_photo_save,p);p->captured=0;p->saved[0]=0;p->save_failed=0;
    p->tick=plat_time_ns();p->remainder=0;ui_set_photo_renderer(menu_ui(m),photo_travel_draw,p);
    for(int i=0;i<ui_root_count(menu_ui(m));i++)if(!strcmp(ui_root_name(menu_ui(m),i),"ShootRoot")){
        ui_widget *r=ui_root_widget(menu_ui(m),i),*map=ui_widget_child(r,"Map");
        if(map)map->visible=0;menu_enter(m,i);p->overlay_tick=0;
        photo_pause_close(p->pause);p->pause=photo_pause_open(p->volume,s,p->apertures,p->shutters);
        if(!p->pause){photo_travel_drive_end(p);return 0;}return 1;
    }
    photo_travel_drive_end(p);return 0;
}
int photo_travel_drive_preview(photo_travel *p,menu *preview){
    if(!p || !p->editor || !preview)return 0;
    *menu_photo_state(preview)=*menu_photo_state(p->editor);p->owner=preview;p->captured=0;
    ui_set_photo_renderer(menu_ui(preview),photo_travel_draw,p);menu_set_photo_saver(preview,native_photo_save,p);
    for(int i=0;i<ui_root_count(menu_ui(preview));i++)if(!strcmp(ui_root_name(menu_ui(preview),i),"PreviewRoot")){menu_enter(preview,i);return 1;}
    p->owner=p->editor;return 0;
}
void photo_travel_drive_editor(photo_travel *p){if(p && p->editor){p->owner=p->editor;p->tick=plat_time_ns();p->overlay_tick=0;p->remainder=0;p->editor_buttons=0;for(int k=0;k<ui_root_count(menu_ui(p->editor));k++)if(!strcmp(ui_root_name(menu_ui(p->editor),k),"ShootRoot")){menu_enter(p->editor,k);break;}}}
void photo_travel_editor_draw(photo_travel *p){ui_widget view={0};view.type="PhotoViewFace";photo_travel_draw(p,&view,0,0,640,480,1);}
static int film_pose(photo_travel *p,const gt4_film_frame *f){
 /* 00459610 +28..3C: RotateY(second), RotateX(first), RotateZ(third).
    004595C8 translates the second CPRM vector without axis conversion.
    A: host trig / native scene renderer, no PS2 image comparison yet. */
 for(int k=0;k<f->cars;k++){
  const float *v=f->parameters[k];float a=v[0]*.01745329252f,b=v[1]*.01745329252f,c=v[2]*.01745329252f;
  float ca=cosf(a),sa=sinf(a),cb=cosf(b),sb=sinf(b),cc=cosf(c),sc=sinf(c);
  float rot[9]={cb*cc+sb*sa*sc,-cb*sc+sb*sa*cc,sb*ca,ca*sc,ca*cc,-sa,-sb*cc+cb*sa*sc,sb*sc+cb*sa*cc,cb*ca};
  if(k)race_view_set_opponent(p->view,k-1,v+3,rot,NULL);else race_view_set_pose(p->view,v+3,rot);
  float wheels[4][8];
  for(int i=0;i<4;i++){const float *q=(const float *)((const unsigned char *)v+0x3c+i*0x34);int slot=i^1;memcpy(wheels[slot],q+3,12);for(int j=0;j<3;j++)wheels[slot][j+3]=q[j]*.01745329252f;if(i&1)wheels[slot][5]=-wheels[slot][5];wheels[slot][6]=q[10]*.01745329252f;wheels[slot][7]=q[7];}
  race_view_set_wheel_snapshot(p->view,(unsigned)k,wheels);
  if(!race_view_set_driver_snapshot(p->view,(unsigned)k,f->has_driver[k]?&f->driver[k]:NULL))return 0;
  if(f->has_driver_grasp[k])race_view_set_driver_grasp(p->view,(unsigned)k,f->driver_grasp[k]);
 }
 const float *c=f->camera;float back[3],right[3],up[3],rows[9],length=0;
 for(int k=0;k<3;k++){back[k]=c[k]-c[k+3];length+=back[k]*back[k];}
 if(length<=0 || c[9]<=0 || c[9]>=180 || c[10]<=0 || c[11]<=0)return 0;
 length=sqrtf(length);for(int k=0;k<3;k++)back[k]/=length;
 right[0]=c[7]*back[2]-c[8]*back[1];right[1]=c[8]*back[0]-c[6]*back[2];right[2]=c[6]*back[1]-c[7]*back[0];
 length=sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);if(length<=0)return 0;
 for(int k=0;k<3;k++)right[k]/=length;
 up[0]=back[1]*right[2]-back[2]*right[1];up[1]=back[2]*right[0]-back[0]*right[2];up[2]=back[0]*right[1]-back[1]*right[0];
 memcpy(rows,right,12);memcpy(rows+3,up,12);memcpy(rows+6,back,12);
 float sy=1/tanf(c[9]*.01745329252f*.5f);race_view_set_camera(p->view,c,rows,sy/c[10],sy,c[11]);return 1;
}
int photo_travel_develop(photo_travel *p,menu *preview,const char *path,void (*pump)(void *),void *user){
 if(!p || !preview)return 0;uint8_t *data=NULL;size_t n=0;gt4_film f;
 int ok=gt4_save_read(path,&data,&n,NULL)>0 && gt4_media_film_decode(data,n,&f);free(data);if(!ok)return 0;
 photo_travel_drive_end(p);p->film_active=0;p->view=p->owned_view;
 const char *cars[7];int colors[7];for(int k=1;k<f.cars;k++){cars[k-1]=f.car[k].code;colors[k-1]=f.car[k].paint;}
 race_view_set_parts(p->view,NULL,NULL);
 if(!race_view_set_opponents(p->view,cars,colors,f.cars-1))return 0;
 for(int k=0;k<f.cars;k++){
  if(!race_view_set_film_parts(p->view,(unsigned)k,f.car[k].parts[2],f.car[k].parts[3]))return 0;
  /* 0033A1B0 applies CPRM tire dimensions through 00450F50 before
     pose interpolation. Preserve each wheel, including differing axles. */
  float tires[4][3];for(int i=0;i<4;i++){const float *q=(const float *)((const uint8_t *)f.frame[0].parameters[k]+0x3c+i*0x34);memcpy(tires[i^1],q+6,12);}
  if(!race_view_set_film_tires(p->view,(unsigned)k,tires))return 0;
 }
 if(!race_view_request_film(p->view,f.course,f.car[0].code,f.car[0].paint))return 0;
 int ready=0;while(!ready && !plat_should_quit()){ready=race_view_poll(p->view,0);if(pump)pump(user);if(!ready)SDL_Delay(1);}if(ready!=1)return 0;
 if(f.has_environment && !race_view_photo_environment(p->view,f.environment,sizeof f.environment))return 0;
 for(int k=0;k<f.frames;k++){if(!film_pose(p,&f.frame[k]))return 0;race_view_photo_record(p->view);}
 p->film=f;p->film_active=1;p->owner=preview;p->editor=NULL;p->captured=0;p->saved[0]=0;p->save_failed=0;
 {const gt4_course *course=race_view_course(p->view);uint32_t env=course && course->size>0x84?camera_u32(course->data+0x80):0;
  const uint8_t *data=f.has_environment?p->film.environment:env && env<=course->size?course->data+env:NULL;
  size_t size=f.has_environment?sizeof f.environment:data?course->size-env:0;
  if(f.frame[f.frames-1].leaf_count){p->film_leaves=race_leaves_open(data,size);if(!p->film_leaves || !race_leaves_snapshot(p->film_leaves,f.frame[f.frames-1].leaves,f.frame[f.frames-1].leaf_count)){photo_travel_drive_end(p);return 0;}race_view_set_leaves(p->view,p->film_leaves);}
  if(f.frame[f.frames-1].smoke_count){p->film_smoke=race_smoke_open(data,size);if(!p->film_smoke || !race_smoke_snapshot(p->film_smoke,f.frame[f.frames-1].smoke,f.frame[f.frames-1].smoke_count)){photo_travel_drive_end(p);return 0;}race_view_set_smoke(p->view,p->film_smoke);}
 }
 menu_set_origin(preview,"labomode"); /* PhotoSave returnSequence uses last_menu_project. */
 gt4_photo_mode *s=menu_photo_state(preview);memset(s,0,sizeof *s);s->ready=s->drive=1;s->image_scale=1;
 const float *camera=f.frame[f.frames-1].camera;s->view_angle=camera[15]*1000;s->brightness=f.lens[3]*8;
 s->aperture=1;for(int k=1;k<17;k++)if(fabsf(p->apertures[k]-camera[13])<fabsf(p->apertures[(int)s->aperture]-camera[13]))s->aperture=(float)k;
 /* PCAM +40 stores focus distance; never run autofocus on this snapshot.
    A: native image composition / suspension clamp are not GS parity.
    DRVR, CPRM wheel transforms, SMOK and PTCL are restored above. */
 s->focus_distance=camera[16];strcpy(s->color_filter,"none");
 ui_set_photo_renderer(menu_ui(preview),photo_travel_draw,p);menu_set_photo_saver(preview,native_photo_save,p);
 for(int k=0;k<ui_root_count(menu_ui(preview));k++)if(!strcmp(ui_root_name(menu_ui(preview),k),"PreviewRoot")){menu_enter(preview,k);return 1;}
 photo_travel_drive_end(p);return 0;
}
void photo_travel_drive_end(photo_travel *p)
{
    if(p && p->film_active){race_view_photo_end(p->view);race_view_set_smoke(p->view,NULL);race_smoke_close(p->film_smoke);p->film_smoke=NULL;race_view_set_leaves(p->view,NULL);race_leaves_close(p->film_leaves);p->film_leaves=NULL;p->film_active=0;p->owner=p->editor=NULL;}
    if(!p || p->view==p->owned_view)return;
    photo_pause_close(p->pause);p->pause=NULL;
    race_view_photo_end(p->view);
    race_view_photo_jitter(p->view,NULL);
    race_view_set_camera(p->view,p->saved_eye,p->saved_rows,p->saved_projection[0],p->saved_projection[1],p->saved_projection[2]);
    race_view_show_car(p->view,p->saved_car_visible);
    p->view=p->owned_view;
}
