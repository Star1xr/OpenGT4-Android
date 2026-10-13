/* Authored gtmode.ad L705..778: cached Home statistics versus a freshly
   loaded original project over the same persisted race records. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
static ah_val calculate(menu *m){ah_val out;CHECK(ah_call_member(m->vm,m->project,"getRaceEventRecordStatistics",0,NULL,&out)>=0);CHECK(ah_array_size(ah_as_obj(out,AO_ARRAY))==2);return out;}
int main(int argc,char **argv){
 CHECK(argc>=2);const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&err);CHECK(vol && gt4_game_load_disc(argv[1]));
 char directory[180],msg[256];snprintf(directory,sizeof directory,"build/career-native/home-progress-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);
 menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);ah_val initial=calculate(m);CHECK(ah_num(ah_array_get(initial.u.o,0))==0 && ah_num(ah_array_get(initial.u.o,1))==0);
 menu_save_capture(m);CHECK(gt4_save_create(1,"Home test"));
 for(unsigned i=1;i<=3;i++){
  char race[32];snprintf(race,sizeof race,"am_sunday_%04u",i);uint8_t best=15;uint32_t award=0;
  CHECK(menu_apply_career_result(m,race,1,0,60000,0,(int)(20*i),600,&best,&award));
  menu *fresh=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(fresh);menu_save_refresh(fresh);ah_val reference=calculate(fresh);
  CHECK(ah_num(ah_array_get(reference.u.o,0))>0 && ah_num(ah_array_get(reference.u.o,1))==10*i*(i+1));
  if(argc<3 || strcmp(argv[2],"--negative"))menu_save_refresh(m);
  ah_val actual=calculate(m);
  CHECK(ah_num(ah_array_get(actual.u.o,0))==ah_num(ah_array_get(reference.u.o,0)));
  CHECK(ah_num(ah_array_get(actual.u.o,1))==ah_num(ah_array_get(reference.u.o,1)));
  menu_close(fresh);
 }
 menu_close(m);gt4_save_shutdown();gt4_vol_close(vol);puts("PASS cached Home vs fresh original script, three Sunday Cup wins");return 0;
}
