/* Diagnostic native render of a user's PCSX2 pit snapshot. The clocks and
   selected motion names come from the capture; GPU output is still A.
   No capture or game data is distributed. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "miniz.h"
#include "../../src/app/race_game.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pit capture line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned word(const uint8_t*d){uint32_t v;memcpy(&v,d,4);return v;}
static unsigned half(const uint8_t*d){return d[0]|(unsigned)d[1]<<8;}
static float real(const uint8_t*d){float v;memcpy(&v,d,4);return v;}
typedef struct {gt4_vol*vol;const uint8_t*ram;uint32_t runway;char course[96];} match_course;
static void match(void*user,const char*name,int directory,uint32_t bytes){
 (void)bytes;match_course*m=user;if(directory || *m->course)return;char path[128];snprintf(path,sizeof path,"/crs/%s",name);
 size_t size;uint8_t*d=gt4_vol_read(m->vol,path,&size);gt4_course c;
 if(d && gt4_course_open(&c,d,size) && !memcmp(d+c.runway+0x14,m->ram+m->runway+0x14,4) &&
    !memcmp(d+c.road_vertices,m->ram+word(m->ram+m->runway+0x74),48))snprintf(m->course,sizeof m->course,"%s",name);
 free(d);
}
int main(int argc,char**argv){
 CHECK(argc>=4 && argc<=6);int negative=argc==6 && !strcmp(argv[5],"--negative");
 int memory_negative=argc==6 && !strcmp(argv[5],"--negative-memory");plat_video_desc desc={1280,896,0,0,0};CHECK(!plat_video_init(&desc));
 SDL_HideWindow(plat_sdl_window());
#ifdef _WIN32
 CHECK(!_putenv_s("OPENGT4_RACE_SNAPSHOT",argv[2]));
#else
 CHECK(!setenv("OPENGT4_RACE_SNAPSHOT",argv[2],1));
#endif
 FILE*input=fopen(argv[2],"rb");CHECK(input);uint8_t*ram=malloc(0x2000000);CHECK(ram && fread(ram,1,0x2000000,input)==0x2000000);fclose(input);
 uint32_t captured_world=0;for(uint32_t p=0x1000000;p<0x1ff0000;p+=16)if(word(ram+p+0x80)==0x67a078){captured_world=p;break;}CHECK(captured_world);
 uint32_t cond=word(ram+captured_world+0x70),course_object=word(ram+cond+0xcbd8),runway=word(ram+word(ram+course_object+4)+0xdc);
 const char*error;gt4_vol*vol=gt4_vol_open(argv[1],&error);CHECK(vol);match_course matched={vol,ram,runway,{0}};
 if(argc>=5)snprintf(matched.course,sizeof matched.course,"%s",argv[4]);else gt4_vol_list(vol,"/crs",match,&matched);
 CHECK(*matched.course);printf("captured course: %s (length and first runway vertices exact)\n",matched.course);gt4_vol_close(vol);free(ram);
 race_game*g=race_game_open(argv[1]);char message[256];CHECK(g);
 CHECK(race_game_start(g,matched.course,"ka_01",0,NULL,NULL,0,message,sizeof message));
 int ready=0;for(int i=0;i<4000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}CHECK(ready==1 && g->pit_scene);
 /* Locate SceneWorld by its vtable marker at +80. */
 uint32_t world=0;for(uint32_t p=0x1000000;p<0x2000000-0x10000;p+=16)if(GT4_U32(p+0x80)==0x67a078){world=p;break;}CHECK(world);
 uint32_t pit=world+0x3628;size_t size;uint8_t*package=gt4_vol_read(g->vol,g->pit_package==2?"/pit/pit_002":"/pit/pit_001",&size);CHECK(package);
 uint8_t*bank=package+word(package+4);unsigned table=word(bank+28);
 for(unsigned entry=0;entry<(unsigned)gt4_race_run_cars(g->run);entry++){
  gt4_pit_crew*c=g->pit_crew+entry;c->stage=GT4_U32(pit+0x9684+entry*4);if(!c->stage)continue;
  gt4_pit_crew calculated=*c;gt4_pit_crew_place(&calculated,gt4_race_run_car(g->run,entry),entry,race_view_car_geometry(g->view,entry),g->pit_package==1,&g->pit_random);
  printf("calculated root %.9g %.9g %.9g %.9g\n",calculated.root[0],calculated.root[1],calculated.root[2],calculated.root[3]);
  for(unsigned role=0;role<6;role++)printf("calculated role %u %.9g %.9g %.9g %.9g\n",role,calculated.place[role][0],calculated.place[role][1],calculated.place[role][2],calculated.place[role][3]);
  memcpy(c->root,gt4_ptr(pit+0x90f8+entry*12),12);c->root[3]=GT4_F32(pit+0x8f30+entry*4);c->placed=1;
  c->car=gt4_race_run_car(g->run,entry);c->fixed=c->stage>=2;c->frozen=c->stage>=4;
  memcpy(c->old_root,gt4_ptr(pit+0x8f90+entry*12),12);
  c->walk_spacing=GT4_U32(pit+0x7c5c+entry*0x320+0x31c);
  for(unsigned r=0;r<4;r++){uint32_t address=pit+0x7c5c+entry*0x320+r*0x74;memcpy(c->walking+r,gt4_ptr(address),0x74);c->walking[r].target=(GT4_U32(address+8)-address-0xc)/16;}
  c->old_euler[0]=GT4_F32(pit+0x8f48+entry*12);c->old_euler[1]=-GT4_F32(pit+0x8f50+entry*12);c->old_euler[2]=GT4_F32(pit+0x8f4c+entry*12);
  uint32_t camera=pit+4,cam_bank=word(package+8);uint8_t*cb=package+cam_bank;unsigned cam_index=GT4_U32(camera+0xc8+entry*4);
  CHECK(cam_index<half(cb+22));CHECK(gt4_license_motion_select(c->motion,6,(char*)cb+word(cb+word(cb+28)+cam_index*16)));
  CHECK(gt4_license_motion_set_time(c->motion,6,GT4_F32(camera+0x80+entry*4)));c->camera_control=GT4_U32(camera+0xb0+entry*4);
  c->camera_choice=GT4_U32(camera+0xe0+entry*4);c->camera_begin=GT4_F32(camera+0x128+entry*4);c->camera_atomic=GT4_U32(camera+0x110+entry*4);
  printf("camera entry %u %s clock %.9g control %d choice %d begin %.9g\n",entry,(char*)cb+word(cb+word(cb+28)+cam_index*16),gt4_license_motion_time(c->motion,6),c->camera_control,c->camera_choice,c->camera_begin);
  memcpy(c->camera_origin,gt4_ptr(camera+0x158+entry*12),12);c->camera_origin[3]=GT4_F32(camera+0x140+entry*4);memcpy(c->camera_hub,gt4_ptr(camera+0x1a0+entry*48),48);
  for(unsigned role=0;role<6;role++){
   uint32_t record=pit+0x3e4+entry*0x8d0+role*0x178,named=GT4_U32(pit+0x38c4+role*0xaa4);CHECK(named<half(bank+22));
   const char*name=(char*)bank+word(bank+table+named*16);
   CHECK(gt4_license_motion_select(c->motion,role,name));CHECK(gt4_license_motion_set_time(c->motion,role,GT4_F32(record)));
   c->requested[role]=!GT4_U32(record+0x16c);c->active[role]=!(GT4_U32(record+0x170) || GT4_U32(record+0x174));memcpy(c->place[role],gt4_ptr(record+4),12);c->place[role][3]=GT4_F32(record+0x10);
   if(role<4){c->blend[role]=GT4_F32(pit+0x9140+entry*16+role*4);c->tire_scale[role]=GT4_F32(pit+0x91a0+entry*16+role*4);c->camber[role]=GT4_F32(pit+0x95e0+entry*16+role*4);c->rotation[role][0]=GT4_F32(pit+0x9520+entry*16+role*4);c->rotation[role][1]=GT4_F32(pit+0x9580+entry*16+role*4);
    for(unsigned axis=0;axis<3;axis++)c->old_hub[role][axis]=GT4_F32(pit+0x8fd8+axis*0x60+entry*16+role*4);
    memcpy(c->cable[role],gt4_ptr(pit+0x96d8+entry*32+role*8),8);memcpy(c->cable[role]+2,gt4_ptr(pit+0x9798+entry*32+role*8),8);}
   const char*markers[]={"PELVIS","CRANIUM",role<4?"gun":role==4?"fuel":"LoliPop"};float matrices[3][16];
   CHECK(gt4_license_motion_frames_blend(c->motion,role,role<4?c->blend[role]:0,markers,3,matrices)==3);
   if(role<4){const char*tires[]={"tire0","tire1"};float wheel[2][16];CHECK(gt4_license_motion_frames(c->motion,role,tires,2,wheel)==2);
    printf("tire prop %u old %.9g %.9g %.9g new %.9g %.9g %.9g\n",role,wheel[0][12],wheel[0][13],wheel[0][14],wheel[1][12],wheel[1][13],wheel[1][14]);}
   printf("entry %u role %u %s time %.9g local %.9g %.9g %.9g yaw %.9g\n",entry,role,name,GT4_F32(record),c->place[role][0],c->place[role][1],c->place[role][2],c->place[role][3]);
  }
 }
 for(unsigned entry=0;entry<(unsigned)gt4_race_run_cars(g->run);entry++){float look[9],fov;if(gt4_pit_camera(g->pit_crew+entry,look,&fov)){printf("camera look fov %.9g\n",fov);for(unsigned i=0;i<9;i++)printf("look %u %.9g EE %.9g\n",i,look[i],GT4_F32(pit+4+i*4));}}
 free(package);race_game_update(g,0);
#ifdef _WIN32
 CHECK(!_putenv_s("OPENGT4_RACE_SNAPSHOT",""));
#else
 unsetenv("OPENGT4_RACE_SNAPSHOT");
#endif
 {float look[9],fov,eye[3],rows[9],projection[3];if(gt4_pit_camera(g->pit_crew,look,&fov)){
  printf("after update: choice %d begin %.9g clock %.9g\n",g->pit_crew[0].camera_choice,g->pit_crew[0].camera_begin,gt4_license_motion_time(g->pit_crew[0].motion,6));
  CHECK(race_view_get_camera(g->view,eye,rows,projection));for(unsigned j=0;j<3;j++){printf("render eye %u %.9g wanted %.9g\n",j,eye[j],look[j]);CHECK(fabsf(eye[j]-look[j])<=.00005f);}
  for(unsigned j=0;j<9;j++){if(negative && j==0)look[j]+=.125f;CHECK(fabsf(look[j]-GT4_F32(pit+4+j*4))<=.00005f);}}}
 plat_video_begin_frame();CHECK(race_game_draw(g));race_game_draw_hud(g,0);
 int w,h;unsigned char*pixels=plat_sdl_read_pixels(&w,&h);CHECK(pixels);size_t bytes;
 void*png=tdefl_write_image_to_png_file_in_memory(pixels,w,h,4,&bytes);CHECK(png);FILE*f=fopen(argv[3],"wb");CHECK(f);CHECK(fwrite(png,1,bytes,f)==bytes);fclose(f);mz_free(png);free(pixels);
 /* A presentation regression on captured car state, not a physics run.
    Inject only fixture inputs, then compare the entire RAM before/after
    each controller/placement batch. Events must reach the existing driver. */
 gt4_pit_crew*c=g->pit_crew;uint32_t car=gt4_race_run_car(g->run,0);uint8_t original[CAR_SIZE];memcpy(original,gt4_ptr(car),sizeof original);
 uint8_t*before=malloc(0x2000000);CHECK(before);unsigned effects[80]={0},flow=0;
 gt4_pit_crew_init(c,c->motion);
 for(unsigned frame=0;frame<980;frame++){
  GT4_U8(car+0x56b)=frame<120?2:frame<841?3:4;GT4_U8(car+0x5b5)|=0x10;
  /* 0035AF18/0035B180: service timer counts DOWN from 720. */
  GT4_U16(car+0x5bc)=frame<840?0x1c0:0;GT4_U16(car+0x5be)=frame<120 || frame>=841?0:(uint16_t)(840-frame);
  if(frame==841)GT4_U16(car+0x5bc)|=0x200;
  memcpy(before,gt4_phys_mem,0x2000000);gt4_pit_crew_sound heard;
  int previous=c->stage,next=gt4_pit_stage(car);
  if(next){c->stage=next;gt4_pit_crew_place(c,car,0,race_view_car_geometry(g->view,0),g->pit_package==1,&g->pit_random);c->stage=previous;}
  gt4_pit_crew_step(c,car,1.f/60,&heard);
  unsigned lane=GT4_U8(car+0x56b);int commands=c->stage==1?(lane==4?1:lane==2 || lane==3?(int)lane:0):0;
  gt4_pit_walk_step(g->pit_crew,(unsigned)gt4_race_run_cars(g->run),0,commands,1.f/60,&g->pit_random);
  if(memory_negative && frame==200)GT4_U8(car+0x5bc)^=1;
  CHECK(!memcmp(before,gt4_phys_mem,0x2000000));CHECK(c->stage>0);
  for(unsigned k=0;k<heard.count;k++){CHECK(heard.effect[k]<80);effects[heard.effect[k]]++;}flow+=heard.refueling!=0;
  race_audio_pit(g->audio,&heard);
  for(unsigned role=0;role<6;role++)if(c->active[role]){const char*label="CRANIUM";float matrix[1][16];CHECK(gt4_license_motion_frames(c->motion,role,&label,1,matrix)==1);for(int i=0;i<16;i++)CHECK(isfinite(matrix[0][i]));}
 }
 printf("native cycle: gun=%u nut=%u tire=%u jack up=%u/down=%u flow=%u change=%u; RAM unchanged\n",effects[0x43],effects[0x3f],effects[0x40],effects[0x42],effects[0x41],flow,effects[0x45]);
 CHECK(effects[0x43] && effects[0x3f] && effects[0x40] && effects[0x41] && flow && effects[0x45]);
 memcpy(gt4_ptr(car),original,sizeof original);free(before);
 plat_video_end_frame();race_game_close(g);plat_video_shutdown();return 0;
}
