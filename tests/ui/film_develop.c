/* Native development of authored film scenes. No physics session is opened.
   Confirms reconstruction/export boundaries, not original GS pixel parity. */
#include "../../src/app/photo_travel.c"
#include "film_fixture.h"
#include "car_model.h"
#include "plat_sdl.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"film develop %d: %s\n",__LINE__,#x);return 1;}}while(0)
static size_t scene(gt4_vol *vol,uint8_t *p,const char *course,const float origin[3],const uint8_t *environment){
 film_fixture(p,2048);p[29]=2;ff_float(p+31,0);size_t at=91,b=ff_begin(p,&at,"RHDR");p[at++]=0;
 ff_string(p,&at,course);ff_string(p,&at,"");p[at++]=2;
 const char *cars[]={"ka_01","beat_91"};
 for(int k=0;k<2;k++){ff_string(p,&at,cars[k]);ff_string(p,&at,cars[k]);p[at++]=(uint8_t)k;}
 ff_word(p+at,1);at+=4;for(int k=0;k<2;k++){size_t detail=ff_begin(p,&at,"CDTL");p[at++]=1;ff_string(p,&at,"");ff_string(p,&at,"");ff_string(p,&at,k?"brdg001w":"w000");ff_string(p,&at,"p000");p[at++]=0;ff_end(p,at,detail);}if(environment){size_t env=ff_begin(p,&at,"ENVP");memcpy(p+at,environment,0x17f4);at+=0x17f4;ff_end(p,at,env);}ff_end(p,at,b);
 b=ff_begin(p,&at,"SFRM");size_t cam=ff_begin(p,&at,"PCAM");p[at++]=1;
 float values[17]={origin[0]+8,origin[1]+3,origin[2]+8,origin[0]+2,origin[1]+.5f,origin[2],0,1,0,45,4.f/3,.2f,20000,8,1.f/60,.035f,10};
 for(int k=0;k<17;k++){ff_float(p+at,values[k]);at+=4;}ff_end(p,at,cam);
 for(int k=0;k<2;k++){size_t car=ff_begin(p,&at,"CARD"),prm=ff_begin(p,&at,"CPRM");p[at++]=3;memset(p+at,0,361);
  ff_float(p+at+12,origin[0]+k*4);ff_float(p+at+16,origin[1]);ff_float(p+at+20,origin[2]);
  car_model_tables *tables=car_model_tables_open(vol);car_model *model=car_model_load_race(vol,tables,cars[k],k,2,(unsigned)k);car_model_tables_close(tables);if(!model)return 0;
  for(int wheel=0;wheel<4;wheel++)for(int j=0;j<3;j++)ff_float(p+at+85+wheel*52+12+j*4,model->hub[wheel^1][j]);
  for(int wheel=0;wheel<4;wheel++){for(int j=0;j<3;j++)ff_float(p+at+85+wheel*52+24+j*4,model->tire_size[wheel^1][j]);ff_float(p+at+85+wheel*52,37);ff_float(p+at+85+wheel*52+4,wheel<2?18:0);}
  at+=361;
  ff_end(p,at,prm);
  size_t nb=model->driver_size;uint8_t *shared=model->driver_path?gt4_vol_read(vol,model->driver_path,&nb):NULL;const uint8_t *data=shared?shared:model->driver_data;gt4_driver_rig rig;
  if(data && gt4_driver_rig_read(data,nb,&rig)){gt4_driver_skeleton skeleton;float channels[64],poses[20][16];if(!gt4_driver_skeleton_init(rig.dimensions,&skeleton) || !gt4_driver_record_init(&model->driver,channels) || !gt4_driver_race_pose(&model->driver,channels,&skeleton,poses)){free(shared);car_model_free(model);return 0;}
   size_t driver=ff_begin(p,&at,"DRVR");p[at++]=0;
   for(int bone=0;bone<20;bone++){const gt4_driver_segment *q=&skeleton.bone[bone];memcpy(p+at,q->position,12);at+=12;memcpy(p+at,q->rotation,16);at+=16;memcpy(p+at,q->end,12);at+=12;if(bone==GT4_PELVIS || bone==GT4_SCAPULA){memcpy(p+at,q->branch,24);at+=24;}}
   memset(p+at,0,16);at+=16;ff_end(p,at,driver);
  }free(shared);car_model_free(model);
  if(!k){size_t smoke=ff_begin(p,&at,"SMOK");p[at++]=0;p[at++]=1;p[at++]=0;memset(p+at,0,33);p[at+2]=p[at+3]=p[at+4]=128;p[at+6]=40;p[at+7]=p[at+8]=255;p[at+13]=127;ff_float(p+at+14,origin[0]+2);ff_float(p+at+18,origin[1]+2);ff_float(p+at+22,origin[2]);ff_float(p+at+26,origin[1]);p[at+31]=127;at+=33;ff_end(p,at,smoke);}
  ff_end(p,at,car);
 }size_t leaves=ff_begin(p,&at,"PTCL");p[at++]=0;p[at++]=2;memset(p+at,0,17);p[at+1]=32;ff_float(p+at+4,origin[0]+3);ff_float(p+at+8,origin[1]+2);ff_float(p+at+12,origin[2]);p[at+16]=255;at+=17;p[at++]=0;ff_end(p,at,leaves);ff_end(p,at,b);return at;
}
static unsigned hash_file(const char *path){size_t n;FILE *f=fopen(path,"rb");if(!f)return 0;fseek(f,0,SEEK_END);n=(size_t)ftell(f);rewind(f);uint8_t *b=malloc(n);if(!b){fclose(f);return 0;}size_t got=fread(b,1,n,f);fclose(f);unsigned h=got==n?(unsigned)mz_crc32(0,b,n):0;free(b);return h;}
static int draw(photo_travel *p,menu *m){
 plat_video_begin_frame();ui_widget face={0};face.type="PhotoRenderFace";
 int ok=photo_travel_draw(p,&face,0,0,640,480,1);plat_video_end_frame();return ok && menu_photo_state(m)->image_width>0;
}
int main(int argc,char **argv){
 CHECK(argc>=3);CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));SDL_HideWindow(plat_sdl_window());plat_video_set_view(640,480);
 gt4_save_init(argv[2]);CHECK(gt4_save_mkdir(argv[2]));gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol);char error[256];
 menu *m=menu_open(vol,"photo_save",error,sizeof error);CHECK(m);photo_travel *p=photo_travel_open(argv[1]);CHECK(p);
 const char *courses[]={"tsukuba","ss_brooklyn"};
 for(int i=0;i<2;i++){
  char crs[64];snprintf(crs,sizeof crs,"/crs/%s",courses[i]);size_t bytes;uint8_t *course=gt4_vol_read(vol,crs,&bytes);CHECK(course);
  float pos[3];if(!i){gt4_course c;CHECK(gt4_course_open(&c,course,bytes));gt4_grid g=gt4_course_grid(&c,0);memcpy(pos,g.pos,12);}
  else{gt4_photo_location l;gt4_course c;CHECK(gt4_photo_location_read(course,bytes,&l) && gt4_course_open(&c,course,bytes));memcpy(pos,l.car,12);CHECK(gt4_course_height(&c,pos[0],pos[2],100,&pos[1]));}uint8_t environment[0x17f4];uint32_t env=bytes>0x84?camera_u32(course+0x80):0;
  CHECK(env && env<=bytes && bytes-env>=sizeof environment);memcpy(environment,course+env,sizeof environment);free(course);
  uint8_t raw[16384];size_t n=scene(vol,raw,courses[i],pos,environment);gt4_film decoded;CHECK(gt4_media_film_decode(raw,n,&decoded));
  char path[1200];snprintf(path,sizeof path,"%s/fixture-%d.ogt4film",argv[2],i);CHECK(gt4_save_write(path,raw,n));unsigned source=hash_file(path);
  CHECK(photo_travel_develop(p,m,path,NULL,NULL));CHECK(p->film_active && p->film.cars==2);
  CHECK(p->film.frame[0].has_driver[0] && p->film_smoke && race_smoke_live(p->film_smoke)==1 && p->film_leaves && race_leaves_live(p->film_leaves)==1);
  for(int tick=0;tick<30;tick++){menu_tick(m);menu_animate(m,1.f/60);plat_video_begin_frame();int drawn,skipped;ui_draw_root(menu_ui(m),menu_active_root(m),&drawn,&skipped);plat_video_end_frame();}
  CHECK(p->captured && !strcmp(ui_root_name(menu_ui(m),menu_active_root(m)),"PreviewRoot"));
  CHECK(race_view_car_geometry(p->view,0) && race_view_car_geometry(p->view,1));
  float eye[3],rows[9],pr[3],car[3],rot[9];CHECK(race_view_get_camera(p->view,eye,rows,pr));CHECK(!memcmp(eye,decoded.frame[0].camera,12));
  CHECK(race_view_photo_pose(p->view,car,rot));CHECK(!memcmp(car,decoded.frame[0].parameters[0]+3,12));
  /* Removing only saved wheels/pilot/particles must change developed pixels. */
  p->captured=0;CHECK(draw(p,m) && native_photo_save(p));unsigned details=hash_file(photo_travel_last_saved(p));
  for(int actor=0;actor<4;actor++){
   if(actor==0)race_view_set_wheel_snapshot(p->view,0,NULL);
   if(actor==1)race_view_set_driver_snapshot(p->view,1,NULL);
   if(actor==2)race_view_set_smoke(p->view,NULL);
   if(actor==3)race_view_set_leaves(p->view,NULL);
   p->captured=0;CHECK(draw(p,m) && native_photo_save(p));unsigned removed=hash_file(photo_travel_last_saved(p));
   if(argc>3 && !strcmp(argv[3],"--details-negative"))removed=details;
   if(details==removed)fprintf(stderr,"saved actor %d invisible on course %d\n",actor,i);CHECK(details!=removed);
   CHECK(film_pose(p,&decoded.frame[0]));race_view_set_smoke(p->view,p->film_smoke);race_view_set_leaves(p->view,p->film_leaves);
  }
  CHECK(film_pose(p,&decoded.frame[0]));race_view_set_smoke(p->view,p->film_smoke);race_view_set_leaves(p->view,p->film_leaves);p->captured=0;CHECK(draw(p,m));
  menu_send_input(m,MENU_ACCEPT);CHECK(menu_photo_state(m)->save_requested);
  CHECK(draw(p,m));unsigned developed=hash_file(photo_travel_last_saved(p));CHECK(developed);
  /* Cached preview cannot change with the resident scene or repeated save. */
  race_view_show_car(p->view,0);CHECK(draw(p,m) && native_photo_save(p));CHECK(developed==hash_file(photo_travel_last_saved(p)));
  p->captured=0;CHECK(draw(p,m) && native_photo_save(p));unsigned hidden=hash_file(photo_travel_last_saved(p));
  if(argc>3 && !strcmp(argv[3],"--negative"))hidden=developed;CHECK(developed!=hidden);CHECK(hash_file(path)==source);
  menu_send_input(m,MENU_BACK);char destination[64];int exited=0;
  for(int tick=0;tick<180 && !exited;tick++){menu_tick(m);menu_animate(m,1.f/60);exited=menu_take_project(m,destination,sizeof destination);}
  CHECK(exited);photo_travel_drive_end(p);CHECK(!p->film_active);
  /* Truncation must not replace the last successfully developed scene. */
  CHECK(gt4_save_write(path,raw,n-1));CHECK(!photo_travel_develop(p,m,path,NULL,NULL));
 }
 puts("PASS race/Photo Travel films, two resident cars, stored camera/pose, visible car pixels, frozen preview/PNG, source preserved, corrupt rejection; negative suppresses car pixel difference. fitted wheels and saved actors/particles. Image A.");
 photo_travel_close(p);menu_close(m);gt4_vol_close(vol);gt4_save_shutdown();plat_video_shutdown();return 0;
}
