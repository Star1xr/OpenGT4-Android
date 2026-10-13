/* MCourseRecordUnit: the Time Trial's RankingRoot list (00433178 insert
   order, 00433470 count, 00433430 delete), kept across a save and reload. */
#include "../../src/core/menu.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"course ranking line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static const char *call_text(ah_vm *vm,ah_obj *unit,const char *name,int index){
    ah_val arg=ah_int(index),out;if(ah_call_member(vm,unit,name,1,&arg,&out)<0)return NULL;return ah_cstr(out);
}
static int rank_num(ah_vm *vm,ah_obj *unit){ah_val out;return ah_call_member(vm,unit,"getRankNum",0,NULL,&out)<0?-1:(int)ah_num(out);}
static int passcode_text(ah_vm *vm,ah_obj *unit,char code[33]){
    for(int part=0;part<4;part++){ah_val args[2]={ah_int(0),ah_int(part)},out;
        if(ah_call_member(vm,unit,"getPassCode",2,args,&out)<0 || !ah_cstr(out) || strlen(ah_cstr(out))!=8)return 0;
        memcpy(code+part*8,ah_cstr(out),8);
    }code[32]=0;return 1;
}
static ah_obj *get_unit(menu *m,const char *course){
    ah_vm *vm=m->vm;ah_val arg=ah_str(vm,course),out,record;
    if(!ah_get_member(vm,game_object(m),"course_record",&record) || ah_call_member(vm,ah_as_obj(record,AO_MODULE),"getUnit",1,&arg,&out)<0)return NULL;
    return ah_as_obj(out,AO_MODULE);
}
int main(int argc,char **argv)
{
    CHECK(argc>=3);gt4_save_init(argv[2]);const char *why;char error[256];
    gt4_vol *vol=gt4_vol_open(argv[1],&why);CHECK(vol);
    gt4_save_info info;if(gt4_save_info_get(1,&info)==0)CHECK(gt4_save_create(1,"Ranking"));else CHECK(gt4_save_load_slot(1));
    menu *m=menu_open(vol,"quick-tt",error,sizeof error);CHECK(m);menu_save_refresh(m);ah_vm *vm=m->vm;
    ah_obj *unit=get_unit(m,"tsukuba");CHECK(unit);
    for(int i=rank_num(vm,unit);i>0;i--){ah_val zero=ah_int(0),out;CHECK(ah_call_member(vm,unit,"deleteEntry",1,&zero,&out)>=0);}
    CHECK(rank_num(vm,unit)==0);
    CHECK(!get_unit(m,"no_such_course"));
    CHECK(!strcmp(call_text(vm,unit,"getBestTime",0),"\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f"));
    CHECK(menu_course_record_insert(m,"tsukuba","ka_01",70000));
    CHECK(menu_course_record_insert(m,"tsukuba","ka_01",65432));
    CHECK(menu_course_record_insert(m,"tsukuba","ka_01",70000));   /* an equal time goes after */
    CHECK(!menu_course_record_insert(m,"tsukuba","ka_01",COURSE_RECORD_EMPTY));
    unit=get_unit(m,"tsukuba");CHECK(unit && rank_num(vm,unit)==3);
    CHECK(!strcmp(call_text(vm,unit,"getBestTime",0),"1'05.432"));
    CHECK(!strcmp(call_text(vm,unit,"getBestTime",1),"1'10.000"));
    CHECK(!strcmp(call_text(vm,unit,"getBestTime",3),"\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f"));
    char passcode[33];CHECK(passcode_text(vm,unit,passcode));
    CHECK(strspn(passcode,"0123456789")==32);
    if(argc>3)passcode[0]=0;
    char name[96],date[32];const char *text=call_text(vm,unit,"getCarName",0);CHECK(text && *text);snprintf(name,sizeof name,"%s",text);
    text=call_text(vm,unit,"getDate",0);CHECK(text && strlen(text)>=8 && text[4]=='/');snprintf(date,sizeof date,"%s",text);
    for(int i=0;i<10;i++)menu_course_record_insert(m,"tsukuba","ka_01",90000+i);
    CHECK(rank_num(vm,unit)==10);
    CHECK(!strcmp(call_text(vm,unit,"getBestTime",9),"1'30.006"));   /* the slower ones dropped */
    CHECK(!menu_course_record_insert(m,"tsukuba","ka_01",99999));
    ah_val one=ah_int(1),out;CHECK(ah_call_member(vm,unit,"deleteEntry",1,&one,&out)>=0);
    CHECK(rank_num(vm,unit)==9 && !strcmp(call_text(vm,unit,"getBestTime",1),"1'10.000"));
    CHECK(gt4_save_store());menu_close(m);
    CHECK(gt4_save_load_slot(1));m=menu_open(vol,"quick-tt",error,sizeof error);CHECK(m);menu_save_refresh(m);vm=m->vm;
    unit=get_unit(m,"tsukuba");CHECK(unit && rank_num(vm,unit)==9);
    CHECK(!strcmp(call_text(vm,unit,"getBestTime",0),"1'05.432"));
    CHECK(!strcmp(call_text(vm,unit,"getCarName",0),name));
    char reloaded[33];CHECK(passcode_text(vm,unit,reloaded));CHECK(!strcmp(reloaded,passcode));
    printf("PASS ranking: 9 entries kept, best %s %s %s; order, equal time, ten-entry limit, delete, save round trip.\n",call_text(vm,unit,"getBestTime",0),name,date);
    menu_close(m);gt4_vol_close(vol);gt4_save_shutdown();return 0;
}
