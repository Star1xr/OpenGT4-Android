/* Final race payment -> same PrizeRoot reopened for the series bonus.
   Race finish is a fixture; amounts come from the original RACE row. */
#define GT4_SUNDAY_EMBEDDED 1
#include "sunday_playable.c"
static int settle(menu *q)
{
    ah_val ret,context=ah_objv(q->context);
    CHECK(ah_call_member(q->vm,q->project,"makePrizeFast",0,NULL,&ret)>=0);
    while(ah_num(get(q->vm,q->project,"rest_prize"))>0)CHECK(ah_call_member(q->vm,q->project,"onPrizeCheck",1,&context,&ret)>=0);
    return 0;
}
int main(int argc,char **argv)
{
    CHECK(argc==2);const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol && gt4_game_load_disc(argv[1]));
    char directory[180],msg[256];snprintf(directory,sizeof directory,"build/career-series/bonus-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
    gt4_save_init(directory);menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);menu_save_capture(m);CHECK(gt4_save_create(1,"Championship bonus"));load_race_tables(m);
    uint8_t row[151];CHECK(gt4_career_race_row(gt4_game_race_lookup_data(),m->race_labels,m->race_labels_size,m->race_db,m->race_db_size,"pr_tuning_0505",row));
    uint32_t prize=gt4_career_race_prize(row,0);int16_t units;memcpy(&units,row+0x2c,2);uint32_t bonus=(uint32_t)units*100u;
    CHECK(prize && bonus && prize!=bonus);
    menu_quick_race work={0};work.entries=6;work.player=0;work.finished=1;work.championship=1;work.final_session=1;work.session_number=4;work.prize=prize;work.grand_prize=bonus;work.total_points[0]=10;
    strcpy(work.race,"pr_tuning_0505");strcpy(work.course,"tsukuba");for(int i=0;i<6;i++)strcpy(work.grid[i].car,"ka_01");menu_set_quick_race(&work);
    menu *q=menu_open(vol,"quick-championship",msg,sizeof msg);CHECK(q);ah_set_log(q->vm,log_script,NULL);menu_save_refresh(q);
    int64_t balance=get(q->vm,game_object(q),"money").u.i;CHECK(menu_enter_quick_start(q));ticks(q);CHECK(!settle(q));
    CHECK(get(q->vm,game_object(q),"money").u.i==balance+prize);
    ah_obj *page=get(q->vm,q->project,"PrizeRoot").u.o;ah_val ret,context=ah_objv(q->context);
    CHECK(ah_call_member(q->vm,page,"goNext",1,&context,&ret)>=0);
    ticks(q);CHECK(ah_truthy(get(q->vm,page,"grand_prize_mode")));
    uint32_t amount=(uint32_t)ah_num(get(q->vm,page,"prize_amount"));
    if(getenv("GT4_CHAMPIONSHIP_BONUS_NEGATIVE"))amount^=1;
    CHECK(amount==bonus);
    CHECK(!settle(q));CHECK(get(q->vm,game_object(q),"money").u.i==balance+prize+bonus);
    CHECK(ah_call_member(q->vm,page,"goNext",1,&context,&ret)>=0);ticks(q);
    CHECK(!strcmp(ui_root_name(q->ui,menu_active_root(q)),"AfterRoot"));CHECK(!script_errors);
    ah_val quick;CHECK(nat_quick_work(q->vm,NULL,NULL,0,NULL,&quick)>=0);
    CHECK(!ah_num(get(q->vm,quick.u.o,"prize")) && !ah_num(get(q->vm,quick.u.o,"GrandPrize")));
    menu_save_capture(q);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(m);
    CHECK(get(m->vm,game_object(m),"money").u.i==balance+prize+bonus);
    printf("PASS authored final-stage prize %u + championship bonus %u, same-page reinitialization, consumption, AfterRoot and save/reload.\n",prize,bonus);
    menu_close(q);menu_close(m);gt4_save_shutdown();gt4_vol_close(vol);return 0;
}
