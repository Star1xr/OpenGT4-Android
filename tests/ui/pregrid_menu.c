/* Original quick-arcade Adhoc + mproject + US RT03 from the user's disc.
   No generated EE code runs here. Headless platform records draw ordering. */
#include "../../src/core/menu.c"
#include "gpb.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pregrid menu line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
struct plat_texture{int dummy;};
static unsigned quads,scenes,errors,icon_quads;
static int record_glyphs;static float glyph_left,glyph_right,glyph_top,glyph_bottom;
plat_texture *plat_texture_create(int w,int h,const uint8_t *p){(void)w;(void)h;(void)p;return calloc(1,sizeof(plat_texture));}
void plat_texture_destroy(plat_texture *t){free(t);}
void plat_draw_quad4(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4]){(void)u;(void)v;(void)a;(void)b;(void)c;
    if(t && y==360 && x>=56 && x<=512){CHECK(w==72 && h==68);icon_quads++;}quads++;}
void plat_draw_solid_quad(const float xy[4][2],const uint8_t c[4]){(void)xy;(void)c;}
void plat_draw_quad(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4]){(void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;quads++;}
void plat_draw_quad_blend(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4],float f){(void)f;plat_draw_quad(t,x,y,w,h,u,v,a,b,c);}
void plat_set_clip(float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;}
void plat_draw_glyph(const plat_texture*t,float x,float y,float w,float h,float b,float s,float u,float v,float u1,float v1,const uint8_t c[4]){(void)t;(void)b;(void)s;(void)u;(void)v;(void)u1;(void)v1;(void)c;if(record_glyphs){glyph_left=fminf(glyph_left,x);glyph_right=fmaxf(glyph_right,x+w);glyph_top=fminf(glyph_top,y);glyph_bottom=fmaxf(glyph_bottom,y+h);}quads++;}
void plat_draw_rotated(const plat_texture*t,float x,float y,float w,float h,float cx,float cy,float a,const uint8_t c[4]){(void)t;(void)x;(void)y;(void)w;(void)h;(void)cx;(void)cy;(void)a;(void)c;quads++;}
int plat_dialog(const char *title,const char *text,int confirm){(void)title;(void)text;return confirm?1:0;}
int plat_audio_play(const int16_t*p,size_t n,int r,int c){(void)p;(void)n;(void)r;(void)c;return 1;}
static void log_script(void *user,const char *s){(void)user;if(strstr(s,"error:") || strstr(s,"undefined") || strstr(s,"call of nil") || strstr(s,"call of module")){errors++;fprintf(stderr,"%s\n",s);}}
static int scene(void *user,float x,float y,float w,float h,float a)
{(void)user;CHECK(!quads && !scenes);CHECK(x==0 && y==0 && w==640 && h==480 && a==1);scenes++;return 1;}
static ui_widget *child(ui_widget *w,const char *name){ui_widget *v=ui_widget_child(w,name);CHECK(v);return v;}
static void advance(menu *m){for(int i=0;i<60;i++){menu_tick(m);menu_animate(m,1.f/60);}}
static int trophy_draws;
static tex1_image *trophy_reference;static int trophy_reference_count;
static void *trophy_create(const uint8_t *data,size_t size){CHECK(data && size>16);return (void *)1;}
static void trophy_destroy(void *mesh){CHECK(mesh==(void *)1);}
static void trophy_draw(void *mesh,const mp_node *node,const float angles[3],float x,float y,float w,float h,const tex1_image *env)
{
 (void)angles;(void)x;(void)y;CHECK(mesh==(void *)1 && w>0 && h>0);
 if(getenv("GT4_PRIZE_ENV_NEGATIVE"))env=NULL;
 CHECK(env && env->rgba && env->width>0 && env->height>0);
 CHECK(env->width==trophy_reference[0].width && env->height==trophy_reference[0].height);
 CHECK(!memcmp(env->rgba,trophy_reference[0].rgba,(size_t)env->width*env->height*4));
 CHECK(mp_number(node,"l1_enable",0)==1);double diffuse[4];CHECK(mp_numbers(node,"l1_dif",diffuse,4)==4);
 CHECK(diffuse[0]==90 && diffuse[1]==90 && diffuse[2]==90);trophy_draws++;
}
/* All authored RootWindow pages, including popups omitted by Root suffix scans. */
static void inventory(gt4_vol *vol,int negative)
{
    static const struct {const char *project,*pages;} cases[]={
        {"quick","LicenseRoot,RecordsRoot,StoreHouse,GuidePage,PasscodePopup"},
        {"quick-arcade","ArcadeRoot,QuickTunePopup"},
        {"quick-championship","ChampionshipRoot,AfterRoot,PrizeRoot"},
        {"quick-event","RaceEventRoot,AfterRoot,PrizeRoot"},
        {"quick-freerun","FreeRunRoot,RankingRoot,PasscodePopup"},
        {"quick-license","LicenseRoot,RankingRoot,PasscodePopup"},
        {"quick-mission","MissionRoot,PrizeRoot,AfterRoot"},
        {"quick-mt","QuickRoot,RankingRoot"},{"quick-practice","PracticeRoot"},
        {"quick-tt","TimeTrialRoot,QuickTunePopup,RankingRoot,PasscodePopup"}};
    unsigned total=0;
    for(unsigned i=0;i<sizeof cases/sizeof *cases;i++){
        char path[128],err[256],actual[512]="";size_t size;
        snprintf(path,sizeof path,"/projects/%s/%s.mproject",cases[i].project,cases[i].project);
        uint8_t *data=gt4_vol_read(vol,path,&size);CHECK(data);
        mp_node *project=mp_parse((const char *)data,size,err,sizeof err);CHECK(project);free(data);
        const mp_node *const *pages;int n=mp_children(project,&pages);CHECK(n>0);
        for(int j=0;j<n;j++)if(!strcmp(pages[j]->name,"RootWindow")){
            const char *name=mp_string(pages[j],"name");CHECK(name);
            if(*actual)strcat(actual,",");CHECK(strlen(actual)+strlen(name)<sizeof actual);strcat(actual,name);total++;
        }
        CHECK(!strcmp(actual,negative && i==9?"ArcadeRoot":cases[i].pages));
        printf("%s: %s\n",cases[i].project,actual);mp_free(project);
    }
    CHECK(total==29);
}
/* Execute the disc's actual page selectors. Prizes are supplied as boundary
   state; no career reward logic or race physics is reimplemented by this test. */
static void page_selectors(gt4_vol *vol,int negative)
{
    static const char *projects[]={"quick-event","quick-mission","quick-championship"};
    char msg[256];
    for(int i=0;i<3;i++){
        ah_val qval,result;menu_quick_race work={0};
        strcpy(work.race,"am_sunday_0001");strcpy(work.course,"grandvalley_east");work.entries=6;work.player=5;
        for(int j=0;j<6;j++){strcpy(work.grid[j].car,"ka_01");work.grid[j].power=61;work.grid[j].weight=965;}
        menu_set_quick_race(&work);
        menu *m=menu_open(vol,projects[i],msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);
        for(int state=0;state<(i==2?4:3);state++){
            work.entries=6;
            for(int j=0;j<6;j++)work.grid[j].time=j==5?0x157529ffu:123456u+j*789;
            work.finished=state!=0;work.prize=state==2;work.grand_prize=state==3;
            work.laps=state&1?-30:5;work.course_length=1234+state;
            work.bspec=state&1;menu_set_quick_race(&work);
            CHECK(nat_quick_work(m->vm,NULL,NULL,0,NULL,&qval)==0);
            ah_val field;CHECK(ah_get_member(m->vm,qval.u.o,"Laps",&field));
            CHECK(ah_num(field)==work.laps);
            CHECK(ah_get_member(m->vm,qval.u.o,"CourseLength",&field));
            if(getenv("GT4_QUICK_METADATA_NEGATIVE"))field=ah_int(work.course_length+1);
            CHECK(ah_num(field)==work.course_length);
            for(int j=0;j<6;j++){ah_val index=ah_int(j),time;
                CHECK(ah_call_member(m->vm,qval.u.o,"getGridTime",1,&index,&time)>=0);
                if(getenv("GT4_GRID_TIME_BRIDGE_NEGATIVE"))time=ah_int(work.grid[j].time+1);
                CHECK((uint32_t)ah_num(time)==work.grid[j].time);
            }
            CHECK(ah_get_member(m->vm,qval.u.o,"raceLabel",&field));
            CHECK(!strcmp(ah_cstr(field),work.race));
            CHECK(ah_get_member(m->vm,qval.u.o,"IsBSpec",&field));CHECK(ah_num(field)==work.bspec);
            CHECK(ah_call_member(m->vm,m->project,"getStartPage",0,NULL,&result)>=0);
            int root;
            const char *expected=state==0?(i==0?"RaceEventRoot":i==1?"MissionRoot":"ChampionshipRoot"):state==1?"AfterRoot":"PrizeRoot";
            for(root=0;root<ui_root_count(m->ui);root++)if(!strcmp(ui_root_name(m->ui,root),expected))break;
            if(negative && i==0 && state==0)root=1;
            if(ah_as_obj(result,AO_MODULE)!=widget_module(m,ui_root_widget(m->ui,root)))fprintf(stderr,"selector %s state %d returned %s, expected %s\n",projects[i],state,ah_module_name(ah_as_obj(result,AO_MODULE)),expected);
            CHECK(ah_as_obj(result,AO_MODULE)==widget_module(m,ui_root_widget(m->ui,root)));
            CHECK(!errors);
            if(i==0 && state==2){
                for(int pkg=0;pkg<ui_root_count(m->ui) && !trophy_reference;pkg++){
                    char path[256];snprintf(path,sizeof path,"/projects/quick-event/US/%s.gpb",ui_root_name(m->ui,pkg));
                    size_t size=0,n=0;uint8_t *bytes=gt4_vol_read(vol,path,&size);
                    uint8_t *raw=bytes?gpb_load(bytes,size,"image/arcade/car/env0.png",&n):NULL;
                    if(raw)trophy_reference_count=tex1_decode(raw,n,&trophy_reference);free(raw);free(bytes);
                }
                CHECK(trophy_reference_count>0);
                ah_set_member(m->vm,game_object(m),"money",ah_int(10000));
                ui_set_model_renderer(m->ui,(ui_model_renderer){trophy_create,trophy_draw,trophy_destroy});
                CHECK(menu_enter_quick_start(m));CHECK(m->nevents>0);
                ui_prepare_root(m->ui,menu_active_root(m),NULL,NULL);
                /* Loading reveal advances only presentation. It must retain
                   quick-event.ad L899's pending doAction until menu_tick. */
                int pending=m->nevents;for(int frame=0;frame<60;frame++)menu_animate(m,1.f/60);
                if(getenv("GT4_PRIZE_LOADING_NEGATIVE"))menu_tick(m);
                CHECK(m->nevents==pending);trophy_draws=0;ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);CHECK(trophy_draws==1);quads=scenes=0;
                tex1_free(trophy_reference,trophy_reference_count);trophy_reference=NULL;
            }
            if(i!=1 && state<=1){
                /* The production return invokes this same authored selector,
                   rather than forcing the first root. Exercise actual buttons. */
                CHECK(menu_enter_quick_start(m));advance(m);
                CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),expected));
                for(int j=0;j<10;j++){menu_send_input(m,MENU_LEFT);advance(m);}
                for(int j=0;j<10 && strcmp(menu_focus_name(m),"Option");j++){menu_send_input(m,MENU_RIGHT);advance(m);}
                if(strcmp(menu_focus_name(m),"Option"))fprintf(stderr,"option route %s state %d focus %s\n",projects[i],state,menu_focus_name(m));
                CHECK(!strcmp(menu_focus_name(m),"Option"));
                menu_send_input(m,MENU_ACCEPT);advance(m);char command[64];
                CHECK(menu_take_quick_command(m,command,sizeof command) && !strcmp(command,"COMMAND_OPTION"));
                unsigned previous_serial=m->pcm_serial;
                CHECK(!*menu_pcm_request(m,NULL));
                CHECK(menu_enter_quick_start(m));advance(m);
                if(getenv("GT4_QUICK_PCM_RETURN_NEGATIVE"))m->pcm_group[0]=0;
                CHECK(!strcmp(menu_pcm_request(m,NULL),"default_quick") && m->pcm_serial>previous_serial);
                const char *return_expected=getenv("GT4_CAREER_OPTION_RETURN_NEGATIVE")?"wrong returned root":expected;
                CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),return_expected));
                CHECK(!strcmp(menu_focus_name(m),"Option") && !errors);
                if(i==0 && state==1){
                    ui_widget *after=ui_root_widget(m->ui,menu_active_root(m));
                    CHECK(!strcmp(ui_widget_text(m->ui,child(child(after,"Grid1"),"Time")),"2'03.456"));
                    CHECK(!strcmp(ui_widget_text(m->ui,child(child(after,"Grid6"),"Time")),"\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f"));
                }
            }
        }
        /* Reuse the same VM after a completed session: the original
           selector must return to its pre-race page, including replay
           being unavailable until another race has actually run. */
        memset(&work,0,sizeof work);menu_set_quick_race(&work);
        CHECK(ah_call_member(m->vm,m->project,"getStartPage",0,NULL,&result)>=0);
        CHECK(ah_as_obj(result,AO_MODULE)==widget_module(m,ui_root_widget(m->ui,0)));
        CHECK(nat_quick_work(m->vm,NULL,NULL,0,NULL,&qval)==0);
        ah_val replay;CHECK(ah_get_member(m->vm,qval.u.o,"canReplay",&replay));CHECK(!ah_truthy(replay));
        menu_close(m);
    }
}
/* Disc-to-Adhoc bridge: rows remain the original SpecDB bytes. The
   independent function oracle tests the stage/selection semantics. */
static void setting_disc_bridge(gt4_vol *vol,const char *iso){
 gt4_career_part_data loaded;CHECK(gt4_career_part_data_load(iso,&loaded));gt4_game_set_part_data(&loaded);
 char msg[128];menu *m=menu_open(vol,"quick-tt",msg,sizeof msg);CHECK(m);
 const gt4_career_part_data *rules=gt4_game_part_data();CHECK(rules);
 unsigned compared=0;
 for(unsigned table=0;table<37;table++){
  if(table>=32 && table!=36)continue;
  char path[160];snprintf(path,sizeof path,"/specdb/GT4_US2560/%s.dbt",rules->table_names[table]);
  size_t size=0;uint8_t *db=gt4_vol_read(vol,path,&size);CHECK(db && size>=16);
  unsigned count=car_u32(db+8);CHECK(count);
  for(unsigned edge=0;edge<2;edge++){
   unsigned index=edge?count-1:0;uint32_t id=car_u32(db+16+index*8);uint64_t key=((uint64_t)table<<32)|id;
   int64_t signed_key;memcpy(&signed_key,&key,8);ah_val arg=ah_int(signed_key),ret;
   CHECK(nat_setting_part_row(m->vm,NULL,m,1,&arg,&ret)==0);
   ah_obj *bytes=ah_as_obj(ret,AO_ARRAY);uint8_t expected[256];int width=gt4_specdb_row_at(db,size,index,expected,sizeof expected);
   CHECK(bytes && width>0 && ah_array_size(bytes)==width);
   for(int j=0;j<width;j++){
    ah_val actual=ah_array_get(bytes,j);
    if(getenv("GT4_SETTING_DISC_NEGATIVE") && !compared && !j)actual.u.i^=1;
    CHECK(actual.u.i==expected[j]);
   }compared++;ah_gc(m->vm);
  }free(db);
 }
 /* A missing high-word identifier must not silently become row zero. */
 ah_val missing=ah_int(UINT64_C(0x100010000)),ret;
 CHECK(nat_setting_part_row(m->vm,NULL,m,1,&missing,&ret)==0 && ret.t==AV_NIL);
 load_car_tables(m);unsigned cars=car_u32(m->car_db+8),queries=0;
 ah_obj *car=ah_module_new(m->vm,"part fixture",NULL,NULL,NULL);ah_pin(m->vm,ah_objv(car));
 for(unsigned index=0;index<cars;index+=cars/16?cars/16:1){
  uint32_t id=car_u32(m->car_db+16+index*8);ah_set_member(m->vm,car,"spec_id",ah_int(id));
  char path[160];snprintf(path,sizeof path,"/specdb/GT4_US2560/CARS/%016llX",(unsigned long long)id);
  size_t size=0;uint8_t *list=gt4_vol_read(vol,path,&size);
  uint8_t body[0x4b8]={0};unsigned slot=index%3;memcpy(body+0x488,&slot,4);uint64_t car_key=id;memcpy(body+slot*0x178,&car_key,8);
  CHECK(gt4_career_blob_set(m->vm,car,"full_car_ps2",body,sizeof body));gt4_game_bind_car(m->vm,car);
  ah_set_member(m->vm,car,"_getSettingPartRow",ah_bind(m->vm,car,ah_native(m->vm,"disc rows",nat_setting_part_row,m)));
  uint8_t generic[256],stock[256];CHECK(gt4_specdb_row_at(m->car_db,m->car_db_size,index,generic,sizeof generic)>=8);
  uint64_t default_key;memcpy(&default_key,generic,8);int expected_default=0;
  if(default_key!=UINT64_MAX){
   unsigned table=(unsigned)(default_key>>32);CHECK(table<39);
   char default_path[160];snprintf(default_path,sizeof default_path,"/specdb/GT4_US2560/%s.dbt",rules->table_names[table]);
   size_t n=0;uint8_t *db=gt4_vol_read(vol,default_path,&n);
   int width=gt4_specdb_row(db,n,(uint32_t)default_key,stock,sizeof stock);free(db);
   if(width>=0xa0){uint64_t muffler;memcpy(&muffler,stock+0x98,8);expected_default=muffler==UINT64_MAX;}
  }
  ah_val fn,arg=ah_int(20);CHECK(ah_get_member(m->vm,car,"isDefaultNoParts",&fn));CHECK(ah_call(m->vm,fn,1,&arg,&ret)==0);
  if(getenv("GT4_DEFAULT_DISC_NEGATIVE") && !queries)ret.u.i^=1;
  CHECK(ret.u.i==expected_default);
  for(int type=-1;type<=32;type++)for(int stage=-1;stage<4;stage++){
   ah_val args[]={ah_int(type),ah_int(stage)};int expected=gt4_career_part_key(list,size,type,stage)!=UINT64_MAX;
   CHECK(nat_owned_parts_exist(m->vm,car,m,2,args,&ret)==0);
   if(getenv("GT4_PART_EXIST_NEGATIVE") && !queries)ret.u.i^=1;
   CHECK(ret.u.i==expected);queries++;
  }free(list);ah_gc(m->vm);
 }
 menu_close(m);printf("PASS setting disc bridge: %u original rows across 33 namespaces and %u car part existence queries.\n",compared,queries);
}
static void machine_return(gt4_vol *vol)
{
    menu_quick_race q={0};strcpy(q.course,"testcourse");strcpy(q.race,"mt400_testcourse");
    q.entries=1;q.laps=1;strcpy(q.grid[0].car,"ka_01");menu_set_quick_race(&q);
    menu_set_quick_best(0x157529ffu);char error[256];menu *m=menu_open(vol,"quick-mt",error,sizeof error);CHECK(m);
    ah_set_log(m->vm,log_script,NULL);CHECK(menu_enter_quick_start(m));advance(m);
    ui_widget *root=ui_root_widget(m->ui,menu_active_root(m)),*buttons=child(root,"Buttons");
    CHECK(!child(buttons,"Replay")->visible && !child(buttons,"SaveReplay")->visible);
    menu_set_quick_best(23456);menu_set_quick_completed(m,1);
    if(getenv("GT4_MT_RETURN_NEGATIVE"))menu_enter(m,0);else CHECK(menu_enter_quick_start(m));
    advance(m);CHECK(child(buttons,"Replay")->visible && child(buttons,"SaveReplay")->visible);
    CHECK(!strcmp(ui_widget_text(m->ui,child(root,"Sessionbest_Time")),"0'23.456"));
    strcpy(q.race,"mtmax_testcourse");menu_set_quick_race(&q);menu_set_quick_best_speed(18000);
    ah_val option;CHECK(ah_get_member(m->vm,game_object(m),"option",&option));
    for(int mph=0;mph<2;mph++){
        ah_set_member(m->vm,option.u.o,"unit_velocity",ah_str(m->vm,mph?"MPH":"KMPH"));
        CHECK(menu_enter_quick_start(m));advance(m);
        CHECK(!strcmp(ui_widget_text(m->ui,child(root,"Sessionbest_Time")),mph?"111.85 mph":"180.00 km/h"));
    }
    menu_set_quick_best_speed(0);CHECK(menu_enter_quick_start(m));advance(m);
    CHECK(!strcmp(ui_widget_text(m->ui,child(root,"Sessionbest_Time")),"---- mph"));
    CHECK(!errors);menu_close(m);puts("PASS quick-mt return refreshes session best and replay/save buttons.");
}
int main(int argc,char **argv)
{
    CHECK(argc==2 || (argc==3 && (!strcmp(argv[2],"--negative") || !strcmp(argv[2],"--inventory-negative") || !strcmp(argv[2],"--tt-negative") || !strcmp(argv[2],"--selector-negative"))));
    const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol);
    setting_disc_bridge(vol,argv[1]);
    page_selectors(vol,argc==3 && !strcmp(argv[2],"--selector-negative"));
    machine_return(vol);
    /* quick-license onInitialize itself owns startStreamEx. Reenter the
       cached examination after its stream was stopped for the race/demo. */
    {
        menu_quick_race exam={0};strcpy(exam.race,"l0b0001");strcpy(exam.course,"gymkhana");
        exam.entries=1;exam.license=1;strcpy(exam.grid[0].car,"demio_j_99");menu_set_quick_race(&exam);
        char message[256];menu *license=menu_open(vol,"quick-license",message,sizeof message);CHECK(license);
        ah_set_log(license->vm,log_script,NULL);menu_quick_event_context(license,"l0b0001");
        CHECK(menu_enter_quick_start(license));advance(license);
        CHECK(menu_pcm_request(license,NULL) && !strcmp(menu_pcm_request(license,NULL),"default_quick"));
        unsigned serial=license->pcm_serial;ah_val ret;
        CHECK(nat_quick_stream(license->vm,NULL,(void *)1,0,NULL,&ret)==0);
        CHECK(!*menu_pcm_request(license,NULL));
        CHECK(menu_enter_quick_start(license));advance(license);
        CHECK(!strcmp(menu_pcm_request(license,NULL),"default_quick") && license->pcm_serial>serial && !errors);
        menu_close(license);
    }
    inventory(vol,argc==3 && !strcmp(argv[2],"--inventory-negative"));
    menu_quick_race q={0};strcpy(q.course,"grandvalley_east");q.entries=6;q.player=5;q.tire=8;q.dry=1;
    for(int i=0;i<6;i++){strcpy(q.grid[i].car,"ka_01");q.grid[i].power=61+i;q.grid[i].weight=965;}
    strcpy(q.grid[2].car,"celica_2000gt_four_86");
    strcpy(q.grid[4].car,"karmann_ghia_type1_1500_68");
    strcpy(q.race,"de_audi_tt_0301");q.laps=5;q.course_length=1234;
    menu_set_quick_race(&q);char msg[256];
    menu *champ=menu_open(vol,"quick-championship",msg,sizeof msg);CHECK(champ);
    menu_quick_event_context(champ,"de_audi_tt_0301");
    ah_set_log(champ->vm,log_script,NULL);menu_enter(champ,0);advance(champ);CHECK(!errors);
    for(int i=0;i<8 && strcmp(menu_focus_name(champ),"Setting");i++){menu_send_input(champ,MENU_RIGHT);advance(champ);}
    CHECK(!strcmp(menu_focus_name(champ),"Setting"));
    menu_send_input(champ,MENU_ACCEPT);advance(champ);
    char settings_command[64];CHECK(menu_take_quick_command(champ,settings_command,sizeof settings_command));
    if(getenv("GT4_CHAMPIONSHIP_SETTINGS_NEGATIVE"))strcpy(settings_command,"COMMAND_OPTION");
    CHECK(!strcmp(settings_command,"COMMAND_SETTINGS") && !errors);
    menu_enter(champ,0);advance(champ);CHECK(!errors && !strcmp(menu_focus_name(champ),"Setting"));
    menu_close(champ);
    menu *m=menu_open(vol,"quick-arcade",msg,sizeof msg);CHECK(m);
    ah_set_log(m->vm,log_script,NULL);menu_enter(m,0);advance(m);CHECK(!errors);
    ui_widget *root=ui_root_widget(m->ui,0),*buttons=child(root,"Buttons"),*tip=child(child(root,"ToolTip"),"tip");
    CHECK(root->nkids && !strcmp(root->kids[0]->type,"SceneViewFace"));
    /* Authored setIconPosition (quick-arcade.ad L201..232): order and spacing. */
    const char *names[]={"Start","QuickTune","Option","Exit"};
    for(int i=0;i<4;i++){ui_widget *w=child(buttons,names[i]);CHECK(w->visible && w->w==72 && w->x==170+i*76);}
    CHECK(!child(buttons,"Spectator")->visible && !child(buttons,"Replay")->visible && !child(buttons,"SaveReplay")->visible);
    for(int i=1;i<=6;i++){char name[20];snprintf(name,sizeof name,"Grid%d",i);CHECK(fabsf(child(root,name)->x-40)<.5f && child(root,name)->y==152+(i-1)*32);}
    CHECK(!strcmp(menu_focus_name(m),"Start"));
    CHECK(!strcmp(ui_widget_text(m->ui,tip),argc==3 && !strcmp(argv[2],"--negative")?"wrong tooltip":"A-Spec=Driver's mode"));
    CHECK(menu_pcm_request(m,NULL) && !strcmp(menu_pcm_request(m,NULL),"default_quick"));
    ui_widget *long_name=child(child(root,"Grid3"),"CarName");
    CHECK(long_name->scale_x==.875f && long_name->letter_spacing==0);
    CHECK(strstr(ui_widget_text(m->ui,long_name),"...")!=NULL);
    ui_set_scene_renderer(m->ui,scene,NULL);ui_draw_root(m->ui,0,NULL,NULL);CHECK(scenes==1 && quads>0 && icon_quads==4);
    menu_send_input(m,MENU_RIGHT);advance(m);
    menu_send_input(m,MENU_ACCEPT);advance(m);
    CHECK(menu_active_root(m)==1 && !errors);menu_send_input(m,MENU_BACK);advance(m);
    CHECK(menu_active_root(m)==0 && !errors);
    CHECK(!strcmp(menu_focus_name(m),"QuickTune"));
    menu_send_input(m,MENU_LEFT);advance(m);
    for(int i=1;i<4;i++){
        menu_send_input(m,MENU_RIGHT);advance(m);CHECK(!strcmp(menu_focus_name(m),names[i]));
        char key[96];snprintf(key,sizeof key,"TOOLTIP|%s",mp_string(child(buttons,names[i])->src,"tooltip_key"));
        const char *original=ui_translate(m->ui,0,NULL,key);CHECK(original && !strcmp(ui_widget_text(m->ui,tip),original));
    }
    menu_send_input(m,MENU_LEFT);advance(m);CHECK(!strcmp(menu_focus_name(m),"Option"));
    menu_send_input(m,MENU_ACCEPT);advance(m);char command[64];CHECK(menu_take_quick_command(m,command,sizeof command) && !strcmp(command,"COMMAND_OPTION"));CHECK(!errors);
    menu_set_quick_completed(m,1);menu_enter(m,0);advance(m);
    buttons=child(ui_root_widget(m->ui,0),"Buttons");
    const char *after[]={"Start","Replay","SaveReplay","QuickTune","Option","Exit"};
    for(int i=0;i<6;i++){ui_widget *w=child(buttons,after[i]);CHECK(w->visible && w->x==94+i*76);}
    CHECK(!child(buttons,"Spectator")->visible && !errors);
    menu_set_quick_completed(m,0);menu_enter(m,0);advance(m);
    CHECK(!child(buttons,"Replay")->visible && !child(buttons,"SaveReplay")->visible);
    menu_close(m);
    m=menu_open(vol,"quick-arcade",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);menu_enter(m,0);advance(m);
    menu_send_input(m,MENU_ACCEPT);advance(m);CHECK(menu_take_quick_command(m,command,sizeof command) && !strcmp(command,"COMMAND_START"));CHECK(!errors);
    menu_close(m);
    q.entries=1;q.player=0;q.can_replay=argc==3 && !strcmp(argv[2],"--tt-negative");menu_set_quick_race(&q);
    m=menu_open(vol,"quick-tt",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);menu_enter(m,0);advance(m);
    CHECK(!errors);CHECK(!strcmp(ui_root_name(menu_ui(m),0),"TimeTrialRoot"));
    buttons=child(ui_root_widget(m->ui,0),"Buttons");
    CHECK(!child(buttons,"Replay")->visible && !child(buttons,"SaveReplay")->visible);
    CHECK(!child(buttons,"SaveGhost")->visible && !child(buttons,"Spectator")->visible);
    CHECK(child(buttons,"Ranking")->visible && child(buttons,"Logger")->visible && child(buttons,"LoadGhost")->visible);
    menu_set_quick_completed(m,1);menu_enter(m,0);advance(m);CHECK(!errors);
    CHECK(child(buttons,"Replay")->visible && child(buttons,"SaveReplay")->visible);
    menu_set_quick_completed(m,0);menu_enter(m,0);advance(m);
    CHECK(!child(buttons,"Replay")->visible && !child(buttons,"SaveReplay")->visible);
    CHECK(!strcmp(menu_focus_name(m),"Start"));
    menu_send_input(m,MENU_RIGHT);advance(m);menu_send_input(m,MENU_RIGHT);advance(m);
    CHECK(!strcmp(menu_focus_name(m),"QuickTune"));menu_send_input(m,MENU_ACCEPT);advance(m);
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"QuickTunePopup") && !errors);
    menu_send_input(m,MENU_BACK);advance(m);CHECK(menu_active_root(m)==0 && !errors);
    while(strcmp(menu_focus_name(m),"Option")){menu_send_input(m,MENU_RIGHT);advance(m);}
    menu_send_input(m,MENU_ACCEPT);advance(m);CHECK(menu_take_quick_command(m,command,sizeof command) && !strcmp(command,"COMMAND_OPTION"));CHECK(!errors);
    ah_obj *game=game_object(m);ah_val result;
    ah_set_member(m->vm,game,"next_sequence",ah_str(m->vm,"time_trial"));
    CHECK(menu_race_is_time_trial(m));CHECK(nat_finish(m->vm,NULL,m,0,NULL,&result)==0);
    CHECK(menu_take_race_request(m));
    ah_set_member(m->vm,game,"next_sequence",ah_str(m->vm,"single_race"));CHECK(!menu_race_is_time_trial(m));
    menu_close(m);gt4_vol_close(vol);
    puts("PASS 29 authored pages; Time Trial own page and replay states; original pregrid script: four pre-race / six post-race icons, disc tooltips, music, scene-before-grid ordering, Option and Start commands; no script errors.");return 0;
}
