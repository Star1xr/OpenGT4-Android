/* Disc-wide prize flow. Results are explicit fixtures, not physical wins.
   Executes gtmode.ad PresentRoot L14090..14146 / get_car L14542..14646. */
#define GT4_SUNDAY_EMBEDDED 1
#include "sunday_playable.c"

int main(int argc,char **argv)
{
    CHECK(argc==2);const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);
    CHECK(vol && gt4_game_load_disc(argv[1]));char msg[256],directory[180];
    snprintf(directory,sizeof directory,"build/career-series/awards-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
    gt4_save_init(directory);menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
    menu_set_car_loader(m,transaction_model_loader,NULL);menu_set_dialog_presenter(m,purchase_dialog);menu_set_event_presenter(m,buy_event,NULL);
    ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);menu_save_capture(m);CHECK(gt4_save_create(1,"All series awards"));load_race_tables(m);
    /* Owned car is a fixture; the authored return path expects one. */
    {ah_obj *garage=get(vm,game_object(m),"garage").u.o;ah_val args[]={ah_str(vm,"ka_01"),ah_int(0)},ret,index=ah_int(0);
        CHECK(ah_call_member(vm,garage,"addNewCar",2,args,&ret)>=0);CHECK(ah_call_member(vm,garage,"setRidingCar",1,&index,&ret)>=0);
        ah_val car=get(vm,garage,"riding_car");CHECK(nat_bind_game_car(vm,NULL,m,1,&car,&ret)>=0);menu_save_capture(m);}
    ah_obj *page=get(vm,m->project,"PresentRoot").u.o,*data=get(vm,m->project,"PresentData").u.o;
    ah_obj *conditions=get(vm,data,"condition_carlist").u.o,*details=get(vm,data,"detail_carlist").u.o;
    unsigned series=0,stages=0,endurance=0,championships=0;
    for(int id=0;id<ah_array_size(conditions);id++){
        if(strcmp(ah_cstr(ah_array_get(conditions,id)),"win"))continue;
        const char *code=ah_cstr(ah_array_get(details,id));CHECK(code && strlen(code)<32);
        /* Explicit return-page fixture, through the script's from_page
           branch (L14493..14498); no event-selection UI is fabricated. */
        ah_set_member(vm,page,"from_page",get(vm,m->project,"HomeRoot"));
        char race[32];unsigned count=0;int champ=gt4_career_championship_label(code,0,race,&count);
        ah_val code_arg=ah_str(vm,code),subraces;
        CHECK(ah_call_member(vm,m->project,"extractSubRaceCode",1,&code_arg,&subraces)>=0);
        unsigned authored_count=(unsigned)ah_array_size(subraces.u.o);CHECK(authored_count);
        ah_val prize_id=ah_int(id),column,ret;CHECK(ah_call_member(vm,page,"makeColumn",1,&prize_id,&column)>=0);
        ah_val eligibility[]={prize_id,column};CHECK(ah_call_member(vm,page,"win",2,eligibility,&ret)>=0 && !ah_truthy(ret));
        ah_obj *game=game_object(m),*garage=get(vm,game,"garage").u.o;int before=ah_array_size(get(vm,garage,"cars").u.o);
        int days=(int)ah_num(get(vm,get(vm,game,"calendar").u.o,"elapsed_days"));
        unsigned rounds=0;int32_t points[6]={0};
        for(unsigned round=0;round<99;round++){
            if(champ){if(round==count)break;CHECK(gt4_career_championship_label(code,round,race,&count));}
            else snprintf(race,sizeof race,"%.*s%02u",(int)strlen(code)-2,code,round+1);
            uint32_t key;if(!gt4_specdb_label(m->race_labels,m->race_labels_size,race,&key))break;
            uint8_t row[151];CHECK(gt4_career_race_row(gt4_game_race_lookup_data(),m->race_labels,m->race_labels_size,m->race_db,m->race_db_size,race,row));
            uint32_t award=0,grand=0;uint8_t best=15;
            int position=getenv("GT4_SERIES_AWARDS_NEGATIVE") && !series?1:0;
            if(champ){uint8_t order[]={0,1,2,3,4,5};gt4_career_championship_points(points,order,row[0x3e],1,(const int8_t *)gt4_game_race_lookup_data()->driver_points);
                int16_t units;memcpy(&units,row+0x2c,2);CHECK(menu_apply_championship_result(m,race,code,1,round+1==count,0,60000,0,20,gt4_career_race_prize(row,0),points,units,&award,&grand));}
            else CHECK(menu_apply_career_result(m,race,1,position,60000,0,20,gt4_career_race_prize(row,position),&best,&award));
            rounds++;stages++;
            CHECK(menu_enter_career_return(m));ticks(m);CHECK(!script_errors);
            ah_obj *list=get(vm,page,"current_list").u.o;int queued=0;for(int i=0;i<ah_array_size(list);i++)queued|=ah_num(ah_array_get(list,i))==id;
            char next[32];snprintf(next,sizeof next,"%.*s%02u",(int)strlen(code)-2,code,round+2);
            int final=champ?round+1==count:!gt4_specdb_label(m->race_labels,m->race_labels_size,next,&key);
            CHECK(queued==final);
        }
        CHECK(rounds==authored_count);ah_obj *list=get(vm,page,"current_list").u.o;int found=0;
        for(int i=0;i<ah_array_size(list);i++)found|=ah_num(ah_array_get(list,i))==id;CHECK(found);
        /* L14466 has scheduled get_car: use the event delivery, not a
           second direct call that would duplicate the prize. */
        CHECK(!script_errors);
        ah_obj *cars=get(vm,get(vm,game_object(m),"garage").u.o,"cars").u.o;CHECK(ah_array_size(cars)==before+1);
        ah_obj *car=ah_array_get(cars,before).u.o;const char *label=ah_cstr(get(vm,car,"car_code"));CHECK(label);
        uint8_t unit[32]={0},expected[0x4b8],actual[0x4b8];int paint=(int)ah_num(get(vm,car,"color_index"));
        CHECK(gt4_career_vehicle_create(factory,label,paint,0,unit,expected));CHECK(gt4_game_car_body(vm,car,actual) && !memcmp(actual,expected,sizeof actual));
        CHECK(ah_num(get(vm,get(vm,game_object(m),"calendar").u.o,"elapsed_days"))==days+rounds+1);
        CHECK(ah_num(get(vm,get(vm,game_object(m),"statistics").u.o,"prize_cars"))==before);
        /* Returned VM values held only by C are not roots across ticks. */
        CHECK(ah_call_member(vm,page,"makeColumn",1,&prize_id,&column)>=0);eligibility[1]=column;
        CHECK(ah_call_member(vm,page,"win",2,eligibility,&ret)>=0 && !ah_truthy(ret));
        /* Finish the authored prize screen before entering another event.
           Remaining achievement prizes are confirmed through the same UI. */
        ah_set_member(vm,page,"from_page",get(vm,m->project,"HomeRoot"));
        for(unsigned confirm=0;confirm<32 && !strcmp(ui_root_name(m->ui,menu_active_root(m)),"PresentRoot");confirm++){
            menu_send_input(m,MENU_ACCEPT);ticks(m);
            char project[64];if(menu_take_project(m,project,sizeof project)){
                /* The final championship also requests the ending movie.
                   Test the hand-over; movie decoding is a separate boundary. */
                CHECK(!strcmp(code,"pr_gtworld_1000") && !strcmp(project,"demo_movie"));
                CHECK(ah_truthy(get(vm,get(vm,game_object(m),"option").u.o,"can_watch_ending")));
                CHECK(enter_named(m,"HomeRoot"));ticks(m);
            }
        }
        CHECK(strcmp(ui_root_name(m->ui,menu_active_root(m)),"PresentRoot"));CHECK(!script_errors);
        menu_save_capture(m);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(m);
        cars=get(vm,get(vm,game_object(m),"garage").u.o,"cars").u.o;CHECK(ah_array_size(cars)>=before+1);
        CHECK(gt4_game_car_body(vm,ah_array_get(cars,before).u.o,actual) && !memcmp(actual,expected,sizeof actual));
        series++;endurance+=!strncmp(code,"en_",3);championships+=champ;printf("award %s stages=%u car=%s\n",code,rounds,label);fflush(stdout);
    }
    CHECK(series && endurance && championships);CHECK(!script_errors);
    printf("PASS %u original series / %u stages / %u championships / %u endurance: partial-win gate, authored delivery, bodies, statistics, date, no duplicate, save/reload. Injected result boundary.\n",series,stages,championships,endurance);
    menu_close(m);gt4_save_shutdown();gt4_game_set_car_factory(NULL,NULL);gt4_game_set_stats_reader(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
