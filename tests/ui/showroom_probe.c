/* Visual regression capture using the actual asynchronous showroom path. */
#include "showroom.h"
#include "car_model.h"
#include "gpb.h"
#include "mot1.h"
#include "career_vehicle.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include "race_view.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "render3d.h"
#include "platform/platform.h"
#include <SDL.h>
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
typedef struct {showroom *cars;showroom_face *face;const char *code;} service_page;
static void service_page_car(void *user,const ui_widget *w,float x,float y,float wd,float h,float opacity){
    (void)w;(void)opacity;service_page *p=user;
    p->face->x=x;p->face->y=y;p->face->w=wd;p->face->h=h;
    showroom_draw_face(p->cars,p->code,0,p->face);
}
static ui_widget *find_car(ui_widget *w){
    if(!strcmp(w->type,"CarFace"))return w;
    for(int i=0;i<w->nkids;i++){ui_widget *car=find_car(w->kids[i]);if(car)return car;}
    return NULL;
}
static void service_frame(showroom *s,const char *code,showroom_face *f,ui_project *page,int root){
    plat_video_begin_frame();
    if(page){int drawn,skipped;ui_draw_root(page,root,&drawn,&skipped);}
    else showroom_draw_face(s,code,0,f);
}
/* Triangle structs contain host padding; compare their drawing fields. */
static int same_triangles(const gt4_triangle *a,const gt4_triangle *b,int count){
    for(int i=0;i<count;i++,a++,b++){
        if(memcmp(a->v,b->v,sizeof a->v) || a->texture!=b->texture || a->kind!=b->kind ||
           a->blend_enabled!=b->blend_enabled || a->wheel_blur_texture!=b->wheel_blur_texture ||
           a->attachment!=b->attachment || a->lamp!=b->lamp)return 0;
#define FIELD(name) if(a->state.name!=b->state.name)return 0
        FIELD(color_mask);FIELD(blend);FIELD(fixed_alpha);FIELD(depth_write);FIELD(depth_test);
        FIELD(alpha_test);FIELD(alpha_func);FIELD(alpha_ref);FIELD(destination_test);FIELD(destination_bit);
        FIELD(cull);FIELD(depth_func);FIELD(alpha_fail);FIELD(fog_override);
#undef FIELD
        if(memcmp(a->state.fog_rgb,b->state.fog_rgb,3))return 0;
    }return 1;
}
static void benchmark_frame(showroom *s,race_view *v,const char *car)
{
    /* The target is established once before measuring. Clear directly to
       exclude frontend pacing; this probe measures rendering and present. */
    glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
    glClearColor(0,0,0,1);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    if(v)race_view_draw(v);else showroom_draw(s,car,0);
    plat_video_end_frame();
}
static void benchmark(showroom *s,race_view *v,const char *car)
{
    const char *value=getenv("OPENGT4_PROBE_FRAMES");int frames=value?atoi(value):0;
    if(frames<=0)return;
    if(frames>10000)frames=10000;
    void (APIENTRY *finish)(void)=(void (APIENTRY *)(void))SDL_GL_GetProcAddress("glFinish");
    if(!finish)return;
    video_set_vsync(0);
    plat_video_begin_frame();
    for(int i=0;i<10;i++){benchmark_frame(s,v,car);finish();}
    Uint64 start=SDL_GetPerformanceCounter();
    for(int i=0;i<frames;i++){benchmark_frame(s,v,car);finish();}
    double ms=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency()/frames;
    fprintf(stderr,"renderer benchmark: %d frames %.3f ms/frame (CPU + GPU, vsync off)\n",frames,ms);
    r3d_stats stats=r3d_frame_stats();
    fprintf(stderr,"renderer frame: %u draws, %u triangles, %u groups culled\n",stats.draws,stats.triangles,stats.culled_groups);
}
int main(int argc,char **argv)
{
    if(argc<4){fprintf(stderr,"usage: showroom_probe disc car output.png [color] [seconds]\n");return 2;}
    plat_video_desc desc={640,448,0,0,getenv("OPENGT4_UPSCALE")!=NULL};
    const char *size=getenv("OPENGT4_WINDOW");   /* e.g. 1920x1344: GPU captures at that resolution */
    if(size && sscanf(size,"%dx%d",&desc.width,&desc.height)!=2)return 2;
    if(plat_video_init(&desc))return 1;
    plat_video_set_view(640,448);SDL_HideWindow(SDL_GetWindowFromID(1));
    if(getenv("OPENGT4_PROBE_SERVICE_BLEND")){
        unsigned cases=0;int ok=1;float maximum=0;
        /* GS ALPHA 0x64=(Cs-Cd)*FIX/128+Cd, 0x68=Cs*FIX/128+Cd.
           FIX itself is compared to original 004A2808 by service_placement.
           Read back only in this test, never in the service renderer. */
        for(int samples=1;samples<=2;samples++)for(int add=0;add<2;add++)for(unsigned i=0;i<=32;i++){
            unsigned dst[3]={41,97,151},src[2][3]={{173,113,61},{23,179,137}};float opacity=i/32.f;
            plat_video_begin_frame();glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);
            glClearColor(dst[0]/255.f,dst[1]/255.f,dst[2]/255.f,1);glClear(GL_COLOR_BUFFER_BIT);
            for(int sample=0;sample<samples;sample++){
                r3d_begin(0,0,640,448,NULL);ok=ok && r3d_begin_layer(1);
                glClearColor(src[sample][0]/255.f,src[sample][1]/255.f,src[sample][2]/255.f,1);glClear(GL_COLOR_BUFFER_BIT);
                int index=getenv("OPENGT4_PROBE_SERVICE_SAMPLE_NEGATIVE")?0:sample;
                r3d_end_layer_sample(opacity,add,index,samples);
            }
            int w,h;uint8_t *pixels=plat_sdl_read_pixels(&w,&h);ok=ok && pixels;
            unsigned fix=gt4_service_blend_fix(opacity);
            if(getenv("OPENGT4_PROBE_SERVICE_BLEND_NEGATIVE"))fix^=16;
            for(int k=0;k<3 && pixels;k++){
                float merged=samples==1?src[0][k]:(src[0][k]+src[1][k])*.5f;
                float expected=fminf(255,(merged*fix+dst[k]*(add?128:128-fix))/128.f);
                float delta=fabsf(pixels[(h/2*w+w/2)*4+k]-expected);if(delta>maximum)maximum=delta;
                if(delta>(samples==1?1.f:2.f))ok=0;
            }
            free(pixels);plat_video_end_frame();cases++;
        }
        fprintf(stderr,"%s service GPU composition: %u cases, maximum %.3f/255 (tolerance 1/255 single, 2/255 accumulated)\n",ok?"PASS":"FAIL",cases,maximum);
        plat_video_shutdown();return ok?0:1;
    }
    const char *course=getenv("OPENGT4_COURSE");   /* race view: car on this course's grid */
    if(course) {
        int color=argc>4?atoi(argv[4]):0,r=0;race_view *v=race_view_open(argv[1]);
        if(!v || !race_view_request(v,course,argv[2],color))r=-1;
        while(!r){SDL_PumpEvents();SDL_Delay(10);r=race_view_poll(v,0);}
        if(r<0){fprintf(stderr,"cannot load course %s\n",course);race_view_close(v);plat_video_shutdown();return 1;}
        benchmark(NULL,v,argv[2]);
        plat_video_begin_frame();int ok=race_view_draw(v);
        int pw=0,ph=0;size_t len=0;unsigned char *px=ok?plat_sdl_read_pixels(&pw,&ph):NULL;
        void *png=px?tdefl_write_image_to_png_file_in_memory(px,pw,ph,4,&len):NULL;
        FILE *f=png?fopen(argv[3],"wb"):NULL;if(f){fwrite(png,1,len,f);fclose(f);}else ok=0;
        mz_free(png);free(px);race_view_close(v);plat_video_shutdown();return ok?0:1;
    }
    if(getenv("OPENGT4_PROBE_TEST_WING")){
        const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&err);
        car_model_tables *tables=vol?car_model_tables_open(vol):NULL;
        car_model *stock=tables?car_model_load(vol,tables,argv[2],0):NULL;
        car_model *changed=tables?car_model_load_parts(vol,tables,argv[2],0,NULL,"p001"):NULL;
        car_model *restored=tables?car_model_load_parts(vol,tables,argv[2],0,NULL,"p000"):NULL;
        if(getenv("OPENGT4_PROBE_WING_NEGATIVE") && restored)restored->car->triangles[0].v[0].p[0]+=1;
        int ok=stock && changed && restored && changed->car->count>stock->car->count &&
            stock->car->count==restored->car->count &&
            same_triangles(stock->car->triangles,restored->car->triangles,stock->car->count);
        car_model_free(stock);car_model_free(changed);car_model_free(restored);
        car_model_tables_close(tables);if(vol)gt4_vol_close(vol);plat_video_shutdown();
        fprintf(stderr,"%s wing replacement: authored geometry added, removal restores drawing fields exactly\n",ok?"PASS":"FAIL");return ok?0:1;
    }
    if(getenv("OPENGT4_PROBE_TEST_WHEEL")){
        const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&err);
        car_model_tables *tables=vol?car_model_tables_open(vol):NULL;
        car_model *stock=tables?car_model_load(vol,tables,argv[2],0):NULL;
        car_model *changed=tables?car_model_load_wheel(vol,tables,argv[2],0,"brdg001w"):NULL;
        car_model *restored=tables?car_model_load_wheel(vol,tables,argv[2],0,"w000"):NULL;
        if(getenv("OPENGT4_PROBE_WHEEL_NEGATIVE") && restored)restored->car->triangles[0].v[0].p[0]+=1;
        int ok=stock && changed && restored && stock->body_count==changed->body_count &&
            same_triangles(stock->car->triangles,changed->car->triangles,stock->body_count) &&
            stock->car->count==restored->car->count &&
            same_triangles(stock->car->triangles,restored->car->triangles,stock->car->count);
        int different=0;
        for(int i=0;i<4 && ok;i++){
            if(!stock->wheel[i] || !changed->wheel[i]){ok=0;break;}
            if(stock->wheel[i]->count!=changed->wheel[i]->count ||
                !same_triangles(stock->wheel[i]->triangles,changed->wheel[i]->triangles,stock->wheel[i]->count))different++;
        }
        ok=ok && different==4;
        car_model_free(stock);car_model_free(changed);car_model_free(restored);
        car_model_tables_close(tables);if(vol)gt4_vol_close(vol);plat_video_shutdown();
        fprintf(stderr,"%s wheel replacement: body preserved, four wheel meshes changed, stock restored byte for byte\n",ok?"PASS":"FAIL");return ok?0:1;
    }
    showroom *s=showroom_open(argv[1]);if(!s)return 1;
    const char *wing=getenv("OPENGT4_PROBE_WING");if(wing)showroom_set_wing(s,wing);
    const char *wheel=getenv("OPENGT4_PROBE_WHEEL");
    if(wheel)showroom_set_wheel(s,wheel);
    int ready=0,color=argc>4?atoi(argv[4]):0,applied=!color;
    for(int i=0;i<1500;i++){
        SDL_PumpEvents();plat_video_begin_frame();ready=showroom_draw(s,argv[2],1.f/60);
        if(ready && !applied){showroom_color(s,color);applied=1;ready=0;}
        if(ready)break;plat_video_end_frame();SDL_Delay(10);
    }
    /* The number of frames spent waiting for the asynchronous load varies
       between runs; restart the rotation so captures are reproducible. */
    showroom_set_angle(s,0);
    if(getenv("OPENGT4_PROBE_SERVICE")){
        /* main.c uses the menu's 640x480 canvas; the 640x448 canvas above
           is for standalone showroom/race probes. Do not compare a menu
           through the race canvas and attribute its stretch to the model. */
        plat_video_set_view(640,480);
        int wash=!strcmp(getenv("OPENGT4_PROBE_SERVICE"),"wash");const char *error=NULL;size_t bytes=0;
        gt4_vol *vol=gt4_vol_open(argv[1],&error);uint8_t *package=vol?gt4_vol_read(vol,wash?"/projects/gtmode/US/CarWashRoot.gpb":"/projects/gtmode/US/OilChangeRoot.gpb",&bytes):NULL;
        const char *mpaths[]={"image/gtmode/gtauto/pitmanFix.mdl",wash?"image/gtmode/gtauto/carwash_cloth.mdl":"image/gtmode/gtauto/oil_change_cart.mdl"};
        const char *qpaths[]={wash?"image/gtmode/gtauto/carwash_motion1.mot":"image/gtmode/gtauto/oil_change.mot",wash?"image/gtmode/gtauto/carwash_motion2.mot":"image/gtmode/gtauto/oil_change_cart.mot","image/gtmode/gtauto/carwash_cloth1.mot","image/gtmode/gtauto/carwash_cloth2.mot"};
        uint8_t *models[2]={0};gt4_service_model bank[2]={0};gt4_service_motion tracks[4]={0};int count=wash?4:2,ok=ready && package;
        for(int i=0;i<2;i++){size_t n=0;models[i]=package?gpb_load(package,bytes,mpaths[i],&n):NULL;bank[i]=(gt4_service_model){models[i],n,(uint64_t)i+100};ok=ok && models[i];}
        float service_time=getenv("OPENGT4_PROBE_SERVICE_TIME")?(float)atof(getenv("OPENGT4_PROBE_SERVICE_TIME")):5;
        int service_alignment=getenv("OPENGT4_PROBE_SERVICE_ALIGNMENT")?atoi(getenv("OPENGT4_PROBE_SERVICE_ALIGNMENT")):9;
        for(int i=0;i<count;i++){size_t n=0;uint8_t *b=package?gpb_load(package,bytes,qpaths[i],&n):NULL;gt4_mot1 *q=b?gt4_mot1_open(b,n):NULL;free(b);
            tracks[i]=(gt4_service_motion){q,service_time,wash?0:-90,wash?i/2:i,wash?(i%2?1:7):service_alignment,0,1,0};ok=ok && q;}
        /* OilChangeRoot L12062..12088: equal front/rear service alignment
           (FF or MR/RR) uses f_cam; MR/RR sets car_model.ry to 180. */
        const char *camera_path=wash?"image/gtmode/gtauto/car_wash_cam.mot":"image/gtmode/gtauto/oil_change_f_cam.mot";
        size_t cn=0;uint8_t *cb=package?gpb_load(package,bytes,camera_path,&cn):NULL;
        gt4_mot1 *cam=cb?gt4_mot1_open(cb,cn):NULL;free(cb);showroom_face face={0};face.w=640;face.h=480;face.pose_valid=1;float fov=0;
        /* CarModel+8C in both captured service states (slots06/07): 3F733333;
           application obtains this value from the original onLoad script. */
        face.models=bank;face.motions=tracks;face.model_count=2;face.motion_count=count;face.additional_scale=.949999988079071f;
        face.look_valid=gt4_mot1_camera(cam,service_time,face.look,&fov);face.view_angle=fov*57.29577951308232f;ok=ok && face.look_valid;
        gt4_mot1_value(cam,"lift",service_time,&face.service_lift);
        if(!wash && service_alignment==10)face.car_angle=180.f*.017453292519943295f;
        gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);uint8_t unit[32],body[0x4b8];
        face.service_geometry_valid=factory && gt4_career_vehicle_create_seeded(factory,argv[2],0,0,0,unit,body) && gt4_career_vehicle_service_geometry(factory,body,face.service_geometry);
        ok=ok && face.service_geometry_valid;
        /* Optional comparison fixture: authored page, CarFace rectangle and
           LightSet attributes. The service itself remains frozen at the
           selected savestate time; this is not a script/UI timing oracle. */
        ui_project *page=NULL;int root=-1;service_page page_car={s,&face,argv[2]};
        if(getenv("OPENGT4_PROBE_SERVICE_PAGE")){
            char err[256];page=ui_project_load(vol,"gtmode",err,sizeof err);
            for(int i=0;page && i<ui_root_count(page);i++)if(!strcmp(ui_root_name(page,i),wash?"CarWashRoot":"OilChangeRoot")){root=i;break;}
            ui_widget *car=root>=0?find_car(ui_root_widget(page,root)):NULL;ok=ok && car;
            if(car){
                car->car_code=strdup(argv[2]);ui_root_widget(page,root)->visible=1;ui_set_car_renderer(page,service_page_car,&page_car);
                /* 00210830 +0x9c..0x12c: constructor light defaults, in UI color units. */
                double world[4],pos0[4],amb0[4],dif0[4],spe0[4];
                for(int k=0;k<4;k++){
                    world[k]=k==3?255:0;pos0[k]=k==1?1:0;
                    amb0[k]=k==3?255:.29999998211860657*255;
                    dif0[k]=spe0[k]=k==3?255:127.5;
                }
                mp_numbers(car->src,"world_ambient",world,4);
                for(int l=0;l<3;l++){
                    char key[24];double on[1]={l==0},pos[4],amb[4],dif[4],spe[4];
                    memcpy(pos,pos0,sizeof pos);memcpy(amb,amb0,sizeof amb);memcpy(dif,dif0,sizeof dif);memcpy(spe,spe0,sizeof spe);
                    if(l){snprintf(key,sizeof key,"l%d_enable",l);mp_numbers(car->src,key,on,1);}if(!on[0])continue;
                    snprintf(key,sizeof key,"l%d_pos",l);mp_numbers(car->src,key,pos,4);
                    snprintf(key,sizeof key,"l%d_amb",l);mp_numbers(car->src,key,amb,4);
                    snprintf(key,sizeof key,"l%d_dif",l);mp_numbers(car->src,key,dif,4);
                    snprintf(key,sizeof key,"l%d_spe",l);mp_numbers(car->src,key,spe,4);
                    int n=face.lights.count++;
                    for(int k=0;k<3;k++){face.lights.direction[n][k]=(float)pos[k];face.lights.ambient[n][k]=(float)(amb[k]+(n==0?world[k]:0))/255;
                        face.lights.diffuse[n][k]=(float)dif[k]/255;face.lights.specular[n][k]=(float)spe[k]/255;}
                }
                face.lights.specular_scale=.18f; /* existing app approximation */
            }
            if(!car){ui_project_free(page);page=NULL;}
        }
        showroom_service_channels(&face,cam,service_time);
        int w=0,h=0;unsigned changes[2]={0};
        if(getenv("OPENGT4_PROBE_SERVICE_NEGATIVE"))face.motion_count=0;
        for(int i=0;i<30;i++){service_frame(s,argv[2],&face,page,root);plat_video_end_frame();}
        service_frame(s,argv[2],&face,page,root);unsigned char *all=ok?plat_sdl_read_pixels(&w,&h):NULL;
        unsigned lift_changes=0;float lift=face.service_lift;
        if(lift>0){face.service_lift=0;service_frame(s,argv[2],&face,page,root);
            int lw,lh;unsigned char *flat=plat_sdl_read_pixels(&lw,&lh);
            if(flat && all && lw==w && lh==h)for(int p=0;p<w*h;p++)if(memcmp(all+p*4,flat+p*4,4))lift_changes++;
            free(flat);face.service_lift=lift;ok=ok && lift_changes>0;
        }
        for(int model=0;model<2;model++){
            for(int i=0;i<count;i++)tracks[i].hidden=tracks[i].model==model;
            service_frame(s,argv[2],&face,page,root);int w1,h1;unsigned char *other=ok?plat_sdl_read_pixels(&w1,&h1):NULL;
            if(other && all && w==w1 && h==h1)for(int p=0;p<w*h;p++)if(memcmp(all+p*4,other+p*4,4))changes[model]++;free(other);
            for(int i=0;i<count;i++)tracks[i].hidden=0;
        }
        ok=ok && all && changes[0]>0 && changes[1]>0;
        size_t png_size=0;void *png=all?tdefl_write_image_to_png_file_in_memory(all,w,h,4,&png_size):NULL;FILE *out=png?fopen(argv[3],"wb"):NULL;
        if(out){fwrite(png,1,png_size,out);fclose(out);}else ok=0;
        fprintf(stderr,"%s %s renderer: hiding model 0/1 changes %u/%u pixels, resident meshes\n",ok?"PASS":"FAIL",wash?"wash":"oil",changes[0],changes[1]);
        fprintf(stderr,"service lift %.9g: %u changed pixels\n",lift,lift_changes);
        fprintf(stderr,"service viewport %.9g %.9g %.9g %.9g\n",face.x,face.y,face.w,face.h);
        mz_free(png);free(all);for(int i=0;i<2;i++)free(models[i]);for(int i=0;i<count;i++)gt4_mot1_close((gt4_mot1 *)tracks[i].motion);gt4_mot1_close(cam);
        ui_project_free(page);gt4_career_vehicle_close(factory);free(package);if(vol)gt4_vol_close(vol);showroom_close(s);plat_video_shutdown();return ok?0:1;
    }
    if(getenv("OPENGT4_PROBE_TEST_CREW")){
        const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);size_t bytes=0,model_size=0,motion_size=0,camera_size=0;
        uint8_t *package=vol?gt4_vol_read(vol,"/projects/gtmode/US/GTAutoRoot.gpb",&bytes):NULL;
        uint8_t *model=package?gpb_load(package,bytes,"image/gtmode/gtauto/pitmanFix.mdl",&model_size):NULL;
        uint8_t *motion_data=package?gpb_load(package,bytes,"image/gtmode/gtauto/GTAutoEntranceMotion.mot",&motion_size):NULL;
        uint8_t *camera_data=package?gpb_load(package,bytes,"image/gtmode/gtauto/gtauto_entrance_cam.mot",&camera_size):NULL;
        gt4_mot1 *motion=motion_data?gt4_mot1_open(motion_data,motion_size):NULL,*camera=camera_data?gt4_mot1_open(camera_data,camera_size):NULL;
        showroom_face face={0};face.w=640;face.h=480;face.pose_valid=1;float fov=0;
        int ok=ready && model && motion && camera && gt4_mot1_camera(camera,0,face.look,&fov);
        face.look_valid=1;face.view_angle=fov*57.29577951308232f;
        for(int i=0;i<30;i++){plat_video_begin_frame();showroom_draw_face(s,argv[2],.05f,&face);plat_video_end_frame();}
        int w=0,h=0;plat_video_begin_frame();showroom_draw_face(s,argv[2],0,&face);
        unsigned char *before=ok?plat_sdl_read_pixels(&w,&h):NULL;unsigned base_draws=r3d_frame_stats().draws;
        face.crew_data=model;face.crew_size=model_size;face.crew_motion=motion;face.crew_serial=1;
        if(getenv("OPENGT4_PROBE_CREW_NEGATIVE"))face.crew_motion=NULL;
        plat_video_begin_frame();showroom_draw_face(s,argv[2],0,&face);
        int w2=0,h2=0;unsigned char *after=ok?plat_sdl_read_pixels(&w2,&h2):NULL;
        ok=ok && before && after && w==w2 && h==h2 && r3d_frame_stats().draws>base_draws;
        unsigned changed=0;if(ok)for(int p=0;p<w*h;p++)if(memcmp(before+p*4,after+p*4,4))changed++;
        ok=ok && changed>0;
        size_t png_size=0;void *png=after?tdefl_write_image_to_png_file_in_memory(after,w2,h2,4,&png_size):NULL;
        FILE *output=png?fopen(argv[3],"wb"):NULL;if(output){fwrite(png,1,png_size,output);fclose(output);}else ok=0;
        fprintf(stderr,"%s crew rendering: %u pixels changed from car-only, resident skin mesh\n",ok?"PASS":"FAIL",changed);
        mz_free(png);free(before);free(after);gt4_mot1_close(motion);gt4_mot1_close(camera);free(model);free(motion_data);free(camera_data);free(package);
        if(vol)gt4_vol_close(vol);showroom_close(s);plat_video_shutdown();return ok?0:1;
    }
    /* OPENGT4_PROBE_APPEAR: capture [seconds] into the car's entrance instead */
    if(getenv("OPENGT4_PROBE_APPEAR"))showroom_hide(s);
    plat_video_begin_frame();showroom_draw(s,argv[2],0);
    int frames=argc>5?(int)(atof(argv[5])*20):0;
    for(int i=0;i<frames;i++){plat_video_begin_frame();showroom_draw(s,argv[2],.05f);if(i+1<frames)plat_video_end_frame();}
    benchmark(s,NULL,argv[2]);
    plat_video_begin_frame();showroom_draw(s,argv[2],0);
    int pw=0,ph=0;size_t len=0;
    unsigned char *px=ready?plat_sdl_read_pixels(&pw,&ph):NULL;
    int ok=px!=NULL;
    void *png=ok?tdefl_write_image_to_png_file_in_memory(px,pw,ph,4,&len):NULL;
    FILE *f=png?fopen(argv[3],"wb"):NULL;if(f){fwrite(png,1,len,f);fclose(f);}else ok=0;
    mz_free(png);free(px);showroom_close(s);plat_video_shutdown();return ok?0:1;
}
