/* Original exit_menu callback supplies next_sequence; native snapshot
   reads the authored event label and the complete equipped garage car. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
#include "career_blob.h"
#include "career_event.h"
static unsigned finishes;
static int finish_boundary(ah_vm *vm,ah_obj *self,void *d,int n,const ah_val *a,ah_val *r)
{(void)vm;(void)self;(void)d;(void)n;(void)a;finishes++;*r=ah_nil();return 0;}
static void run_exit_event(menu *m)
{
 CHECK(m->nevents==1);ah_obj *event=m->events[--m->nevents];ah_val fn,args,result;
 CHECK(ah_get_member(m->vm,event,"function",&fn));CHECK(ah_get_member(m->vm,event,"arguments",&args));
 ah_val values[2]={ah_array_get(args.u.o,0),ah_array_get(args.u.o,1)};
 CHECK(ah_array_size(args.u.o)==2);CHECK(ah_call(m->vm,fn,2,values,&result)>=0);ah_unpin(m->vm,ah_objv(event));
}
int main(int argc,char **argv)
{
 CHECK(argc==3);const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol);
 CHECK(gt4_game_load_disc(argv[1]));uint8_t body[0x4b8];FILE *file=fopen(argv[2],"rb");CHECK(file);
 CHECK(fread(body,1,sizeof body,file)==sizeof body && fgetc(file)==EOF);fclose(file);
 char message[256];menu *m=menu_open(vol,"gtmode",message,sizeof message);CHECK(m);ah_set_log(m->vm,log_script,NULL);
 ah_vm *vm=m->vm;ah_obj *game=game_object(m);ah_val garage;CHECK(ah_get_member(vm,game,"garage",&garage));
 /* Only the authored exit callback/event queue is under test; unrelated
    welcome-screen watchers are outside this request boundary. */
 m->nwatchers=0;
 while(m->nevents)ah_unpin(vm,ah_objv(m->events[--m->nevents]));
 ah_obj *car=ah_module_new(vm,"equipped_fixture",NULL,NULL,NULL);gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body);
 ah_set_member(vm,garage.u.o,"riding_car",ah_objv(car));
 ah_set_member(vm,m->context,"finish",ah_native(vm,"finish",finish_boundary,NULL));
 ah_set_member(vm,m->context,"transition",ah_nil()); /* Presentation is outside this boundary. */
 static const char *sequences[]={"solo_event","championship"};
 menu_career_request snapshot;
 for(unsigned i=0;i<2;i++){
  ah_set_member(vm,game,"race_code",ah_str(vm,i?"am_sunday_0000":"am_sunday_0001"));
  ah_val args[]={ah_objv(m->context),ah_str(vm,sequences[i])},result;
  CHECK(ah_call_member(vm,m->project,"exit_menu",2,args,&result)>=0);run_exit_event(m);CHECK(finishes==i+1 && !errors);
  CHECK(menu_get_career_request(m,&snapshot,message,sizeof message));
  if(getenv("GT4_CAREER_REQUEST_NEGATIVE")&&!i)snapshot.player_body[0x19]^=1;
  CHECK(!memcmp(snapshot.player_body,body,sizeof body));CHECK(snapshot.championship==(int)i);
  CHECK(!strcmp(snapshot.race,i?"am_sunday_0000":"am_sunday_0001"));
  if(!i){gt4_career_event event;uint32_t fixture_rng=17;
   CHECK(gt4_career_event_prepare(&event,argv[1],gt4_game_race_lookup_data(),snapshot.race,snapshot.player_body,snapshot.championship,&fixture_rng,message,sizeof message));
   CHECK(!memcmp(event.player_body,body,sizeof body));CHECK(event.desc.cars==6 && !strcmp(event.desc.course,"r_mini"));}
 }
 ah_obj *wear=ah_array_new(vm);for(unsigned i=0;i<32;i++)ah_array_push(vm,wear,ah_int((body[0x498+i]+i+1)&255));
 ah_set_member(vm,car,"oil_state",ah_objv(wear));CHECK(menu_get_career_request(m,&snapshot,message,sizeof message));
 CHECK(!memcmp(snapshot.player_body,body,0x498));for(unsigned i=0;i<32;i++)CHECK(snapshot.player_body[0x498+i]==((body[0x498+i]+i+1)&255));
 ah_set_member(vm,game,"next_sequence",ah_str(vm,"single_race"));CHECK(!menu_get_career_request(m,&snapshot,message,sizeof message));
 menu_career_request empty={0};CHECK(!memcmp(&snapshot,&empty,sizeof snapshot));
 ah_set_member(vm,game,"next_sequence",ah_str(vm,"solo_event"));ah_set_member(vm,garage.u.o,"riding_car",ah_nil());
 CHECK(!menu_get_career_request(m,&snapshot,message,sizeof message));CHECK(!memcmp(&snapshot,&empty,sizeof snapshot));
 ah_set_member(vm,garage.u.o,"riding_car",ah_objv(car));CHECK(menu_get_career_request(m,&snapshot,message,sizeof message));
 menu_close(m);CHECK(!strcmp(snapshot.race,"am_sunday_0000"));CHECK(!memcmp(snapshot.player_body,body,0x498));
 gt4_vol_close(vol);puts("PASS original exit_menu event/championship routes, equipped car snapshot, wear refresh, invalid route/car rejection and ownership after VM close; Sunday Cup descriptor prepared.");return 0;
}
