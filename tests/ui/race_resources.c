/* Read-only presentation adapters through a real recorded race.
   Native integration checks; original scalar/sampler proofs are separate. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "photo_travel.h"
#include "save.h"
#include "career_vehicle.h"
#include "miniz.h"
#include "film_fixture.h"
#include "../../src/app/race_audio.c"
#include "../../src/app/race_game.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"race resources %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void frame(menu *m)
{menu_tick(m);menu_animate(m,1.f/60);plat_video_begin_frame();int d,s;ui_draw_root(menu_ui(m),menu_active_root(m),&d,&s);plat_video_end_frame();}
static void ready(race_game *g)
{int status=0;for(int i=0;i<30000 && !status;i++){status=race_game_poll(g);if(!status)SDL_Delay(1);}CHECK(status==1);race_game_update(g,0);}
static void clone(const gt4_replay_file *r,gt4_replay_file *out)
{uint8_t *p;size_t n;CHECK(gt4_replay_pack(r,&p,&n));CHECK(gt4_replay_unpack(p,n,out));free(p);}
static void capture(const char *path)
{int w,h;uint8_t *rgba=plat_sdl_read_pixels(&w,&h);CHECK(rgba);size_t n;void *png=tdefl_write_image_to_png_file_in_memory(rgba,w,h,4,&n);CHECK(png);FILE *f=fopen(path,"wb");CHECK(f && fwrite(png,1,n,f)==n);fclose(f);mz_free(png);free(rgba);}
int main(int argc,char **argv)
{
 CHECK(argc>=4);int negative=argc>4 && !strcmp(argv[4],"--negative"),photo_negative=argc>4 && !strcmp(argv[4],"--photo-negative");int camera_negative=argc>4 && !strcmp(argv[4],"--camera-negative"),film_negative=argc>4 && !strcmp(argv[4],"--film-negative");const char *disc=argv[1];char msg[256];
 CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));SDL_HideWindow(plat_sdl_window());gt4_save_init(argv[2]);
 race_game *g=race_game_open(disc);CHECK(g);race_game_set_time_trial(g,1);race_game_set_driving(g,0,0,0,0);
 CHECK(race_game_start(g,"tsukuba","ka_01",2,NULL,NULL,0,msg,sizeof msg));
 gt4_replay_file a,b,borrowed;gt4_logger_point *points_a=calloc(GT4_LOGGER_POINTS,sizeof *points_a),*points_b=calloc(GT4_LOGGER_POINTS,sizeof *points_b);CHECK(points_a && points_b);
 uint8_t *lap_car=malloc(GT4_GHOST_CAR);CHECK(lap_car);
 for(int i=0;i<1800;i++){
  memset(&command,0,sizeof command);command.device=2;command.throttle=i>650?26000:0;CHECK(gt4_race_run_frame(g->run));logger_sample(g);
  if(i==1000)memcpy(lap_car,gt4_ptr(gt4_race_run_car(g->run,0)),GT4_GHOST_CAR);
 }
 unsigned count_a=g->license_log.count;CHECK(count_a>100);memcpy(points_a,g->license_log.points,count_a*sizeof *points_a);
 CHECK(race_game_replay_export(g,&borrowed,GT4_REPLAY_FULL));clone(&borrowed,&a);
 CHECK(race_game_start(g,"tsukuba","today_g_85",1,NULL,NULL,0,msg,sizeof msg));
 for(int i=0;i<1800;i++){memset(&command,0,sizeof command);command.device=2;command.throttle=i>700?20000:0;CHECK(gt4_race_run_frame(g->run));logger_sample(g);}
 unsigned count_b=g->license_log.count;CHECK(count_b>50);memcpy(points_b,g->license_log.points,count_b*sizeof *points_b);
 CHECK(race_game_replay_export(g,&borrowed,GT4_REPLAY_FULL));clone(&borrowed,&b);
 CHECK(race_game_replay_import(g,&a));g->log_analyzing=1;g->replay_next=1;CHECK(race_game_restart(g,msg,sizeof msg));
 for(size_t i=0;i<a.steps;i++){CHECK(gt4_race_run_frame(g->run));logger_sample(g);}g->log_analyzing=0;
 CHECK(g->license_log.count==count_a && !memcmp(g->license_log.points,points_a,count_a*sizeof *points_a));
 CHECK(race_game_logger_load(g,&b,1,msg,sizeof msg));
 CHECK(g->license_log.count==count_a && !memcmp(g->license_log.points,points_a,count_a*sizeof *points_a));
 CHECK(g->log_b.count==count_b && !memcmp(g->log_b.points,points_b,count_b*sizeof *points_b));
 CHECK(!strcmp(g->last_car,"ka_01") && g->tape_n==a.steps && !memcmp(g->tape,a.tape,a.steps*32));
 menu_logger_source source;CHECK(race_game_logger_source(g,&source));CHECK(!strcmp(source.car[1],"today_g_85") && source.power[1]>0);
 CHECK(race_game_logger_load(g,&b,0,msg,sizeof msg));
 if(negative)g->log_b.points[0].speed_gear^=8;
 CHECK(g->license_log.count==count_b && !memcmp(g->license_log.points,points_b,count_b*sizeof *points_b));
 CHECK(g->log_b.count==count_b && !memcmp(g->log_b.points,points_b,count_b*sizeof *points_b));
 char wrong[32];strcpy(wrong,b.course);strcpy(b.course,"nurburgring");CHECK(!race_game_logger_load(g,&b,1,msg,sizeof msg));strcpy(b.course,wrong);
 gt4_replay_file lap=a;lap.kind=GT4_REPLAY_BEST_LAP;lap.tape=a.tape+1001*32;lap.steps=a.steps-1001;lap.ghost_car=lap_car;lap.ghost_rest=0;lap.ghost_frac=0;lap.best_lap=18000;
 CHECK(race_game_logger_load(g,&lap,0,msg,sizeof msg));CHECK(g->license_log.count>20 && g->license_log.maximum_speed>1);
 CHECK(g->log_b.count==count_b && !memcmp(g->log_b.points,points_b,count_b*sizeof *points_b));
 puts("PASS Logger: different-car A/B, exact loaded telemetry, other bank/tape/setup preserved, wrong-course rejection, stored best-lap start");
 CHECK(race_game_replay(g,msg,sizeof msg));ready(g);
 for(int i=0;i<1000;i++)CHECK(gt4_race_run_frame(g->run));race_game_update(g,0);replay_button(g,2);for(int i=0;i<8;i++)race_game_update(g,STEP);plat_video_set_view(640,480);
 const char *error;gt4_vol *vol=gt4_vol_open(disc,&error);CHECK(vol);menu *m=menu_open(vol,"photo_shoot",msg,sizeof msg);CHECK(m);
 photo_travel *p=photo_travel_open(disc);CHECK(p);float saved[15],after[15];
 int saved_visible=race_view_car_visible(g->view);CHECK(race_view_get_camera(g->view,saved,saved+3,saved+12));uint8_t car_state[GT4_GHOST_CAR];memcpy(car_state,gt4_ptr(gt4_race_run_car(g->run,0)),sizeof car_state);size_t cursor=g->tape_at;
 uint8_t *world_state=malloc(32u*1024*1024);CHECK(world_state);memcpy(world_state,gt4_ptr(0),32u*1024*1024);
 float camera_size[4],camera_distance;CHECK(race_game_photo_camera_data(g,camera_size,&camera_distance));photo_travel_drive_setup(p,camera_size,camera_distance);
 CHECK(photo_travel_drive(p,m,race_game_photo_view(g)));race_game_photo_pause(g,1);
 if(photo_negative)race_game_photo_pause(g,0);
 for(int i=0;i<60;i++){race_game_update(g,1.f/60);frame(m);}
 gt4_photo_mode *state=menu_photo_state(m);CHECK(state->drive && state->ready && state->focus_distance>0);
 CHECK(state->drive_cameras>1 && state->drive_cameras<=64);float first_eye[3];memcpy(first_eye,state->eye,12);
 photo_travel_drive_camera(p,1);CHECK(state->drive_camera==1 && memcmp(first_eye,state->eye,12));
 state->brightness=2;photo_travel_drive_camera(p,-1);CHECK(state->drive_camera==0 && !memcmp(first_eye,state->eye,12));
 photo_travel_drive_camera(p,1);if(camera_negative)state->brightness=0;CHECK(state->brightness==2);photo_travel_drive_camera(p,-1);
 strcpy(state->color_filter,"mature");state->brightness=1;state->aperture=1;
 photo_travel_shoot(p);for(int i=0;i<60;i++)frame(m);CHECK(!strcmp(ui_root_name(menu_ui(m),menu_active_root(m)),"PreviewRoot"));
 menu_send_input(m,MENU_ACCEPT);frame(m);CHECK(*photo_travel_last_saved(p));
 plat_video_begin_frame();int d,s;ui_draw_root(menu_ui(m),menu_active_root(m),&d,&s);capture(argv[3]);plat_video_end_frame();
 int first_w,first_h;uint8_t *slow=plat_png_read(photo_travel_last_saved(p),&first_w,&first_h);CHECK(slow);
 menu_send_input(m,MENU_BACK);for(int i=0;i<60;i++)frame(m);state->shutter=18;
 photo_travel_shoot(p);for(int i=0;i<60;i++)frame(m);menu_send_input(m,MENU_ACCEPT);frame(m);
 int fast_w,fast_h;uint8_t *fast=plat_png_read(photo_travel_last_saved(p),&fast_w,&fast_h);CHECK(fast && first_w==fast_w && first_h==fast_h);
 unsigned different=0;for(size_t i=0;i<(size_t)fast_w*fast_h*4;i++)different+=slow[i]!=fast[i];CHECK(different>0);
 printf("PASS Photo shutter: 1/60 vs 1/4000 changes %u native image channels.\n",different);free(slow);free(fast);
 CHECK(cursor==g->tape_at && !memcmp(car_state,gt4_ptr(gt4_race_run_car(g->run,0)),sizeof car_state));
 /* Actual PhotoSave project: frozen preview, successful native storage,
    and explicit return to the editor without advancing replay state. */
 menu *preview=menu_open(vol,"photo_save",msg,sizeof msg);CHECK(preview);
 CHECK(photo_travel_drive_preview(p,preview));for(int i=0;i<60;i++)frame(preview);
 CHECK(!strcmp(ui_root_name(menu_ui(preview),menu_active_root(preview)),"PreviewRoot"));
 CHECK(menu_photo_state(preview)->image_width>0);
 /* The same authored outUSBStorage operation is bound in PhotoSave. */
 menu_photo_state(preview)->save_requested=1;frame(preview);
 CHECK(*photo_travel_last_saved(p));photo_travel_drive_editor(p);
 for(int k=0;k<5;k++){plat_video_begin_frame();photo_travel_editor_draw(p);photo_travel_feedback(p);plat_video_end_frame();SDL_Delay(90);}
 plat_video_begin_frame();photo_travel_editor_draw(p);photo_travel_feedback(p);char editor_image[1024];snprintf(editor_image,sizeof editor_image,"%s.editor.png",argv[3]);capture(editor_image);plat_video_end_frame();
 CHECK(cursor==g->tape_at);menu_close(preview);
 CHECK(!memcmp(world_state,gt4_ptr(0),32u*1024*1024));free(world_state);
 photo_travel_drive_end(p);race_game_photo_pause(g,0);CHECK(race_view_get_camera(g->view,after,after+3,after+12));CHECK(!memcmp(saved,after,sizeof saved) && saved_visible==race_view_car_visible(g->view));
 CHECK(gt4_race_run_frame(g->run) && g->tape_at==cursor+1);
 /* Photo Drive's GT world (0011ACB4/003EDE50) and mode-zero RACE row,
    rather than an Arcade/Free Run replacement. Keep the solver untouched. */
 gt4_career_vehicle *factory=gt4_career_vehicle_open(disc);CHECK(factory);uint8_t unit[32],body[0x4b8],row[151];
 CHECK(gt4_career_vehicle_create_seeded(factory,"ka_01",2,0,0,unit,body));
 size_t ni,nd;uint8_t *idi=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.idi",&ni),*db=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.dbt",&nd);CHECK(idi && db);
 CHECK(gt4_career_race_row(gt4_game_race_lookup_data(),idi,ni,db,nd,"photo_drive",row));free(idi);free(db);
 uint32_t rng;CHECK(gt4_career_event_rng_load(disc,&rng));gt4_race_desc drive={0};
 strcpy(drive.course,"tsukuba");drive.cars=1;memcpy(&drive.car[0],body,4);drive.owned_car[0]=body;drive.event_row=row;drive.event_rng=&rng;
 CHECK(race_game_start_prepared(g,&drive,msg,sizeof msg));ready(g);CHECK(!g->replaying && race_game_photo_view(g));
 CHECK(GT4_U32(GT4_U32(GT4_U32(gt4_race_run_conductor(g->run))+0x84)+0xd68)==13);
 for(int i=0;i<60;i++)race_game_update(g,STEP);
 CHECK(photo_travel_drive(p,m,race_game_photo_view(g)));race_game_photo_pause(g,1);
 world_state=malloc(32u*1024*1024);CHECK(world_state);memcpy(world_state,gt4_ptr(0),32u*1024*1024);cursor=g->tape_n;
 for(int i=0;i<60;i++){race_game_update(g,STEP);frame(m);}CHECK(!memcmp(world_state,gt4_ptr(0),32u*1024*1024) && cursor==g->tape_n);free(world_state);
 photo_travel_drive_end(p);race_game_photo_pause(g,0);race_game_update(g,STEP);CHECK(g->tape_n>cursor);
 CHECK(race_game_replay(g,msg,sizeof msg));ready(g);CHECK(race_game_photo_view(g));
 gt4_career_vehicle_close(factory);puts("PASS Photo Drive GT session, live photo freeze/resume and recorded replay");
 CHECK(photo_travel_load(p,m,"ss_gion","ka_01",2,NULL,NULL));
 for(int i=0;i<ui_root_count(menu_ui(m));i++)if(!strcmp(ui_root_name(menu_ui(m),i),"ShootRoot")){ui_widget *map=ui_widget_child(ui_root_widget(menu_ui(m),i),"Map");CHECK(map && map->visible);}
 puts("PASS Photo Drive: borrowed race, optical PNG, frozen full 32 MiB world/tape cursor with temporal exposure, exact camera restore/resume, Travel map re-entry. Pixels remain A.");
 /* Imported film follows the actual PhotoSave preview and storage path. */
 uint8_t film_bytes[2048];size_t film_size=film_fixture(film_bytes,sizeof film_bytes);
 char film_path[1200];snprintf(film_path,sizeof film_path,"%s/fixture.ogt4film",argv[2]);
 CHECK(gt4_save_write(film_path,film_bytes,film_size));
 preview=menu_open(vol,"photo_save",msg,sizeof msg);CHECK(preview);
 plat_video_set_view(640,480);CHECK(photo_travel_develop(p,preview,film_path,NULL,NULL));
 for(int k=0;k<5;k++)frame(preview);
 CHECK(menu_photo_state(preview)->image_width>0 && menu_photo_state(preview)->image_height>0);
 if(film_negative)menu_photo_state(preview)->image_width++;
 CHECK(menu_photo_state(preview)->image_width*3==menu_photo_state(preview)->image_height*4);
 plat_video_begin_frame();ui_draw_root(menu_ui(preview),menu_active_root(preview),&d,&s);char film_image[1200];snprintf(film_image,sizeof film_image,"%s.film.png",argv[3]);capture(film_image);plat_video_end_frame();
 menu_photo_state(preview)->save_requested=1;frame(preview);CHECK(*photo_travel_last_saved(p));
 int fw,fh;uint8_t *developed=plat_png_read(photo_travel_last_saved(p),&fw,&fh);CHECK(developed && fw>0 && fh>0);free(developed);
 photo_travel_drive_end(p);menu_close(preview);
 puts("PASS imported film: decode, resident scene, authored preview, PNG save and clean return.");
 photo_travel_close(p);menu_close(m);gt4_vol_close(vol);gt4_replay_free(&a);gt4_replay_free(&b);free(lap_car);free(points_a);free(points_b);race_game_close(g);gt4_save_shutdown();plat_video_shutdown();return 0;
}
