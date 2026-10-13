/* Run IsDirtRunnable (0012E498) and its original row unpacker (00448B48).
   Only VM value lifetime, label lookup and compressed table IO are boundaries.
   The host bridge is also exercised through its registered script method. */
#define main pregrid_standard_main
#include "pregrid_menu.c"
#undef main
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
static uint8_t *ids[2],*dbs[2];
static size_t id_sizes[2],db_sizes[2];
static int answer;
static void no_op(ee_ctx *c){(void)c;}
static void copy_value(ee_ctx *c){memset(c->ram+c->r[4].uw[0],0,4);}
static void string_value(ee_ctx *c){c->r[2].ud[0]=0x1800000;}
static void bool_value(ee_ctx *c){answer=c->r[5].uw[0];copy_value(c);}
static void label_key(ee_ctx *c,int table){uint32_t id;const char *s=(const char *)c->ram+c->r[4].uw[0];
    c->r[2].ud[0]=gt4_specdb_label(ids[table],id_sizes[table],s,&id)?((uint64_t)(table?0x26:0)<<32)|id:UINT64_MAX;}
static void generic_key(ee_ctx *c){label_key(c,0);}
static void enemy_key(ee_ctx *c){label_key(c,1);}
static void read_info(ee_ctx *c){unsigned table=c->r[4].uw[0];CHECK(table<2);
    c->r[2].ud[0]=gt4_specdb_row(dbs[table],db_sizes[table],c->r[5].uw[0],c->ram+c->r[6].uw[0],65)==65;}
static ee_fn boundary(uint32_t pc){switch(pc){
    case 0x312370:return copy_value;case 0x314ad8:return string_value;case 0x2fe278:return bool_value;
    case 0x312318:case 0x3285a8:case 0x3285f8:case 0x2fc870:return no_op;
    case 0x447bb8:return generic_key;case 0x447c28:return enemy_key;case 0x449298:return read_info;
    default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static int original(ee_ctx *c,const char *label){
    strcpy((char *)c->ram+0x1800000,label);memset(c->r,0,sizeof c->r);answer=-1;
    c->r[4].uw[0]=0x1810000;c->r[5].uw[0]=1;c->r[29].uw[0]=0x1ef0000;
    memset(c->ram+0x1810000,0,4);jmp_buf j;c->exit_jmp=&j;CHECK(!setjmp(j));lookup(0x12e498)(c);
    CHECK(answer==0 || answer==1);return answer;
}
static ah_obj *member(menu *m,ah_obj *o,const char *name){ah_val v;CHECK(ah_get_member(m->vm,o,name,&v));
    ah_obj *r=ah_as_obj(v,AO_MODULE);CHECK(r);return r;}
static int accepted,blocked;
static int color_selection(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)vm;(void)self;(void)data;(void)argc;(void)argv;accepted++;*ret=ah_nil();return 0;}
static int rejected(menu *m,const char *title,const char *text,int confirm,const char *ok,const char *cancel){
    (void)m;(void)title;(void)confirm;(void)ok;(void)cancel;CHECK(strstr(text,"dirt") && strstr(text,"snow"));blocked++;return 1;}
/* Execute the authored eligibility branch, stopping at the color screen or
   the original rejection dialog. Rendering and race physics are outside it. */
static void script_routes(menu *m,const char *denied,const char *rally){
    ah_obj *course=member(m,m->project,"CourseRoot"),*car=member(m,m->project,"CarRoot"),*game=member(m,m->project,"GameRoot");
    ah_obj *select=member(m,member(m,car,"CarScroll"),"SelectBox");
    printf("Script cases: ka_01, %s, rejected %s\n",rally,denied);
    ah_set_member(m->vm,car,"set_col_select",ah_native(m->vm,"set_col_select",color_selection,NULL));menu_set_dialog_presenter(m,rejected);
    course_capacity(m,"tsukuba");unsigned routes=0,surfaces[4]={0};const char *cars[]={"ka_01",rally,denied};
    for(uint32_t i=0;i<car_u32(m->course_labels+4);i++){
        uint32_t id=car_u32(m->course_labels+20+i*8);const char *label=gt4_specdb_label_for_id(m->course_labels,m->course_labels_size,id);CHECK(label);
        const char *type=gt4_arcade_course_type(arcade_of(m),label);int snow=!strcmp(type,"snow"),dirt=!strcmp(type,"dirt");if(!snow && !dirt)continue;
        surfaces[snow?3:2]++;
        printf("  %s (%s)\n",label,type);
        for(int mode=0;mode<2;mode++)for(int k=0;k<3;k++){
            ah_set_member(m->vm,course,"current_crs",ah_str(m->vm,label));ah_set_member(m->vm,car,"current_car",ah_str(m->vm,cars[k]));
            ah_set_member(m->vm,game,"current_game",ah_str(m->vm,mode?"TimeTrial":"SingleRace"));
            ah_val args[]={ah_objv(m->context),ah_nil(),ah_nil()},result;accepted=blocked=0;
            CHECK(ah_call_member(m->vm,course,"isDirt",0,NULL,&result)>0 && ah_truthy(result));
            CHECK(ah_call_member(m->vm,select,"onActivate",3,args,&result)>0);
            CHECK(accepted==(k!=2) && blocked==(k==2) && !errors);routes++;
        }
    }
    CHECK(surfaces[2] && surfaces[3]);printf("PASS %u original script routes on %u dirt / %u snow courses: road car, rally car, rejected car; SingleRace and TimeTrial.\n",routes,surfaces[2],surfaces[3]);
}
int main(int argc,char **argv){
    CHECK(argc==3 || argc==4);int negative=argc==4;const char *err;gt4_vol *vol=gt4_vol_open(argv[2],&err);CHECK(vol);
    const char *labels[]={"GENERIC_CAR.idi","ENEMY_CARS.idi"},*tables[]={"ARCADEINFO_NORMAL.dbt","ARCADEINFO_TUNED.dbt"};
    for(int i=0;i<2;i++){char path[128];snprintf(path,sizeof path,"/specdb/GT4_US2560/%s",labels[i]);ids[i]=gt4_vol_read(vol,path,&id_sizes[i]);CHECK(ids[i]);
        snprintf(path,sizeof path,"/specdb/GT4_US2560/%s",tables[i]);dbs[i]=gt4_vol_read(vol,path,&db_sizes[i]);CHECK(dbs[i]);}
    ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);c.lookup=lookup;c.hook=boundary;
    uint32_t handles[]={0,1};memcpy(c.ram+0x623800,handles,8);
    char msg[256];menu *m=menu_open(vol,"arcade",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);
    ah_obj *native=member(m,member(m,ah_root(m->vm),"main"),"menu");native=member(m,native,"MCarData");
    unsigned cases=0,allowed=0,denied=0;const char *rejected_car=NULL,*rally_car=NULL;
    for(int table=0;table<2;table++){
        uint32_t count=car_u32(ids[table]+4);
        for(uint32_t i=0;i<count;i++){
            uint32_t id=car_u32(ids[table]+20+i*8);const char *label=gt4_specdb_label_for_id(ids[table],id_sizes[table],id);CHECK(label);
            int expected=original(&c,label);ah_val arg=ah_str(m->vm,label),result;
            CHECK(ah_call_member(m->vm,native,"IsDirtRunnable",1,&arg,&result)>0);int got=ah_truthy(result);
            if(negative && !cases)got=!got;
            if(got!=expected)fprintf(stderr,"%s: native %d original %d\n",label,got,expected);
            CHECK(got==expected);CHECK(gt4_arcade_dirt_runnable(arcade_of(m),label)==expected);
            if(expected)allowed++;else denied++;cases++;
            if(!table){if(!expected && !rejected_car)rejected_car=label;if(expected && strstr(label,"rally") && !rally_car && !strstr(label,"_106"))rally_car=label;}
        }
    }
    CHECK(!original(&c,"unknown_car"));CHECK(!gt4_arcade_dirt_runnable(arcade_of(m),"unknown_car"));CHECK(allowed && denied && !errors);
    printf("PASS %u disc labels vs original IsDirtRunnable: %u allowed, %u denied; registered VM binding.\n",cases,allowed,denied);
    /* arcade.ad L1353 uses this authored pool, not the CLI preview's
       fixed Beetle/Midget lineup. It is not inferred from car names. */
    ah_val pool;CHECK(ah_get_member(m->vm,member(m,m->project,"EnemyData"),"dirt_enemy_carlist",&pool));
    ah_obj *list=ah_as_obj(pool,AO_ARRAY);CHECK(list);int beetle=0,ka=0,rally=0;
    for(int i=0;i<ah_array_size(list);i++){const char *label=ah_cstr(ah_array_get(list,i));CHECK(label);
        beetle|=!strcmp(label,"_1100_standard_type11_49");ka|=!strcmp(label,"ka_01");rally|=!strcmp(label,"rs200_rally_85");}
    CHECK(beetle && ka && rally);
    printf("Original dirt/snow opponent pool: %d candidates, including Beetle, Ka and RS200 Rally.\n",ah_array_size(list));
    CHECK(rejected_car && rally_car);
    if(getenv("GT4_DIRT_BINDING_NEGATIVE"))ah_remove_member(m->vm,native,"IsDirtRunnable");
    script_routes(m,rejected_car,rally_car);
    menu_close(m);gt4_vol_close(vol);for(int i=0;i<2;i++){free(ids[i]);free(dbs[i]);}ee_free(&c);return 0;
}
