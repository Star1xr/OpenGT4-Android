/* Disc-backed GT Mode series audit and complete native-race validation.
   Player ownership/controller are explicit fixture boundaries. */
#define GT4_SUNDAY_EMBEDDED 1
#include "sunday_playable.c"
#include "career_event.h"
#include "physics/race_run.h"
#include "physics/car.h"
#include "engine.h"
#ifdef GT4_SERIES_PACE_FIXTURE
#include "../physics/driver_fixture.h"
#endif

static int series_pad(void *user,uint32_t car,uint8_t *cmd,const uint8_t *event)
{
    idle_pad(user,car,cmd,event);cmd[1]=1;cmd[3]=event[3];
    /* Test strategy: request a stop below 10 L using the original pad
       event (003F19E0 case 6). This threshold is fixture input, not a
       game rule or a changed fuel parameter. Crew/tyres/fuel remain native. */
    if(GT4_F32(car+0x5d0)>0 && GT4_F32(car+0x5d4)<10){cmd[0x14]=6;cmd[0x15]=1;}
    /* Optional test-driver strategy: request fresh tyres at the second
       authored wear band (00356EF0, session+0x10, wheel+0xAC). This is
       an input decision, not an alteration to wear or the game's AI. */
    if(getenv("GT4_SERIES_PIT_WEAR")){
        uint32_t session=GT4_U32(car+4)+4,layout=gt4_car_layout(car);
        unsigned band=0x10;
#ifdef GT4_SERIES_PACE_FIXTURE
        const char *which=getenv("GT4_SERIES_PIT_BAND");if(which)band=(unsigned)atoi(which)*8;
#endif
        if(GT4_U8(session+0x68))for(unsigned i=0;i<GT4_U8(layout+LAYOUT_WHEELS);i++)
            if(GT4_F32(gt4_car_wheel(car,i)+0xac)>=GT4_F32(session+band)){
                cmd[0x14]=6;cmd[0x15]=1;
            }
    }
#ifdef GT4_SERIES_PACE_FIXTURE
    /* Test input: the authored controller command 003F19E0 case 4,
       index 4 of the five entries initialized by 003F1790. Never write
       the coefficients in 006220B0/006220C8 directly. Pit requests win. */
    const char *level=getenv("GT4_SERIES_PACE");int pace=level?atoi(level):4;
    if(!cmd[0x14] && GT4_U8(car+0x14+0x6d)!=pace){cmd[0x14]=4;cmd[0x15]=(uint8_t)pace;}
    /* 003F19E0 case 5 toggles passing; use its pad event, never poke
       model flags. This remains an explicit test-driver decision. */
    if(!cmd[0x14] && getenv("GT4_SERIES_OVERTAKE") && !GT4_U8(car+0x14+0x6e)){
        cmd[0x14]=5;cmd[0x15]=1;
    }
#endif
    return 1;
}

static int catalog(menu *m)
{
    load_race_tables(m);ah_vm *vm=m->vm;ah_obj *data=get(vm,m->project,"PresentData").u.o;
    const char *keys[]={"condition_carlist","detail_carlist","type_carlist","content_carlist","content2_carlist"};
    ah_obj *columns[5];for(unsigned c=0;c<5;c++){columns[c]=get(vm,data,keys[c]).u.o;CHECK(columns[c]);}
    for(int i=0;i<ah_array_size(columns[0]);i++){
        printf("present %d",i);for(unsigned c=0;c<5;c++){
            ah_val v=ah_array_get(columns[c],i);const char *s=ah_cstr(v);printf(" | %s",s?s:"(non-string)");
        }putchar('\n');
    }
    unsigned physical=0,timed=0;
    for(unsigned i=0;i<car_u32(m->race_labels+4);i++){
        const char *label=gt4_specdb_label_for_id(m->race_labels,m->race_labels_size,car_u32(m->race_labels+20+i*8));
        if(!label)continue;uint8_t row[151];
        CHECK(gt4_career_race_row(gt4_game_race_lookup_data(),m->race_labels,m->race_labels_size,m->race_db,m->race_db_size,label,row));
        int time=gt4_career_race_field(row,GT4_RACE_LIMIT_TIME);
        printf("race %s cars=%u laps=%u time=%d license=%u tire=%u type=%u\n",label,row[0x3e],row[0x43],time,row[0x45],row[0x3f],row[0x40]);
        if(row[0x3e]){physical++;timed+=time>0;}
    }
    printf("catalog active=%u timed=%u\n",physical,timed);return 0;
}

static int owned_fixture_parts(ah_vm *vm,menu *m,ah_val selected,const char *parts,const char *car_label)
{
    ah_val ret;
    if(parts && *parts){
        /* Owned upgrade fixtures, through 00140608 -> 0043F470
           (ownership at body+48C), 001403A0 and 0013FF38
           (getParts/changeParts/endSetting). Types/stages are explicit
           test input; records/defaults still come from the user's disc.
           This does not bypass event admission or edit solver parameters. */
        CHECK(ah_call_member(vm,selected.u.o,"beginSetting",0,NULL,&ret)>=0);
        for(const char *p=parts;p && *p;){
            char *end;long type=strtol(p,&end,10);CHECK(end!=p && *end=='=');
            p=end+1;long stage=strtol(p,&end,10);CHECK(end!=p && (!*end || *end==','));
            ah_val fixture[]={ah_int(type),ah_int(stage)};
            CHECK(ah_call_member(vm,selected.u.o,"getParts",2,fixture,&ret)>=0);
            CHECK(ah_call_member(vm,selected.u.o,"changeParts",2,fixture,&ret)>=0);
            CHECK(ah_call_member(vm,selected.u.o,"setupPartsBuffer",0,NULL,&ret)>=0);
            CHECK(ah_call_member(vm,selected.u.o,"getEquipedParts",1,fixture,&ret)>=0 && ah_num(ret)==stage);
            printf("owned part fixture car=%s type=%ld stage=%ld\n",car_label,type,stage);
            p=*end?end+1:NULL;
        }
        CHECK(ah_call_member(vm,selected.u.o,"endSetting",0,NULL,&ret)>=0);menu_save_capture(m);
    }
    return 0;
}

static int physical(const char *disc,gt4_vol *vol,menu *m,const char *series,const char *car_label,int driver,int require_prize)
{
#ifdef GT4_SERIES_PACE_FIXTURE
    const char *pace=getenv("GT4_SERIES_PACE");CHECK(!pace || (atoi(pace)>=0 && atoi(pace)<=4));
    const char *band=getenv("GT4_SERIES_PIT_BAND");CHECK(!band || (atoi(band)>=1 && atoi(band)<=3));
#endif
    int controller_kind=0;
#ifdef GT4_SERIES_PACE_FIXTURE
    controller_kind=getenv("GT4_SERIES_CONTROLLER_KIND")?atoi(getenv("GT4_SERIES_CONTROLLER_KIND")):0;
    CHECK(controller_kind>=0 && controller_kind<=2);
    printf("test controller kind=%d pace=%d; original selection/input fixture\n",controller_kind,pace?atoi(pace):4);fflush(stdout);
#endif
    char directory[180],msg[256];snprintf(directory,sizeof directory,"build/career-series/save-%s-%llu-%llu",series,(unsigned long long)time(NULL),(unsigned long long)clock());
    gt4_save_init(directory);menu_save_capture(m);CHECK(gt4_save_create(1,"Physical series"));menu_save_refresh(m);
    gt4_career_vehicle *factory=gt4_career_vehicle_open(disc);CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
    gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears,factory);
    menu_set_car_loader(m,transaction_model_loader,NULL);menu_set_dialog_presenter(m,purchase_dialog);menu_set_event_presenter(m,buy_event,NULL);
    ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);ah_obj *game=game_object(m),*garage=get(vm,game,"garage").u.o;
    ah_val args[]={ah_str(vm,car_label),ah_int(0)},ret,index=ah_int(0);
    CHECK(ah_call_member(vm,garage,"addNewCar",2,args,&ret)>=0);
    CHECK(ah_call_member(vm,garage,"setRidingCar",1,&index,&ret)>=0);
    ah_val selected=get(vm,garage,"riding_car");CHECK(nat_bind_game_car(vm,NULL,m,1,&selected,&ret)>=0);menu_save_capture(m);
    const char *parts=getenv("GT4_SERIES_PARTS");
    CHECK(!owned_fixture_parts(vm,m,selected,parts,car_label));
    unsigned owned_fixtures=1;char round_car[96];
    load_race_tables(m);uint32_t rng;CHECK(gt4_career_event_rng_load(disc,&rng));
    char label[32];unsigned count=0,races=0,wins=0;int championship=gt4_career_championship_label(series,0,label,&count);
    gt4_career_event event;gt4_race_desc retained={0};uint8_t owned[0x4b8];int32_t points[6]={0};
    ah_obj *present=get(vm,m->project,"PresentRoot").u.o,*data=get(vm,m->project,"PresentData").u.o;
    ah_obj *details=get(vm,data,"detail_carlist").u.o;int prize_id=-1;
    for(int i=0;i<ah_array_size(details);i++)if(!strcmp(ah_cstr(ah_array_get(details,i)),series)){prize_id=i;break;}CHECK(prize_id>=0);
    for(unsigned round=0;round<99;round++){
        if(championship){if(round==count)break;CHECK(gt4_career_championship_label(series,round,label,&count));}
        else{size_t n=strlen(series);CHECK(n>2 && n<sizeof label);snprintf(label,sizeof label,"%.*s%02u",(int)n-2,series,round+1);}
        uint32_t id;if(!gt4_specdb_label(m->race_labels,m->race_labels_size,label,&id))break;
#ifdef GT4_SERIES_PACE_FIXTURE
        const char *cars=getenv("GT4_SERIES_ROUND_CARS"),*round_parts=getenv("GT4_SERIES_ROUND_PARTS");
        if(cars){
            /* Independent event races permit choosing another owned car.
               Use the garage's original add/select/setting calls; a
               championship keeps its entered car throughout. */
            CHECK(!championship && round_parts);const char *p=cars,*q=round_parts;
            for(unsigned j=0;j<round;j++){p=strchr(p,',');q=strchr(q,'|');CHECK(p && q);p++;q++;}
            char previous_car[96];snprintf(previous_car,sizeof previous_car,"%s",car_label);
            size_t n=strcspn(p,",");CHECK(n && n<sizeof round_car);memcpy(round_car,p,n);round_car[n]=0;
            game=game_object(m);garage=get(vm,game,"garage").u.o;
            selected=get(vm,garage,"riding_car");
            if(strcmp(round_car,previous_car)){
                ah_val add[]={ah_str(vm,round_car),ah_int(0)};
                CHECK(ah_call_member(vm,garage,"addNewCar",2,add,&ret)>=0);
                ah_val riding=ah_int(owned_fixtures++);CHECK(ah_call_member(vm,garage,"setRidingCar",1,&riding,&ret)>=0);
                selected=get(vm,garage,"riding_car");CHECK(nat_bind_game_car(vm,NULL,m,1,&selected,&ret)>=0);
            }
            car_label=round_car;char installed[256];n=strcspn(q,"|");CHECK(n<sizeof installed);memcpy(installed,q,n);installed[n]=0;
            CHECK(!owned_fixture_parts(vm,m,selected,installed,car_label));
        }
        /* Controller commands may be chosen between championship rounds,
           as separate test inputs; never change the car or opponents. */
        const char *controllers=getenv("GT4_SERIES_ROUND_CONTROLLERS");
        if(controllers){const char *p=controllers;for(unsigned j=0;j<round;j++){p=strchr(p,',');CHECK(p);p++;}
            char *end;long kind=strtol(p,&end,10);CHECK(end!=p && (!*end || *end==',') && kind>=0 && kind<=2);controller_kind=(int)kind;
            printf("round %u test controller kind=%d\n",round+1,controller_kind);}
#endif
        game=game_object(m);garage=get(vm,game,"garage").u.o;selected=get(vm,garage,"riding_car");
        ah_val reg_args[]={selected,ah_str(vm,label)},reg;
        CHECK(ah_call_member(vm,m->project,"getViolatedRegulatoin",2,reg_args,&reg)>=0);
        if(ah_truthy(get(vm,reg.u.o,"violated"))){fprintf(stderr,"illegal fixture car %s for %s\n",car_label,label);return 1;}
        CHECK(gt4_game_car_body(vm,selected.u.o,owned));
        if(championship && round){retained.owned_car[0]=owned;CHECK(gt4_career_championship_round(&event,disc,gt4_game_race_lookup_data(),label,&retained,msg,sizeof msg));}
        else CHECK(gt4_career_event_prepare(&event,disc,gt4_game_race_lookup_data(),label,owned,championship,&rng,msg,sizeof msg));
#ifdef GT4_SERIES_PACE_FIXTURE
        /* Test driving options: allow the equipped aids, as the entry's
           authored defaults do (003B4B38 +0x11C/+0x124 ->3450/3454).
           Their actual levels stay in the original owned-car setup. */
        if(getenv("GT4_SERIES_AIDS")){event.desc.assist_asm=1;event.desc.assist_tcs=1;}
#endif
        gt4_race_run *run=gt4_race_run_open(disc,&event.desc,msg,sizeof msg);
        if(!run){fprintf(stderr,"%s: %s\n",label,msg);return 1;}
        /* 00441248 copies the active owned setup to entry+20. Fixtures
           must already equip the tyres admitted by the authored event. */
        {
            uint8_t expected_setup[0x178],actual_setup[0x178];
            CHECK(gt4_career_active_setup(owned,expected_setup));
            CHECK(gt4_race_run_setup(run,0,actual_setup));
            CHECK(!memcmp(expected_setup,actual_setup,sizeof expected_setup));
        }
        retained=event.desc;retained.event_row=NULL;retained.owned_car[0]=owned;
        unsigned configured=GT4_U16(gt4_race_run_conductor(run)+0xf850u);
        if(getenv("GT4_ENDURANCE_RULE_NEGATIVE"))configured^=1;
        CHECK(configured==(unsigned)gt4_career_race_field(event.race_row,GT4_RACE_LIMIT_TIME));
        gt4_phys_pad.read=series_pad;gt4_phys_pad.wheel_state=idle_wheel;gt4_phys_pad.write=idle_write;
        if(!controller_kind
          )CHECK(gt4_car_take_over(gt4_race_run_car(run,0),driver));
#ifdef GT4_SERIES_PACE_FIXTURE
        else fixture_controller_kind(gt4_race_run_car(run,0),(uint32_t)controller_kind);
#endif
        /* The native AI is a test controller, not a race-finish fixture. */
        uint64_t frames=0;int minutes=gt4_career_race_field(event.race_row,GT4_RACE_LIMIT_TIME);
        uint64_t timeout=60ULL*(minutes?((uint64_t)minutes*60+3600):60*1200);
        const char *trace=getenv("GT4_SERIES_TRACE_FRAME");
        if(trace)timeout=strtoull(trace,NULL,10); /* diagnostic fixture stop, never a finish */
        const char *interval=getenv("GT4_SERIES_PROGRESS_INTERVAL");
        uint64_t progress_interval=interval?strtoull(interval,NULL,10):216000;
        CHECK(progress_interval);
        const char *stall_limit=getenv("GT4_SERIES_STALL_FRAMES");
        uint64_t unchanged=0,stall_frames=stall_limit?strtoull(stall_limit,NULL,10):0;
        float previous_distance=GT4_F32(gt4_race_run_car(run,0)+0x6d4);
        while(!gt4_race_run_finished(run) && frames<timeout){
            /* The start event restores the human driver. Start this test
               controller after that event, through the original takeover
               API; never write finish/rank or change vehicle parameters. */
            if(!controller_kind
               && frames>600 && !GT4_U8(gt4_race_run_car(run,0)+CAR_56B))CHECK(gt4_car_take_over(gt4_race_run_car(run,0),driver));
            CHECK(gt4_race_run_frame(run));frames++;
            if(stall_frames){
                float distance=GT4_F32(gt4_race_run_car(run,0)+0x6d4);
                unchanged=distance==previous_distance?unchanged+1:0;previous_distance=distance;
                if(unchanged>=stall_frames){
                    fprintf(stderr,"controller stalled %s frame=%llu distance=%.3f; no result written\n",label,(unsigned long long)frames,distance);
                    gt4_race_run_close(run);return 1;
                }
            }
            if(frames==3600){gt4_race_hud h;CHECK(gt4_race_run_hud(run,0,&h));uint32_t p=gt4_race_run_car(run,0);printf("controller speed=%.2f gear=%d throttle=%.3f brake=%.3f distance=%.2f mode=%u target=%.2f how=%u\n",h.speed,h.gear,h.throttle,h.brake,GT4_F32(p+0x6d4),GT4_U8(p+CAR_56B),GT4_F32(p+0x14+0x28),GT4_U8(p+0x14+0x1e));fflush(stdout);CHECK(GT4_F32(p+0x6d4)>100);}
            if(frames%progress_interval==0){gt4_race_hud h;CHECK(gt4_race_run_hud(run,0,&h));uint32_t p=gt4_race_run_car(run,0);printf("progress %s frames=%llu lap=%d rank=%d fuel=%.1f speed=%.3f mode=%u how=%u distance=%.2f pits=%u\n",label,(unsigned long long)frames,h.lap,h.rank,h.fuel,h.speed,GT4_U8(p+CAR_56B),GT4_U8(p+0x14+0x1e),GT4_F32(p+0x6d4),GT4_U16(gt4_race_run_conductor(run)+0xd9e4u));fflush(stdout);}
        }
        if(trace){char path[256];snprintf(path,sizeof path,"%s/trace-eeMemory.bin",directory);FILE *f=fopen(path,"wb");CHECK(f && fwrite(gt4_phys_mem,1,0x02000000,f)==0x02000000);fclose(f);printf("trace %s car=%08x conductor=%08x frames=%llu\n",path,gt4_race_run_car(run,0),gt4_race_run_conductor(run),(unsigned long long)frames);fflush(stdout);}
        gt4_race_hud hud;CHECK(gt4_race_run_hud(run,0,&hud) && hud.finished);
        if(minutes)CHECK(GT4_U32(gt4_race_run_conductor(run)+0xf898u)/3u>=(unsigned)minutes*60000u);
        {uint32_t p=gt4_race_run_car(run,0);printf("finish diagnostic lap=%d flags=%02x/%02x fuel=%.2f wear=%.3f grip=%.3f\n",hud.lap,GT4_U8(p+0x5b5),GT4_U8(p+0x6c6),GT4_F32(p+0x5d4),GT4_F32(gt4_car_wheel(p,0)+0xac),GT4_F32(gt4_car_wheel(p,0)+0x50));fflush(stdout);}
        uint8_t order[6];int ranks[6];for(int i=0;i<event.desc.cars;i++){
            gt4_race_hud h;CHECK(gt4_race_run_hud(run,i,&h));ranks[i]=h.rank;int j=i;
            while(j && ranks[order[j-1]]>ranks[i]){order[j]=order[j-1];j--;}order[j]=(uint8_t)i;
        }
        int position=0;while(order[position])position++;
        uint32_t player=gt4_race_run_car(run,0),a=gt4_career_distance_integer(GT4_F32(player+0x6d4)-GT4_F32(player+0x7b0)),b=gt4_career_distance_integer(GT4_F32(player+0x7b0));
        uint8_t wear[32];memcpy(wear,gt4_ptr(player+CAR_ENGINE_WEAR),32);CHECK(menu_apply_car_wear(m,wear));
        int64_t balance=get(vm,game_object(m),"money").u.i;
        uint32_t prize=gt4_career_race_prize(event.race_row,position),award=0,grand=0;uint8_t best=15;
        if(championship){gt4_career_championship_points(points,order,event.desc.cars,1,(const int8_t *)gt4_game_race_lookup_data()->driver_points);
            int16_t units;memcpy(&units,event.race_row+0x2c,2);if(!getenv("GT4_SERIES_PHYSICAL_NEGATIVE"))CHECK(menu_apply_championship_result(m,label,series,1,round+1==count,position,hud.best_lap,0,hud.aspec_points,prize,points,units,&award,&grand));}
        else if(!getenv("GT4_SERIES_PHYSICAL_NEGATIVE"))CHECK(menu_apply_career_result(m,label,1,position,hud.best_lap,0,hud.aspec_points,prize,&best,&award));
        printf("physical %s car=%s frames=%llu rank=%d award=%u grand=%u pit_stops=%u\n",label,car_label,(unsigned long long)frames,position+1,award,grand,GT4_U16(gt4_race_run_conductor(run)+0xd9e4u));fflush(stdout);
        menu_quick_race q={0};q.entries=event.desc.cars;q.player=position;q.finished=1;q.prize=award;q.grand_prize=grand;q.championship=championship;q.session_number=round;q.final_session=championship && round+1==count;
        snprintf(q.race,sizeof q.race,"%s",label);snprintf(q.course,sizeof q.course,"%s",event.desc.course);memcpy(q.total_points,points,sizeof points);
        for(int slot=0;slot<q.entries;slot++){int i=order[slot];const char *code=gt4_specdb_label_for_id(m->car_labels,m->car_labels_size,event.desc.car[i]);CHECK(code);snprintf(q.grid[slot].car,sizeof q.grid[slot].car,"%s",code);uint8_t setup[0x178];CHECK(gt4_race_run_setup(run,i,setup));q.grid[slot].color=(int8_t)setup[0x10];}
        gt4_race_run_close(run);races++;wins+=position==0;
        menu_save_capture(m);menu *quick=menu_open(vol,championship?"quick-championship":"quick-event",msg,sizeof msg);CHECK(quick);menu_save_refresh(quick);menu_set_quick_race(&q);
        menu_quick_event_result(quick,1,award);CHECK(menu_enter_quick_start(quick));ticks(quick);
        ah_set_log(quick->vm,log_script,NULL);ah_val context=ah_objv(quick->context);
        CHECK(ah_call_member(quick->vm,quick->project,"makePrizeFast",0,NULL,&ret)>=0);
        while(get(quick->vm,quick->project,"rest_prize").u.i>0)CHECK(ah_call_member(quick->vm,quick->project,"onPrizeCheck",1,&context,&ret)>=0);
        if(grand){ah_obj *page=get(quick->vm,quick->project,"PrizeRoot").u.o;
            CHECK(ah_call_member(quick->vm,page,"goNext",1,&context,&ret)>=0);ticks(quick);
            CHECK(ah_truthy(get(quick->vm,page,"grand_prize_mode")));
            CHECK(get(quick->vm,quick->project,"rest_prize").u.i>0);
            CHECK(ah_call_member(quick->vm,quick->project,"makePrizeFast",0,NULL,&ret)>=0);
            while(get(quick->vm,quick->project,"rest_prize").u.i>0)CHECK(ah_call_member(quick->vm,quick->project,"onPrizeCheck",1,&context,&ret)>=0);
            CHECK(ah_call_member(quick->vm,page,"goNext",1,&context,&ret)>=0);ticks(quick);
        }
        int pending=1;CHECK(menu_commit_career_distances(m,quick,a,b,&pending));menu_close(quick);
        CHECK(get(vm,game_object(m),"money").u.i==balance+award+grand);
        CHECK(menu_enter_career_return(m));ticks(m);CHECK(!script_errors);
        menu_save_capture(m);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(m);
        uint8_t restored[0x4b8];CHECK(gt4_game_car_body(vm,get(vm,get(vm,game_object(m),"garage").u.o,"riding_car").u.o,restored));
        CHECK(!memcmp(restored+0x498,wear,sizeof wear));
    }
    CHECK(races);printf("series %s races=%u wins=%u directory=%s\n",series,races,wins,directory);fflush(stdout);
    if(!require_prize){puts("PASS complete native endurance, original duration/finish, pit stops, payment, wear and save/reload; race result not injected.");gt4_game_set_car_factory(NULL,NULL);gt4_game_set_stats_reader(NULL,NULL);gt4_game_set_auto_gears(NULL,NULL);gt4_career_vehicle_close(factory);gt4_save_shutdown();return 0;}
    ah_val pi=ah_int(prize_id),column;CHECK(ah_call_member(vm,present,"makeColumn",1,&pi,&column)>=0);
    /* Return eligibility has already consumed present_result and queued the
       original prize in current_list; do not manufacture another victory. */
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"PresentRoot"));
    ah_obj *entitled=get(vm,present,"current_list").u.o;int found=0;
    for(int i=0;i<ah_array_size(entitled);i++)found|=ah_num(ah_array_get(entitled,i))==prize_id;CHECK(found);
    /* PresentRoot L14466 queues get_car itself. ticks() has delivered it:
       another direct get_car call here would award a duplicate fixture. */
    CHECK(!script_errors);
    game=game_object(m);garage=get(vm,game,"garage").u.o;CHECK(ah_array_size(get(vm,garage,"cars").u.o)==owned_fixtures+1);
    ah_obj *prize_car=ah_array_get(get(vm,garage,"cars").u.o,owned_fixtures).u.o;
    const char *expected_label=ah_cstr(ah_array_get(get(vm,data,"content_carlist").u.o,prize_id));
    CHECK(!strcmp(ah_cstr(get(vm,prize_car,"car_code")),expected_label));
    uint8_t prize_unit[32]={0},prize_expected[0x4b8],prize_actual[0x4b8];
    CHECK(gt4_career_vehicle_create(factory,expected_label,(int32_t)ah_num(get(vm,prize_car,"color_index")),0,prize_unit,prize_expected));
    CHECK(gt4_game_car_body(vm,prize_car,prize_actual) && !memcmp(prize_actual,prize_expected,sizeof prize_actual));
    CHECK(ah_num(get(vm,get(vm,game,"statistics").u.o,"prize_cars"))==1);
    menu_save_capture(m);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(m);
    CHECK(ah_array_size(get(vm,get(vm,game_object(m),"garage").u.o,"cars").u.o)==owned_fixtures+1);
    CHECK(gt4_game_car_body(vm,ah_array_get(get(vm,get(vm,game_object(m),"garage").u.o,"cars").u.o,owned_fixtures).u.o,prize_actual) && !memcmp(prize_actual,prize_expected,sizeof prize_actual));
    puts("PASS complete physical series and authored car-prize delivery/save reload; no finish or position injection.");
    gt4_game_set_car_factory(NULL,NULL);gt4_game_set_stats_reader(NULL,NULL);gt4_game_set_auto_gears(NULL,NULL);gt4_career_vehicle_close(factory);gt4_save_shutdown();return 0;
}

#ifndef GT4_SERIES_EMBEDDED
int main(int argc,char **argv)
{
    CHECK(argc>=2);const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);
    CHECK(vol && gt4_game_load_disc(argv[1]));char msg[256];menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);
    int status=argc>4 && (!strcmp(argv[2],"--physical") || !strcmp(argv[2],"--endurance"))?physical(argv[1],vol,m,argv[3],argv[4],argc>5?atoi(argv[5]):1,!strcmp(argv[2],"--physical")):catalog(m);
    menu_close(m);gt4_vol_close(vol);return status;
}
#endif
