/* Whole native races, rather than injected finishes. The original purchase,
   event and payment scripts run on the same menu bridge as the application.
   Headless presentation and a native AI controller are test boundaries. */
#define main purchase_fixture_main
#include "purchase_menu.c"
#undef main
#include "career_event.h"
#include "physics/race_run.h"
#include "physics/car.h"
#include "engine.h"

static int idle_pad(void *user,uint32_t car,uint8_t *command,const uint8_t *event)
{(void)user;(void)car;(void)event;memset(command,0,32);return 1;}
static int idle_wheel(void *user,uint32_t car,int16_t state[2])
{(void)user;(void)car;state[0]=state[1]=1;return 1;}
static int idle_write(void *user,uint32_t car,const uint8_t *out)
{(void)user;(void)car;(void)out;return 1;}

static int buy_event(menu *m,void *user)
{
    (void)user;
    const char *root=ui_root_name(m->ui,menu_active_root(m));
    if(root && !strcmp(root,"BuyCarDialog")){
        menu_send_input(m,MENU_RIGHT);menu_send_input(m,MENU_ACCEPT);
        /* Reproduce the old modal restore that invalidated the caller's
           garage reference. The authored acquisition must then fail. */
        if(getenv("GT4_MODAL_REFRESH_NEGATIVE"))menu_save_refresh(m);
        return 1;
    }
    if(root && !strcmp(root,"GetCourseDialog")){
        ticks(m);menu_send_input(m,MENU_ACCEPT);ticks(m);return 1;
    }
    return 0;
}

#ifndef GT4_SUNDAY_EMBEDDED
int main(int argc,char **argv)
{
    CHECK(argc==2 || (argc==3 && !strcmp(argv[2],"--career-cycle")));
    int cycle=argc==3;const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
    gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
    char directory[180],msg[256];
    snprintf(directory,sizeof directory,"build/career-native/sunday-playable-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
    gt4_save_init(directory);menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);
    menu_set_car_loader(m,transaction_model_loader,NULL);
    menu_set_dialog_presenter(m,purchase_dialog);
    menu_set_event_presenter(m,buy_event,NULL);
    menu_save_capture(m);CHECK(gt4_save_create(1,"Sunday Cup validation"));menu_save_refresh(m);
    ah_vm *vm=m->vm;ah_val ret,arg;ah_obj *game=game_object(m);
    int64_t initial=get(vm,game,"money").u.i;CHECK(initial==10000);
    ah_obj *used=get(vm,m->project,"UsedCarRoot").u.o;
    ah_set_member(vm,used,"current_region",ah_str(vm,"US"));CHECK(enter_named(m,"UsedCarRoot"));ticks(m);
    ui_widget *list=kid(kid(ui_root_widget(m->ui,menu_active_root(m)),"ScrollBox"),"SelectBox");CHECK(list && list->nkids);
    ah_obj *selected=NULL;int selected_row=-1;int64_t cost=0;float power=0;
    for(int i=0;i<list->nkids;i++){
        ah_obj *car=get(vm,widget_module(m,list->kids[i]),"gcar").u.o;ah_val price,ps;
        CHECK(ah_call_member(vm,car,"getUsedPrice",0,NULL,&price)==1);
        CHECK(ah_call_member(vm,car,"getCurrentPower",0,NULL,&ps)==1);
        if(price.u.i<=initial && price.u.i>0 && ah_num(ps)>power){selected=car;selected_row=i;cost=price.u.i;power=(float)ah_num(ps);}
    }
    CHECK(selected);printf("purchase: %s price=%lld power=%.1f\n",ah_cstr(get(vm,selected,"car_code")),(long long)cost,power);fflush(stdout);
    /* Select an affordable car through the authored UsedCarRoot list. */
    for(int i=0;i<selected_row;i++){menu_send_input(m,MENU_DOWN);ticks(m);}
    menu_send_input(m,MENU_ACCEPT);ticks(m);
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"BuyCarRoot"));
    ah_obj *buy=get(vm,m->project,"BuyCarRoot").u.o;
    script_errors=0;ah_set_log(vm,log_script,NULL);
    ah_obj *button=get(vm,get(vm,buy,"Pane").u.o,"Buy").u.o;arg=ah_objv(m->context);
    CHECK(ah_call_member(vm,button,"onActivate",1,&arg,&ret)==1);ticks(m);
    CHECK(!script_errors);
    ah_obj *acquire=get(vm,m->project,"GetCarRoot").u.o;
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"GetCarRoot"));
    ah_val finish[]={arg,ah_int(1)};CHECK(ah_call_member(vm,acquire,"start_phase1",2,finish,&ret)==1);
    if(getenv("GT4_SUNDAY_PLAYABLE_NEGATIVE"))cost++;
    CHECK(get(vm,game,"money").u.i==initial-cost);
    CHECK(gt4_save_load_slot(1));menu_save_refresh(m);game=game_object(m);
    ah_obj *garage=get(vm,game,"garage").u.o;CHECK(get(vm,garage,"count").u.i==1 && get(vm,garage,"riding_index").u.i==0);
    printf("PASS authored purchase, selected car and Auto Save reload: %s\n",directory);fflush(stdout);
    uint32_t rng;CHECK(gt4_career_event_rng_load(argv[1],&rng));
    unsigned races=0,wins=0;uint64_t total_a=0,total_b=0;char unlocked_course[64]="";
    ah_obj *timed=get(vm,m->project,"PresentTimeData").u.o;
    ah_obj *types=get(vm,timed,"type_carlist").u.o,*contents=get(vm,timed,"content_carlist").u.o;
    const char *first_course=NULL;
    for(int i=0;i<ah_array_size(types);i++)if(!strcmp(ah_cstr(ah_array_get(types,i)),"course")){
        first_course=ah_cstr(ah_array_get(contents,i));break;
    }
    CHECK(first_course && strlen(first_course)<sizeof unlocked_course);snprintf(unlocked_course,sizeof unlocked_course,"%s",first_course);
    arg=ah_str(vm,unlocked_course);CHECK(ah_call_member(vm,game,"isCourseAvailable",1,&arg,&ret)>=0 && !ah_truthy(ret));
    for(unsigned round=1;round<99;round++){
        char label[32];snprintf(label,sizeof label,"am_sunday_%04u",cycle && round>5?1:round);uint32_t id;
        load_race_tables(m);if(!gt4_specdb_label(m->race_labels,m->race_labels_size,label,&id))break;
        uint8_t body[0x4b8];CHECK(gt4_game_car_body(vm,get(vm,garage,"riding_car").u.o,body));
        gt4_career_event event;CHECK(gt4_career_event_prepare(&event,argv[1],gt4_game_race_lookup_data(),label,body,0,&rng,msg,sizeof msg));
        gt4_race_run *run=gt4_race_run_open(argv[1],&event.desc,msg,sizeof msg);
        if(!run){fprintf(stderr,"race open: %s\n",msg);return 1;}
        gt4_phys_pad.read=idle_pad;
        gt4_phys_pad.wheel_state=idle_wheel;gt4_phys_pad.write=idle_write;
        /* Test controller: original 003454A8 takeover (mode 5).
           Race results, engine output and opponent strength are untouched. */
        gt4_car_take_over(gt4_race_run_car(run,0),5);
        gt4_race_hud hud;unsigned frames=0;
        while(!gt4_race_run_finished(run) && frames<60*1200){CHECK(gt4_race_run_frame(run));frames++;}
        CHECK(gt4_race_run_hud(run,0,&hud) && hud.finished);
        int position=0;for(int i=1;i<event.desc.cars;i++){gt4_race_hud other;CHECK(gt4_race_run_hud(run,i,&other));position+=other.rank<hud.rank;}
        uint32_t player=gt4_race_run_car(run,0),a,b;uint8_t wear[32],after[0x4b8];
        memcpy(wear,gt4_ptr(player+CAR_ENGINE_WEAR),32);
        a=gt4_career_distance_integer(GT4_F32(player+0x6d4)-GT4_F32(player+0x7b0));
        b=gt4_career_distance_integer(GT4_F32(player+0x7b0));CHECK(a+b>0);
        /* Actual result grid, in stable signed rank order (003B6FD0),
           including each prepared opponent's label/paint/power/weight. */
        menu_quick_race q={0};snprintf(q.race,sizeof q.race,"%s",label);snprintf(q.course,sizeof q.course,"%s",event.desc.course);
        q.finished=1;q.entries=event.desc.cars;q.player=position;
        int order[6],ranks[6];for(int i=0;i<q.entries;i++){
            gt4_race_hud h;CHECK(gt4_race_run_hud(run,i,&h));ranks[i]=h.rank;int j=i;
            while(j && ranks[order[j-1]]>ranks[i]){order[j]=order[j-1];j--;}order[j]=i;
        }
        for(int slot=0;slot<q.entries;slot++){
            int i=order[slot];uint8_t setup[0x178];CHECK(gt4_race_run_setup(run,i,setup));
            const char *car=gt4_specdb_label_for_id(m->car_labels,m->car_labels_size,event.desc.car[i]);CHECK(car);
            snprintf(q.grid[slot].car,sizeof q.grid[slot].car,"%s",car);q.grid[slot].color=(int8_t)setup[0x10];
            uint32_t address=gt4_race_run_car(run,i),params=GT4_U32(address+CAR_PARAMS);
            q.grid[slot].power=(int)gt4_engine_peak_power(params,address+CAR_ENGINE_WEAR);q.grid[slot].weight=(int)GT4_F32(params+PARAMS_MASS)-70;
        }
        CHECK(menu_apply_car_wear(m,wear));CHECK(gt4_game_car_body(vm,get(vm,garage,"riding_car").u.o,after));
        CHECK(!memcmp(after,body,0x498) && !memcmp(after+0x498,wear,32));
        uint32_t award=0,prize=gt4_career_race_prize(event.race_row,position);uint8_t best;
        CHECK(menu_apply_career_result(m,label,1,position,hud.best_lap,0,hud.aspec_points,prize,&best,&award));
        CHECK(award==prize);
        printf("race %s %s: %u frames, position=%d, prize=%u\n",label,event.desc.course,frames,position+1,award);fflush(stdout);
        gt4_race_run_close(run);races++;wins+=position==0;
        /* The original quick-event PrizeRoot pays money, not the result
           recorder. Exercise its authored fast-forward/continue sequence. */
        menu_save_capture(m);menu *quick=menu_open(vol,"quick-event",msg,sizeof msg);CHECK(quick);menu_save_refresh(quick);
        q.prize=award;
        menu_set_quick_race(&q);ah_set_log(quick->vm,log_script,NULL);
        menu_quick_event_result(quick,1,award);CHECK(menu_enter_quick_start(quick));ticks(quick);
        ah_vm *qvm=quick->vm;ah_obj *qp=quick->project;
        int64_t balance=get(vm,game,"money").u.i;
        CHECK(ah_call_member(qvm,qp,"makePrizeFast",0,NULL,&ret)>=0);
        ah_val qcontext=ah_objv(quick->context);
        for(unsigned i=0;i<1000 && get(qvm,qp,"rest_prize").u.i>0;i++)CHECK(ah_call_member(qvm,qp,"onPrizeCheck",1,&qcontext,&ret)>=0);
        int pending=1;
        if(getenv("GT4_CAREER_MILEAGE_NEGATIVE") && races==1)pending=0;
        CHECK(menu_commit_career_distances(m,quick,a,b,&pending) && !pending);
        /* Repeated navigation/checkpoint must not duplicate the mileage. */
        CHECK(menu_commit_career_distances(m,quick,a,b,&pending));total_a+=a;total_b+=b;
        menu_save_capture(quick);menu_save_refresh(m);menu_close(quick);game=game_object(m);garage=get(vm,game,"garage").u.o;
        fprintf(stderr,"payment before=%lld after=%lld award=%u\n",(long long)balance,(long long)get(vm,game,"money").u.i,award);
        CHECK(get(vm,game,"money").u.i==balance+award);
        CHECK(menu_enter_career_return(m));ticks(m);CHECK(gt4_save_capture(vm,game_object(m),1) && gt4_save_store());
        CHECK(enter_named(m,"TopRoot"));ticks(m);CHECK(enter_named(m,"HomeRoot"));ticks(m);
        game=game_object(m);CHECK(get(vm,get(vm,game,"statistics").u.o,"a_spec_distance").u.i==(int64_t)total_a);
        CHECK(get(vm,get(vm,game,"statistics").u.o,"b_spec_distance").u.i==(int64_t)total_b);
        CHECK(ah_call_member(vm,game,"getTotalRaceCount",0,NULL,&ret)>=0 && ret.u.i==races);
        CHECK(get(vm,get(vm,game,"calendar").u.o,"elapsed_days").u.i==(int64_t)races+1);
        menu_save_capture(m);CHECK(gt4_save_store());
        CHECK(gt4_save_load_slot(1));menu_save_refresh(m);game=game_object(m);garage=get(vm,game,"garage").u.o;
        CHECK(get(vm,game,"money").u.i==balance+award);
        CHECK(gt4_game_car_body(vm,get(vm,garage,"riding_car").u.o,after) && !memcmp(after+0x498,wear,32));
        ah_val metres;CHECK(ah_call_member(vm,get(vm,garage,"riding_car").u.o,"getOdometer",0,NULL,&metres)>=0);
        uint32_t expected_metres;memcpy(&expected_metres,wear,4);CHECK((uint32_t)metres.u.i==expected_metres);
        /* Reopen the menu VM too: this cannot pass through stale UI objects. */
        menu_close(m);m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);vm=m->vm;ah_set_log(vm,log_script,NULL);
        menu_set_car_loader(m,transaction_model_loader,NULL);menu_set_dialog_presenter(m,purchase_dialog);
        menu_set_event_presenter(m,buy_event,NULL);
        menu_save_refresh(m);game=game_object(m);garage=get(vm,game,"garage").u.o;
        CHECK(get(vm,game,"money").u.i==balance+award);
        CHECK(get(vm,get(vm,game,"statistics").u.o,"a_spec_distance").u.i==(int64_t)total_a);
        CHECK(get(vm,get(vm,game,"statistics").u.o,"b_spec_distance").u.i==(int64_t)total_b);
        CHECK(gt4_game_car_body(vm,get(vm,garage,"riding_car").u.o,after) && !memcmp(after+0x498,wear,32));
        /* Cache Home before the next attempt: return must initialize it
           again with that attempt's progress, without reopening this VM. */
        CHECK(enter_named(m,"HomeRoot"));ticks(m);
        game=game_object(m);garage=get(vm,game,"garage").u.o;
        arg=ah_str(vm,unlocked_course);CHECK(ah_call_member(vm,game,"isCourseAvailable",1,&arg,&ret)>=0);
        CHECK(!script_errors);
        if(cycle && ah_truthy(ret))break;
    }
    CHECK(races);printf("PASS %u complete native Sunday Cup races, %u wins, authored money payments and save/reload.\n",races,wins);
    if(cycle){CHECK(ah_truthy(ret));printf("PASS continuous career: %llu/%llu A/B metres, live wear, counters/calendar, Home and VM reopen; authored dated unlock %s after %u races.\n",(unsigned long long)total_a,(unsigned long long)total_b,unlocked_course,races);}
    menu_close(m);gt4_save_shutdown();gt4_game_set_car_factory(NULL,NULL);gt4_game_set_stats_reader(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
#endif
