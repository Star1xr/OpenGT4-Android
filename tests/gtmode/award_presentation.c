/* Authored certificate/prize Syncs, move actors, CarModel feedback and OK ink.
   No physical exam completion is claimed: medal gates are fixture inputs. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
#include "career_vehicle.h"
static ah_val member(menu *m,ah_obj *o,const char *key){ah_val v;CHECK(ah_get_member(m->vm,o,key,&v));return v;}
static int stage,paper_mode;static unsigned frames,audio_requests,moving_frames;
static ui_widget *paper,*car,*ok;
static void audio_request(menu *m,void *user){(void)user;CHECK(!strcmp(menu_pcm_request(m,NULL),"get_car"));audio_requests++;}
static void wait_frames(menu *m,float seconds,void *user){
 (void)user;CHECK(audio_requests && !ok->visible);
 CHECK(seconds==(paper_mode?(stage?5.f:3.f):(stage==0?4.f:stage==1?1.f:3.f)));
 float expected_x=paper_mode?paper->x:0,expected[3];if(!paper_mode)memcpy(expected,car->car_position,sizeof expected);
 for(int i=0;i<(int)lroundf(seconds*60);i++){
  if(paper_mode){expected_x=stage?gt4_move_value(expected_x,150,.2f):640;}
  else if(car->visible && car->car_motion==1)gt4_service_position(expected,car->car_target_position,car->car_position_ratio);
  float before=paper_mode?paper->x:car->car_position[2];
  if(!getenv("GT4_AWARD_ANIMATION_NEGATIVE"))menu_animate(m,1.f/60);
  int d=0,s=0;ui_draw_root(m->ui,menu_active_root(m),&d,&s);
  if(paper_mode)CHECK(fabsf(paper->x-expected_x)<.0001f);
  else if(car->visible){CHECK(!memcmp(expected,car->car_position,sizeof expected));CHECK(car->car_motion==1 && car->car_position_easing);}
  if(before!=(paper_mode?paper->x:car->car_position[2]))moving_frames++;
  CHECK(!ok->visible);frames++;
 }
 stage++;
}
static void check_ok(menu *m,ui_widget *pane,ui_widget *button){
 ui_widget *root=ui_root_widget(m->ui,menu_active_root(m));
 for(int i=0;i<root->nkids;i++)root->kids[i]->visible=root->kids[i]==pane;
 for(int i=0;i<pane->nkids;i++)pane->kids[i]->visible=pane->kids[i]==button;
 if(pane==paper){root->kids[1]->visible=1;}
 glyph_left=glyph_top=1e9f;glyph_right=glyph_bottom=-1e9f;record_glyphs=1;
 int d=0,s=0;ui_draw_root(m->ui,menu_active_root(m),&d,&s);record_glyphs=0;
 ui_widget *bg=child(button,"bg");float x=0,y=0;for(ui_widget *ancestor=button;ancestor;ancestor=ancestor->parent){x+=ancestor->x;y+=ancestor->y;}
 float cx=x+bg->x+(bg->w>0?bg->w:button->w)*.5f,cy=y+bg->y+(bg->h>0?bg->h:button->h)*.5f;
 fprintf(stderr,"OK bounds %.3f %.3f %.3f %.3f expected %.3f %.3f\n",glyph_left,glyph_right,glyph_top,glyph_bottom,cx,cy);
 if(getenv("GT4_AWARD_OK_NEGATIVE"))cy+=4;
 CHECK(glyph_right>glyph_left && fabsf((glyph_left+glyph_right)*.5f-cx)<.51f && fabsf((glyph_top+glyph_bottom)*.5f-cy)<.51f);
}
int main(int argc,char **argv){
 CHECK(argc==2 || argc==4);gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol && gt4_game_load_disc(argv[1]));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 char dir[220],error[256];snprintf(dir,sizeof dir,"build/career-native/award-presentation-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
 gt4_save_init(argc==4?argv[3]:dir);menu *m=menu_open(vol,"gtmode",error,sizeof error);CHECK(m);ah_set_log(m->vm,log_script,NULL);menu_save_capture(m);CHECK(gt4_save_create(1,"Presentation test"));
 if(argc==4){
  CHECK(!strcmp(argv[2],"--seed"));for(int n=1;n<=16;n++){char code[32];snprintf(code,sizeof code,"l0b%04d",n);gt4_career_license_attempts visit;CHECK(menu_license_begin(m,code,&visit));CHECK(menu_license_attempt(m,code,&visit,4,60000,0x060a07eau));CHECK(menu_license_leave(m,code,&visit));}
  menu_save_capture(m);CHECK(gt4_save_store());puts("PASS pending B certificate fixture saved.");return 0;
 }
 ah_obj *page=member(m,m->project,"PresentRoot").u.o,*data=member(m,m->project,"PresentData").u.o;
 ah_obj *kinds=member(m,data,"type_carlist").u.o,*details=member(m,data,"detail_carlist").u.o;int paper_id=-1,car_id=-1;
 for(int i=0;i<ah_array_size(kinds);i++)if(!strcmp(ah_cstr(ah_array_get(details,i)),"l0b")){if(!strcmp(ah_cstr(ah_array_get(kinds,i)),"paper"))paper_id=i;else if(car_id<0)car_id=i;}
 CHECK(paper_id>=0 && car_id>=0);ah_obj *list=ah_array_new(m->vm);ah_array_push(m->vm,list,ah_int(paper_id));ah_set_member(m->vm,page,"current_list",ah_objv(list));
 int root=-1;for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),"PresentRoot"))root=i;CHECK(root>=0);
 menu_enter(m,root);m->nevents=0;
 ui_widget *p=ui_root_widget(m->ui,root),*paper_pane=child(p,"PaperPane"),*car_pane=child(p,"CarPane");paper=child(paper_pane,"Paper");car=child(car_pane,"CarFace");ok=child(paper,"OK");
 paper_pane->visible=1;car_pane->visible=0;paper_mode=1;menu_set_presenter(m,wait_frames,NULL);menu_set_audio_preparer(m,audio_request,NULL);
 ah_val args[]={ah_objv(m->context),ah_str(m->vm,"l0b")},ret;
 CHECK(ah_call_member(m->vm,member(m,page,"PaperPane").u.o,"get_paper",2,args,&ret)>=0 && !errors);
 CHECK(stage==2 && frames==480 && moving_frames>1 && ok->visible && fabsf(paper->x-150)<.001f);check_ok(m,paper,ok);
 for(int i=0;i<p->nkids;i++)p->kids[i]->visible=1;
 for(int i=0;i<car_pane->nkids;i++)car_pane->kids[i]->visible=1;
 car_pane->visible=1;paper_pane->visible=0;paper_mode=0;stage=0;frames=moving_frames=0;ok=child(car_pane,"OK");
 ah_val id=ah_int(car_id),column;CHECK(ah_call_member(m->vm,page,"makeColumn",1,&id,&column)>=0);
 ah_val delivery[]={ah_objv(m->context),ah_array_get(column.u.o,0),ah_array_get(column.u.o,1),ah_array_get(column.u.o,2),ah_array_get(column.u.o,3),ah_array_get(column.u.o,4)};
 CHECK(ah_call_member(m->vm,member(m,page,"CarPane").u.o,"get_car",6,delivery,&ret)>=0 && !errors);
 CHECK(stage==3 && frames==480 && moving_frames>1 && ok->visible && audio_requests==2);check_ok(m,car_pane,ok);
 puts("PASS authored certificate 3+5s/ratio .2, car 4+1+3s/position .025, PCM requested before Sync, both OK labels centred.");
 menu_close(m);gt4_save_shutdown();gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
