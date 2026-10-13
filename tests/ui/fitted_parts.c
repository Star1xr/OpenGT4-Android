/* Fitted original assets through garage, P1/P2, ghost, replay and cached photo.
   Integration proof, not a claim of PCSX2 pixel equivalence. */
#include <SDL.h>
#include "plat_sdl.h"
#include "car_model.h"
#include "photo_travel.h"
#include "career_vehicle.h"
#include "career_blob.h"
#include "../../src/app/race_audio.c"
#include "../../src/app/race_game.c"
#include "../../src/core/menu.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"fitted parts %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void ready(race_game*g){int s=0;for(int i=0;i<30000 && !s;i++){s=race_game_poll(g);if(!s)SDL_Delay(1);}CHECK(s==1);}
/* Worker/main floating-point contexts can round the same asset differently.
   Declared absolute tolerance 1e-5 for positions and UVs, exact counts. */
static int equal_mesh(const gt4_model*a,const gt4_model*b){
 if(!a || !b || a->count!=b->count || a->ntextures!=b->ntextures)return 0;
 for(int i=0;i<a->count;i++)for(int j=0;j<3;j++){
  for(int k=0;k<3;k++)if(fabsf(a->triangles[i].v[j].p[k]-b->triangles[i].v[j].p[k])>1e-5f)return 0;
  for(int k=0;k<2;k++)if(fabsf(a->triangles[i].v[j].uv[k]-b->triangles[i].v[j].uv[k])>1e-5f)return 0;
 }
 return 1;
}
static void match(const car_model*a,const car_model*b){CHECK(a && b && equal_mesh(a->car,b->car));for(int i=0;i<4;i++)CHECK(equal_mesh(a->wheel[i],b->wheel[i]));}
static ah_val field(ah_vm*v,ah_obj*o,const char*n){ah_val a=ah_nil();CHECK(ah_get_member(v,o,n,&a));return a;}
int main(int argc,char**argv){
 CHECK(argc>=3);int replay_negative=argc>3 && !strcmp(argv[3],"--replay-negative");int negative=argc>3 && !strcmp(argv[3],"--negative"),photo_negative=argc>3 && !strcmp(argv[3],"--photo-negative");
 CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));SDL_HideWindow(plat_sdl_window());gt4_save_init(argv[2]);
 const char*err;gt4_vol*vol=gt4_vol_open(argv[1],&err);CHECK(vol && gt4_game_load_disc(argv[1]));
 gt4_career_vehicle*f=gt4_career_vehicle_open(argv[1]);CHECK(f);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,f);
 char msg[256];menu*source=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(source);ah_vm*vm=source->vm;
 ah_obj*garage=field(vm,game_object(source),"garage").u.o;ah_val result,args[]={ah_str(vm,"ka_01"),ah_int(2)};
 CHECK(ah_call_member(vm,garage,"addNewCar",2,args,&result)==1);ah_val index=ah_int(0);CHECK(ah_call_member(vm,garage,"setRidingCar",1,&index,&result)==1);
 ah_obj*owned=field(vm,garage,"riding_car").u.o;
 ah_val wheel[]={ah_str(vm,gt4_game_part_data()->wheel_categories[0]),ah_str(vm,"brdg001w")},wing=ah_str(vm,"p001");
 CHECK(ah_call_member(vm,owned,"setTireCategoryWheel",2,wheel,&result)==1);
 CHECK(ah_call_member(vm,owned,"setWing",1,&wing,&result)==1);
 uint8_t body[0x4b8];CHECK(gt4_game_car_body(vm,owned,body));char w[32],s[32];CHECK(menu_photo_parts(source,w,s));CHECK(!strcmp(w,"brdg001w") && !strcmp(s,"p001"));
 car_model_tables*t=car_model_tables_open(vol);CHECK(t);
 car_model*stock=car_model_load_race(vol,t,"ka_01",2,2,0),*fitted=car_model_load_race_parts(vol,t,"ka_01",2,2,0,w,s);
 car_model*rim_only=car_model_load_race_parts(vol,t,"ka_01",2,2,0,w,"");CHECK(stock && fitted && rim_only);
 CHECK(!equal_mesh(stock->wheel[0],fitted->wheel[0]));CHECK(!equal_mesh(rim_only->car,fitted->car));
 race_game*g=race_game_open(argv[1]);CHECK(g);const char*others[]={"ka_01"};int colors[]={2};
 for(int mask=1;mask<=3;mask++){
  gt4_race_desc d={0};d.tire_2p=1;for(int i=0;i<2;i++)if(mask&(1<<i))d.owned_car[i]=body;
  race_game_set_split(g,&d);CHECK(race_game_start(g,"grandvalley_east","ka_01",2,others,colors,1,msg,sizeof msg));
  for(int retry=0;retry<2;retry++){
   if(retry)CHECK(race_game_restart(g,msg,sizeof msg));ready(g);
   for(int i=0;i<2;i++)match(race_view_car_model(g->view,i),(mask&(1<<i)) && !(negative && i==1)?fitted:stock);
  }
 }
 race_game_set_split(g,NULL);race_game_set_arcade_owned(g,body);
 for(int trial=0;trial<2;trial++){
  race_game_set_time_trial(g,trial);
  CHECK(race_game_start(g,"grandvalley_east","ka_01",2,trial?NULL:others,trial?NULL:colors,trial?0:1,msg,sizeof msg));ready(g);
  match(race_view_car_model(g->view,0),fitted);match(race_view_car_model(g->view,1),trial?fitted:stock);
  for(int i=0;i<20;i++)race_game_update(g,STEP);
  gt4_replay_file borrowed,stored;uint8_t*bytes;size_t size;
  CHECK(race_game_replay_export(g,&borrowed,GT4_REPLAY_FULL));CHECK(gt4_replay_pack(&borrowed,&bytes,&size));CHECK(gt4_replay_unpack(bytes,size,&stored));free(bytes);
  CHECK(race_game_replay(g,msg,sizeof msg));ready(g);match(race_view_car_model(g->view,0),fitted);
  if(replay_negative)stored.owned_on[0]=0;
  CHECK(race_game_replay_import(g,&stored));CHECK(race_game_replay(g,msg,sizeof msg));ready(g);match(race_view_car_model(g->view,0),fitted);gt4_replay_free(&stored);
 }
 menu*editor=menu_open(vol,"photo_shoot",msg,sizeof msg);CHECK(editor);photo_travel*p=photo_travel_open(argv[1]);CHECK(p);
 CHECK(photo_travel_drive(p,editor,race_game_photo_view(g)));match(photo_travel_car_model(p),fitted);photo_travel_drive_end(p);
 uint8_t row[151];size_t ni,nd;uint8_t*idi=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.idi",&ni),*db=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.dbt",&nd);CHECK(idi && db);
 CHECK(gt4_career_race_row(gt4_game_race_lookup_data(),idi,ni,db,nd,"photo_drive",row));free(idi);free(db);uint32_t rng;CHECK(gt4_career_event_rng_load(argv[1],&rng));
 race_game_set_time_trial(g,0);gt4_race_desc prepared={0};strcpy(prepared.course,"grandvalley_east");prepared.cars=1;memcpy(&prepared.car[0],body,4);prepared.owned_car[0]=body;prepared.event_row=row;prepared.event_rng=&rng;
 CHECK(race_game_start_prepared(g,&prepared,msg,sizeof msg));ready(g);
 car_model*single=car_model_load_race_parts(vol,t,"ka_01",2,1,0,w,s);CHECK(single);match(race_view_car_model(g->view,0),single);car_model_free(single);
 car_model*photo=car_model_load_parts(vol,t,"ka_01",2,w,s),*photo_stock=car_model_load(vol,t,"ka_01",2);CHECK(photo && photo_stock);
 /* Same destination becomes explicit car_code after first load. It must
    never override the garage body from the source on re-entry. */
 for(int visit=0;visit<2;visit++){
  CHECK(menu_photo_parts(source,w,s));
  CHECK(photo_travel_load_from(p,editor,photo_negative && visit?editor:source,"ss_gion","ka_01",2,NULL,NULL));
  /* Inspect loaded geometry, not just the labels queued for the worker. */
  match(photo_travel_car_model(p),photo);
 }
 menu_photo_set_selection(source,"ss_gion","ka_01",2);CHECK(photo_travel_load_from(p,editor,source,"ss_gion","ka_01",2,NULL,NULL));match(photo_travel_car_model(p),photo_stock);
 puts("PASS fitted wheel/wing: actual original meshes, P1/P2 owned/stock mixtures, Retry, TT ghost, replay, Photo Travel cached re-entry and catalog reset. Pixels A.");
 photo_travel_close(p);menu_close(editor);race_game_close(g);car_model_free(photo);car_model_free(photo_stock);car_model_free(stock);car_model_free(fitted);car_model_free(rim_only);car_model_tables_close(t);
 menu_close(source);gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(f);gt4_vol_close(vol);gt4_save_shutdown();plat_video_shutdown();return 0;
}
